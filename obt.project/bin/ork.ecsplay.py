#!/usr/bin/env ork.python

################################################################################
# ECS Scene Player
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
#
# Key bindings (mirror ork.scene.viewer.py / ork.particle.viewer.py):
#   E = cycle envmap         S = cycle saturation
#   G = cycle gamma          T = cycle ACES exposure (tonemap)
#   R = reset post-fx
#   Command+Right Arrow = Start / Restart simulation
#   Command+Down  Arrow = Stop (back to staged idle)
################################################################################

import os, sys, argparse, glob
from orkengine.core import vec2, vec3, vec4, VarMap, CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine import ecs
from orkengine.lev2 import (PostFxNodeHSVG, PostFxNodeACES, PbrCommon,
                            StringDrawableData, ui)
from ork.app.application import ComponentizedApplication
from ork.hypergraph.ecs import EcsRuntime

tokens = CrcStringProxy()

lev2_pyexdir.addToSysPath()

################################################################################

parser = argparse.ArgumentParser(description="ECS Scene Player")
parser.add_argument("--scene", "-s", type=str, help="Scene file to play (.json)")
parser.add_argument("--fullscreen", "-f", action="store_true", help="Start in fullscreen mode")
parser.add_argument("-e", "--envmap", default="",
                    help="initial envmap (shortname in <assetcache>/envmaps2/, "
                         "or absolute / <bracketed> path; empty = scene default)")
parser.add_argument("--ssaa", type=int, default=1, help="SSAA multiplier (0=off)")
args = parser.parse_args()

################################################################################

class EcsPlayer(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.runtime = EcsRuntime()
    self._playing = False
    self._ssaa = args.ssaa

    # Preset cycles — same lists/defaults as scene.viewer / particle.viewer.
    self._satset  = [0.0, 0.1, 0.2, 0.5, 0.75, 0.8, 1.0, 1.25, 1.5, 1.75, 2.0]
    self._gamset  = [0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.4]
    self._expset  = [0.0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 5.0]
    self._sat_idx = self._satset.index(1.0)
    self._gam_idx = self._gamset.index(1.0)
    self._exp_idx = self._expset.index(1.0)

    # Envmap discovery — filesystem-driven (only files actually present).
    stage_dir   = os.environ.get("OBT_STAGE", "")
    envmap_glob = os.path.join(stage_dir, "assetcache", "envmaps2", "*.xir")
    envmap_files = sorted(glob.glob(envmap_glob))
    self._envmap_names = [os.path.splitext(os.path.basename(f))[0] for f in envmap_files]
    self._envmap_paths = [f"<assetcache>/envmaps2/{n}.xir" for n in self._envmap_names]
    self._skybox_cache = dict()
    self._skybox_index = -1

    # PostFx nodes — built once in _onGpuInit, re-added to each new
    # scene's sg_params on every stage / start.
    self._aces = None
    self._hsvg = None
    self._pbr_common   = None
    self._hud_drawable = None
    self._hud_node     = None

    self.createEzApp(
      name="OrkidEcsPlayer",
      fullscreen=args.fullscreen,
      enable_audio=True,
      enable_audio_output=True,
      enable_audio_synth=True,
      pre_init_fns=[ecs.ecsInitCallback])

  ##############################################################################
  # UI Setup
  ##############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    lg.clearColorStd = vec4(0.08, 0.08, 0.1, 1)

    # Fullscreen viewport
    vp_item = lg.makeChild(
      fill=True, margin=0,
      uiclass=lev2.ui.SceneGraphViewport,
      args=["Viewport", vec4(0.08, 0.08, 0.1, 1)])
    self.sgv = vp_item.widget

  ##############################################################################
  # GPU Init
  ##############################################################################

  def _onGpuInit(self, ctx):
    self._ctx = ctx
    self.runtime.setup_camera()
    self.runtime.uicam.distance = 1

    # Viewport bindings
    self.sgv.cameraName = "spawncam"
    self.sgv.camera_evhandler = lambda ev: self._on_viewport_event(ev)
    self.sgv.forkDB()

    # PostFx nodes — built once, re-added to each new scene's sg_params.
    self._aces = PostFxNodeACES()
    self._aces.exposure = self._expset[self._exp_idx]
    self._aces.gpuInit(ctx, 8, 8)
    self._hsvg = PostFxNodeHSVG()
    self._hsvg.hue        = 0.0
    self._hsvg.saturation = self._satset[self._sat_idx]
    self._hsvg.value      = 1.0
    self._hsvg.gamma      = self._gamset[self._gam_idx]
    self._hsvg.gpuInit(ctx, 8, 8)

    # Load scene if provided
    if args.scene and os.path.exists(args.scene):
      self.runtime.load_scene(args.scene)

    # Stage scene (same as editor idle — staged simulation)
    self._stage_scene()

    if args.envmap:
      self._apply_initial_envmap()

    print("ECS Player Ready — Command+Right Arrow to start simulation")

  ##############################################################################
  # Staged / Playing states
  ##############################################################################

  def _build_sg_params_with_postfx(self):
    """Pull defaults out of the loaded scene_data and splice in the
    PostFx chain. Authors don't manage post-fx; the player provides
    cycling controls and owns the nodes. Returns the augmented
    varmap, ready to hand to create_scenegraph(sg_params=...)."""
    sg_params = self.runtime.scene_data.generateSceneGraphParams()
    sg_params.ssaa = self._ssaa
    self._aces.addToSceneVars(sg_params, "PostFxChain")
    self._hsvg.addToSceneVars(sg_params, "PostFxChain")
    return sg_params

  def _post_create_scenegraph(self):
    """Cache pbr_common (for envmap cycling) + install HUD on the
    fresh scenegraph layer. Called after each stage / start."""
    self._pbr_common = self.runtime.scenegraph.pbr_common
    self._install_hud()

  def _stage_scene(self):
    """Stage the scene — entities created but not ticking (edit-like idle)."""
    self._hud_node = None
    self.runtime.create_scenegraph(sg_params=self._build_sg_params_with_postfx())
    self.runtime.stage_simulation()
    self.runtime.bind_to_viewport(self.sgv)
    self._post_create_scenegraph()
    self._playing = False

  def _start_simulation(self):
    """Start (or restart) the simulation."""
    self._hud_node = None
    self.runtime.create_scenegraph(sg_params=self._build_sg_params_with_postfx())
    self.runtime.start_simulation()
    self.runtime.bind_to_viewport(self.sgv)
    self._post_create_scenegraph()
    self._playing = True
    print("Simulation started (Command+Right Arrow to restart)")

  ##############################################################################
  # HUD + envmap / post-fx
  ##############################################################################

  def _install_hud(self):
    hud_fonts = {0: "i24", 1: "i36", 2: "i48", 3: "i48", 4: "i48"}
    hud_scale = max(self._ssaa, 1)
    self._hud_drawable = StringDrawableData()
    self._hud_drawable.pos2D = vec2(10 * hud_scale, 30 + 20 * hud_scale)
    self._hud_drawable.color = vec4(0, 0, 0, 1)
    self._hud_drawable.font  = hud_fonts.get(self._ssaa, "i48")
    # HUD on the overlay layer (declared by runtime.create_scenegraph)
    # so probe cubemap captures — which restrict to ProbeComponent's
    # renderLayer (default "std_forward") — don't bake the HUD text
    # into reflections.
    self._hud_node = self.runtime.hud_layer.createDrawableNodeFromData(
        "hud_keys", self._hud_drawable)
    self._hud_node.sortkey = 2000
    self._update_hud()

  def _update_hud(self):
    if not self._hud_drawable:
      return
    sat = self._satset[self._sat_idx]
    gam = self._gamset[self._gam_idx]
    exp = self._expset[self._exp_idx]
    exp_str = f"{exp:.2f}" if exp > 0 else "OFF"
    if 0 <= self._skybox_index < len(self._envmap_names):
      sky_str = self._envmap_names[self._skybox_index]
    else:
      sky_str = "(default)"
    play_str = "PLAYING" if self._playing else "STAGED"
    self._hud_drawable.text = (
      f"[E] Envmap: {sky_str}\n"
      f"[S] Saturation: {sat:.2f}\n"
      f"[G] Gamma: {gam:.2f}\n"
      f"[T] ACES Exposure: {exp_str}\n"
      f"[R] Reset post-fx\n"
      f"[Cmd+→] Start / [Cmd+↓] Stop ({play_str})"
    )

  def _apply_initial_envmap(self):
    arg = args.envmap
    path_str = None
    if arg and ("<" in arg or os.path.isabs(arg)):
      path_str = arg
    elif arg and arg in self._envmap_names:
      self._skybox_index = self._envmap_names.index(arg)
      path_str = self._envmap_paths[self._skybox_index]
    if path_str is None:
      return
    skybox = PbrCommon.requestRadianceMapsAsync(path_str)
    self._skybox_cache[path_str] = skybox
    self._pbr_common.RadianceMaps = skybox
    print("ENVMAP", arg)
    self._update_hud()

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
    self._sat_idx = self._satset.index(1.0)
    self._gam_idx = self._gamset.index(1.0)
    self._exp_idx = self._expset.index(1.0)
    self._hsvg.saturation = self._satset[self._sat_idx]
    self._hsvg.gamma      = self._gamset[self._gam_idx]
    self._aces.exposure   = self._expset[self._exp_idx]

  ##############################################################################
  # Viewport event handling
  ##############################################################################

  def _on_viewport_event(self, uievent):
    """Single viewport event hook — handle app-level keys first,
    then fall through to the runtime's camera handler."""
    if uievent.code == tokens.KEY_DOWN.hashed:
      kc = uievent.keycode
      # Command+arrows: simulation lifecycle
      if uievent.super:
        if kc == 262:  # Command+Right Arrow → Start/Restart
          self._start_simulation()
          self._update_hud(); return ui.HandlerResult()
        elif kc == 264:  # Command+Down Arrow → Stop (back to staged)
          if self._playing:
            self._stage_scene()
          self._update_hud(); return ui.HandlerResult()
      else:
        # Bare letter keys: post-fx + envmap cycle
        if kc == ord("E"):
          self._cycle_envmap();   self._update_hud(); return ui.HandlerResult()
        elif kc == ord("S"):
          self._sat_idx = (self._sat_idx + 1) % len(self._satset)
          self._hsvg.saturation = self._satset[self._sat_idx]
          self._update_hud(); return ui.HandlerResult()
        elif kc == ord("G"):
          self._gam_idx = (self._gam_idx + 1) % len(self._gamset)
          self._hsvg.gamma = self._gamset[self._gam_idx]
          self._update_hud(); return ui.HandlerResult()
        elif kc == ord("T"):
          self._exp_idx = (self._exp_idx + 1) % len(self._expset)
          self._aces.exposure = self._expset[self._exp_idx]
          self._update_hud(); return ui.HandlerResult()
        elif kc == ord("R"):
          self._reset_postfx()
          self._update_hud(); return ui.HandlerResult()
        elif kc == ord("U"):
          # PBR2 P3.D — toggle SSSS on/off live for A/B comparison.
          # Flag lives on pbr_common; PostFxNodeSSSS reads it per-frame.
          pbc = self.runtime.scenegraph.pbr_common
          pbc.enable_SSSS = not pbc.enable_SSSS
          print(f"[SSSS] enable_SSSS = {pbc.enable_SSSS}")
          self._update_hud(); return ui.HandlerResult()
    return self.runtime.handle_camera_event(uievent)

  ##############################################################################
  # Update loop
  ##############################################################################

  def _onGpuUpdate(self, ctx):
    self.runtime.gpuUpdate(ctx)

  def _onUpdate(self, updinfo):
    # Always update — staged mode needs camera sync, playing mode needs full tick
    if self.runtime.controller:
      self.runtime.update(updinfo)
    self.sgv.setDirty()

################################################################################

app = EcsPlayer()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
