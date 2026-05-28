#!/usr/bin/env ork.python

################################################################################
# ork.particle.viewer.py — generic harness for HyperSyn particles DSL files.
#
# Hosts a user-authored ParticleSystem subclass inside a windowed renderer.
# The DSL file is pure content: a single ParticleSystem subclass declaring
# the graph in __init__ and (optionally) per-frame mutations in onUpdate.
# Everything else — app, scene, camera, drawable wrap, main loop — lives here.
#
# Application structure: ComponentizedApplication (ork.app.application).
# Same subsystem-startup/shutdown lifecycle as ork.modelviewer.py — Init
# and Link phases for app/gpu/update/audio, ordered template methods,
# proper teardown via _onGpuExit. See application.py for the canonical
# init/exit sequence diagram.
#
# Usage:
#   ork.particle.viewer.py elliptical                  (bare name; search-path resolve)
#   ork.particle.viewer.py elliptical.py               (bare name with extension)
#   ork.particle.viewer.py /full/path/to/file.py       (explicit path)
#   ork.particle.viewer.py elliptical --class MyClass  (multi-class file)
#   ork.particle.viewer.py elliptical --ssaa 4
#
# Search path: ORK_PARTICLES_SEARCH_PATH env var (colon-separated, PATH-style).
# If unset, defaults to <orkid_root>/ork.data/particles.
################################################################################

import argparse, glob, os, sys
from pathlib import Path

from orkengine.core import vec2, vec3, vec4, VarMap, CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import (ParticlesDrawableData,
                            PostFxNodeHSVG, PostFxNodeACES, PbrCommon,
                            StringDrawableData, ui,
                            particles_drawable_graphinst)

# lev2_pyexdir adds ork.lev2/examples/python/ so `lev2utils.*` resolves
lev2_pyexdir.addToSysPath()
from lev2utils.scenegraph import createSceneGraph
from lev2utils.cameras import setupUiCamera

from ork.app.application import ComponentizedApplication
from ork.dflow.particles import (
    ParticleSystem,                 # noqa: F401 — re-exported here for grep-ability
    resolve_dsl_file,
    load_dsl_class,
    list_dsl_files,
)

tokens = CrcStringProxy()


def parse_args():
  p = argparse.ArgumentParser(
    description="HyperSyn particles DSL viewer — host a ParticleSystem DSL file in a renderer window")
  p.add_argument("dsl_file", nargs="?",
                 help="DSL bare name (resolved via ORK_PARTICLES_SEARCH_PATH) or explicit path to a .py file")
  p.add_argument("--class", dest="class_name", default=None,
                 help="explicit ParticleSystem subclass to load (auto-find used if omitted)")
  p.add_argument("--ssaa", type=int, default=0, help="SSAA multiplier (0=off)")
  p.add_argument("-e", "--envmap", default="",
                 help="initial envmap (path or shortname under <assetcache>/envmaps2/; "
                      "empty = use cold4k or first available)")
  p.add_argument("--param", "-p", action="append", dest="params", default=[],
                 metavar="KEY=VALUE",
                 help="DSL constructor kwarg (repeatable). Values are parsed as "
                      "Python literals first (1.5, true, [1,2,3], {'k':1}); "
                      "literal-parse failures fall back to plain string. "
                      "Example: -p base_radius=1.5 -p pool_size=2000")
  p.add_argument("--list", "-l", action="store_true",
                 help="list all DSL files found in ORK_PARTICLES_SEARCH_PATH and exit")
  return p.parse_args()


def _parse_param(spec):
  """key=value → (key, parsed_value). Tries ast.literal_eval first; on
  SyntaxError/ValueError falls back to the raw string. Lets users pass
  numbers and lists naturally while still accepting bare strings."""
  import ast
  if "=" not in spec:
    raise ValueError(f"--param expects KEY=VALUE, got {spec!r}")
  key, raw = spec.split("=", 1)
  key = key.strip()
  try:
    value = ast.literal_eval(raw)
  except (ValueError, SyntaxError):
    value = raw
  return key, value


################################################################################
# ParticlesApp — ComponentizedApplication subclass.
#
# Lifecycle hooks we override:
#   __init__()      → register ezapp_args, build preset-cycle state,
#                     enumerate envmap files
#   createEzApp()   → inherited; we call it after configuring ezapp_args
#   _onGpuInit(ctx) → build sceneparams (incl. Post-FX chain), create scene
#                     via lev2utils.createSceneGraph, init lighting manager,
#                     instantiate the DSL ParticleSystem and attach its
#                     drawable, create HUD
#   _onUpdate(info) → forward to the DSL system, tick the scene
#   _onUiEvent(ev)  → keyboard cycling for envmap / sat / gamma / exposure;
#                     fall through to uicam handler for orbit
#   _onGpuExit(ctx) → drop GPU-side references in a defined order so the
#                     scene's resources tear down before lev2 exits
################################################################################

class ParticlesApp(ComponentizedApplication):
  """Particles DSL viewer.

  Key bindings (mirror ork.modelviewer.py):
    E = cycle envmap   S = cycle saturation   G = cycle gamma
    T = cycle ACES exposure                    R = reset post-fx
    SPACE = reset particle system (graphinst.reset())
  """

  def __init__(self, dsl_class, ssaa=0, envmap="", dsl_kwargs=None):
    super().__init__()
    self._dsl_class      = dsl_class
    self._dsl_kwargs     = dsl_kwargs or {}
    self._ssaa           = ssaa
    self._envmap_initial = envmap

    # Preset cycles — same lists/defaults as modelviewer so muscle memory
    # carries over (sat=0.8, gamma=0.8, exposure=OFF).
    self._satset = [0.0, 0.1, 0.2, 0.5, 0.75, 0.8, 1.0, 1.25, 1.5, 1.75, 2.0]
    self._gamset = [0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.4]
    self._expset = [0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 5.0]
    self._sat_idx = self._satset.index(0.8)
    self._gam_idx = self._gamset.index(0.8)
    self._exp_idx = 0

    # Envmap discovery (filesystem-driven; only files actually present here).
    stage_dir   = os.environ.get("OBT_STAGE", "")
    envmap_glob = os.path.join(stage_dir, "assetcache", "envmaps2", "*.xir")
    envmap_files = sorted(glob.glob(envmap_glob))
    self._envmap_names = [os.path.splitext(os.path.basename(f))[0] for f in envmap_files]
    self._envmap_paths = [f"<assetcache>/envmaps2/{n}.xir" for n in self._envmap_names]
    self._skybox_cache = dict()
    self._skybox_index = -1

    # ezapp_args is read by ComponentizedApplication.createEzApp().
    self.ezapp_args = {
      "ssaa":       ssaa,
      "fullscreen": True,
    }
    self.createEzApp()
    setupUiCamera(app=self, eye=vec3(0, 0, 30), constrainZ=True, up=vec3(0, 1, 0))

  ##############################################
  # GPU init — first chance with a live Context. Build the scene here.
  ##############################################

  def _onGpuInit(self, ctx):
    # Build sceneparams with initial skybox + Post-FX chain (HSVG + ACES)
    # so keyboard cycles can mutate sat/gamma/exposure live.
    sceneparams = VarMap()
    initial_path = self._resolve_initial_envmap()
    if initial_path:
      sceneparams.SkyboxTexPathStr = initial_path

    self._aces = PostFxNodeACES()
    self._aces.exposure = self._expset[self._exp_idx]
    self._aces.gpuInit(ctx, 8, 8)
    self._aces.addToSceneVars(sceneparams, "PostFxChain")

    self._hsvg = PostFxNodeHSVG()
    self._hsvg.hue        = 0.0
    self._hsvg.saturation = self._satset[self._sat_idx]
    self._hsvg.value      = 1.0
    self._hsvg.gamma      = self._gamset[self._gam_idx]
    self._hsvg.gpuInit(ctx, 8, 8)
    self._hsvg.addToSceneVars(sceneparams, "PostFxChain")

    # ForwardPBR rendermodel; sets self.scene + self.layer1.
    createSceneGraph(app=self, rendermodel="ForwardPBR", vars=sceneparams)
    self.scene.lightingmanager.gpuInit(ctx)
    self._pbr_common = self.scene.pbr_common

    # Instantiate the user's DSL particle system (it builds the graph +
    # any GPU materials it needs, using the loading context). Forward any
    # --param kwargs collected from the CLI; the DSL class's __init__
    # signature decides what it accepts.
    self._ptc_system   = self._dsl_class(**self._dsl_kwargs)
    drawable_data      = ParticlesDrawableData()
    drawable_data.graphdata = self._ptc_system.generatedflow()
    self._drawable     = drawable_data.createDrawable()
    self._particle_node = self.layer1.createDrawableNode("particle-node", self._drawable)
    self._particle_node.sortkey = 1
    self._particle_node.worldTransform.translation = vec3(0, 1, 0)
    # Pull the live graphinst out of the drawable so spacebar can reset
    # the particle system (clears emitter state, kills live particles,
    # resets any module-internal counters via onReset).
    self._ptc_graphinst = particles_drawable_graphinst(self._drawable)

    # HUD legend (StringDrawableData; font scales with SSAA since text
    # renders into the pre-resolve RT).
    hud_fonts = {0: "i24", 1: "i36", 2: "i48", 3: "i48", 4: "i48"}
    hud_scale = max(self._ssaa, 1)
    self._hud_drawable = StringDrawableData()
    self._hud_drawable.pos2D = vec2(10 * hud_scale, 30 + 20 * hud_scale)
    self._hud_drawable.color = vec4(0, 0, 0, 1)
    self._hud_drawable.font  = hud_fonts.get(self._ssaa, "i48")
    self._hud_node = self.layer1.createDrawableNodeFromData("hud_keys", self._hud_drawable)
    self._hud_node.sortkey = 2000
    self._update_hud()

  ##############################################
  # GPU exit — drop references in a defined order so scene resources
  # tear down before lev2 shuts down the context.
  ##############################################

  def _onGpuExit(self, ctx):
    self._hud_node       = None
    self._hud_drawable   = None
    self._particle_node  = None
    self._ptc_graphinst  = None
    self._drawable       = None
    self._ptc_system     = None
    self._hsvg           = None
    self._aces           = None
    self.scene           = None

  ##############################################

  def _resolve_initial_envmap(self):
    """Map --envmap arg → SkyboxTexPathStr string and seed _skybox_index
    so [E] cycling starts from the right spot. Returns "" if there is
    nothing reasonable to set (no envmaps on disk and user gave no path)."""
    arg = self._envmap_initial
    # Explicit path with placeholders or absolute — use as-is.
    if arg and ("<" in arg or os.path.isabs(arg)):
      return arg
    # Shortname → match against discovered names.
    if arg and arg in self._envmap_names:
      self._skybox_index = self._envmap_names.index(arg)
      return self._envmap_paths[self._skybox_index]
    # Empty → cold4k if available, else first.
    if "cold4k" in self._envmap_names:
      self._skybox_index = self._envmap_names.index("cold4k")
      return self._envmap_paths[self._skybox_index]
    if self._envmap_names:
      self._skybox_index = 0
      return self._envmap_paths[0]
    return ""

  ##############################################
  # Per-frame ticks (update thread).
  ##############################################

  def _onUpdate(self, updinfo):
    if self._shutting_down:
      return
    self._ptc_system.onUpdate(updinfo)
    self.scene.updateScene(self.cameralut)

  ##############################################
  # HUD + key handlers.
  ##############################################

  def _update_hud(self):
    sat = self._satset[self._sat_idx]
    gam = self._gamset[self._gam_idx]
    exp = self._expset[self._exp_idx]
    exp_str = f"{exp:.2f}" if exp > 0 else "OFF"
    if 0 <= self._skybox_index < len(self._envmap_names):
      sky_str = self._envmap_names[self._skybox_index]
    else:
      sky_str = "(default)"
    self._hud_drawable.text = (
      f"[E] Envmap: {sky_str}\n"
      f"[S] Saturation: {sat:.2f}\n"
      f"[G] Gamma: {gam:.2f}\n"
      f"[T] ACES Exposure: {exp_str}\n"
      f"[R] Reset post-fx\n"
      f"[SPACE] Reset particles"
    )

  def _cycle_envmap(self):
    if not self._envmap_paths:
      print("E: no envmaps found in <staging>/assetcache/envmaps2/")
      return
    self._skybox_index = (self._skybox_index + 1) % len(self._envmap_paths)
    path_str = self._envmap_paths[self._skybox_index]
    skybox = self._skybox_cache.get(path_str)
    if skybox is None:
      skybox = PbrCommon.requestRadianceMapsAsync(path_str)
      self._skybox_cache[path_str] = skybox
    self._pbr_common.RadianceMaps = skybox
    print("ENVMAP", self._envmap_names[self._skybox_index])

  def _reset_postfx(self):
    self._sat_idx = self._satset.index(0.8)
    self._gam_idx = self._gamset.index(0.8)
    self._exp_idx = 0
    self._hsvg.saturation = self._satset[self._sat_idx]
    self._hsvg.gamma      = self._gamset[self._gam_idx]
    self._aces.exposure   = self._expset[self._exp_idx]

  def _onUiEvent(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed:
      kc = uievent.keycode
      if kc == ord("E"):
        self._cycle_envmap();                                          self._update_hud(); return ui.HandlerResult()
      elif kc == ord("S"):
        self._sat_idx = (self._sat_idx + 1) % len(self._satset)
        self._hsvg.saturation = self._satset[self._sat_idx];           self._update_hud(); return ui.HandlerResult()
      elif kc == ord("G"):
        self._gam_idx = (self._gam_idx + 1) % len(self._gamset)
        self._hsvg.gamma = self._gamset[self._gam_idx];                self._update_hud(); return ui.HandlerResult()
      elif kc == ord("T"):
        self._exp_idx = (self._exp_idx + 1) % len(self._expset)
        self._aces.exposure = self._expset[self._exp_idx];             self._update_hud(); return ui.HandlerResult()
      elif kc == ord("R"):
        self._reset_postfx();                                          self._update_hud(); return ui.HandlerResult()
      elif kc == ord(" "):
        if self._ptc_graphinst is not None:
          self._ptc_graphinst.reset()
          print("particle system reset")
        return ui.HandlerResult()
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom(self.uicam.cameradata)
    return ui.HandlerResult()


def main():
  args = parse_args()
  if args.list or args.dsl_file is None:
    list_dsl_files()
    sys.exit(0)
  try:
    dsl_path  = resolve_dsl_file(args.dsl_file)
    dsl_class = load_dsl_class(dsl_path, args.class_name)
  except (FileNotFoundError, ValueError) as e:
    print(f"ptc viewer: {e}", file=sys.stderr)
    sys.exit(2)
  # Parse --param KEY=VALUE entries into a kwargs dict, surface bad input.
  try:
    dsl_kwargs = dict(_parse_param(s) for s in args.params)
  except ValueError as e:
    print(f"ptc viewer: {e}", file=sys.stderr)
    sys.exit(2)
  print(f"ptc viewer: hosting {dsl_class.__name__} from {dsl_path.name} ({dsl_path.parent})")
  if dsl_kwargs:
    print(f"ptc viewer:   with kwargs: {dsl_kwargs}")
  app = ParticlesApp(dsl_class, ssaa=args.ssaa, envmap=args.envmap, dsl_kwargs=dsl_kwargs)
  app.ezapp.mainThreadLoop()


if __name__ == "__main__":
  main()
