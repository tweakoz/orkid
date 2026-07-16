#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
################################################################################
# ork.hypermesh.viewer.py — LIVE hypermesh asset viewer (equivalent to ork.terrain.viewer2.py).
#
#   ./ork.hypermesh.viewer.py            # list available hypermesh assets, then exit (no window)
#   ./ork.hypermesh.viewer.py RippleGrid # load + render that asset LIVE
#   ./ork.hypermesh.viewer.py Box
#
# Loads the named asset, materializes a persistent LIVE GraphInst, and each frame on the GPU thread
# (onGpuUpdate) calls asset.animate(t) (if defined) to drive its PLUGS, then live.recompute(ctx) —
# the GraphInst re-evaluates (host-writes runtime params from the plugs, re-dispatches) and the
# ComputeDrawable redraws the updated SoA channels. Assets without animate() render static-live.
################################################################################

import sys, os, time, glob, importlib, subprocess, tempfile, math
from orkengine.core import vec2, vec3, vec4, CrcStringProxy   # core before lev2
from orkengine import lev2
from ork.hypergraph.assets.hypermesh._resolve import (   # shared with _ork.hypermesh.validate.py
    assets_dir, list_asset_names, resolve_asset as _resolve_asset, RESULT_TOKEN,
    load_asset_from_path, class_from_module, ensure_asset_file, PATH_MODNAME)

tokens = CrcStringProxy()

# the offscreen validator that --watch spawns on a file change (run via its shebang).
_VALIDATOR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "_ork.hypermesh.validate.py")


def list_assets():
  print("available hypermesh assets:")
  for n in list_asset_names():
    print("  %s" % n)
  print("\nusage: ork.hypermesh.viewer.py <asset>")


def resolve_asset(name):   # CLI: list + exit on unknown (the reload path calls _resolve_asset, which raises)
  try:
    return _resolve_asset(name)
  except KeyError as e:
    print(str(e), "\n")
    list_assets()
    sys.exit(1)


################################################################################

def _build_app(asset_cls, *, asset_path, path_mode, asset_modname, val_argv, label,
               fullscreen=False, watch=False, ssaa=None, msaa=None):
  from ork.app.application import ComponentizedApplication
  from ork.app.std_scenegraph import StandardSceneGraphComponent
  from ork.hypergraph.dflow.hypermesh import make_drawable

  class HypermeshViewer(ComponentizedApplication):
    def __init__(self):
      super().__init__()
      self._watch        = watch
      self._asset_path   = asset_path        # the .py file to watch (mtime) + reload
      self._path_mode    = path_mode         # True: -i explicit path; False: searched package asset
      self._asset_modname = asset_modname    # module to importlib.reload (search mode)
      self._val_argv     = val_argv          # argv to spawn the offscreen validator subprocess
      self._label        = label
      self._valout_path = os.path.join(tempfile.gettempdir(), "ork.hypermesh.watch.%d.out" % os.getpid())
      # post-fx (modelviewer-style): [T] ACES exposure, [S] saturation, [G] gamma. SGC gpuInits these
      # post_nodes (before building the Scene) — chain order aces -> hsvg. [E] cycles the env map.
      self._aces = lev2.PostFxNodeACES()
      self._hsvg = lev2.PostFxNodeHSVG()
      self._satset = [0.0, 0.1, 0.2, 0.5, 0.75, 0.8, 1.0, 1.25, 1.5, 1.75, 2.0]
      self._gamset = [0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.4]
      self._expset = [0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 5.0]
      self._sati   = self._satset.index(0.8); self._gami = 0; self._expi = 0
      _eg = os.path.join(os.environ.get("OBT_STAGE", ""), "assetcache", "envmaps2", "*.xir")
      self._env_names = [os.path.splitext(os.path.basename(f))[0] for f in sorted(glob.glob(_eg))]
      self._env_paths = ["<ork_envmaps2>/%s.xir" % n for n in self._env_names]   # IBL/reflection source
      self._env_cache = {}; self._env_idx = -1
      self._turntable = False       # [R] slow turntable on the subject (1 turn / 8s) — gauge async-load niceness
      self._tt_t0 = 0.0             # absolutetime at which the current spin started
      self._tt_base = 0.0          # azimuth (radians) the spin started from
      self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(6, 5, 9), tgt=vec3(0, 0, 0), up=vec3(0, 1, 0), 
        grid_variant=None, 
        msaa=msaa,
        ssaa=ssaa, post_nodes=[self._aces, self._hsvg])
      # in --watch mode keep the window above the editor/terminal for the iterative edit loop.
      # MSAA is an APPINIT param (-> appinit._msaa_samples, the forward node's MSAA RtGroup), NOT a
      # scenegraph param like ssaa (which the SGC sets above). 0=off,1=2x,2=4x,3=8x,4=16x. Needed for
      # alpha-to-coverage foliage.
      self.createEzApp(name="OrkHypermeshViewer",
                       fullscreen=fullscreen,
                       ssaa=ssaa,
                       msaa=msaa,
                       enable_always_on_top=watch)

    def _onGpuInit(self, ctx):
      self._ctx      = ctx
      self._aces.exposure   = self._expset[self._expi]    # SGC already gpuInit'd these post nodes
      self._hsvg.hue        = 0.0
      self._hsvg.value      = 1.0
      self._hsvg.saturation = self._satset[self._sati]
      self._hsvg.gamma      = self._gamset[self._gami]
      self._asset    = asset_cls()                       # author the graph
      self._animated = bool(self._asset.is_animated)   # onUpdate / S.time / animated field (offset_vel)
                                                        # — the base-class property is the single source of truth
      self._live     = self._asset.materialize_live(ctx) # persistent GraphInst (compiled/allocated once)
      m = self._live.mesh
      print("hypermesh viewer: %s LIVE verts=%d corners=%d faces=%d%s"
            % (asset_cls.__name__, m.num_verts, m.num_corners, m.num_faces,
               " — animating" if self._animated else ""), flush=True)
      # MATERIAL CYCLE: the [M] key swaps the render material live. Each mode is (name, material_cls,
      # albedo, roughness, metallic): asset = the asset's own MATERIAL_CLASS (None -> Solid); white =
      # the plain Solid material, white + matte; mirror = white Solid, metallic 1.0 + roughness 0.0;
      # groups = GroupView (faces by selection-group bits); faces = TopoView (every face distinct).
      # albedo/roughness/metallic are ignored by group/face materials (their FS post sets the surface);
      # None -> the material's own default. [M] cycles all modes (incl. groups/faces). make_drawable
      # wires the face-id buffer if the material WANTS_FACE_ID + the __tags channel if it WANTS_TAGS;
      # the triangulator + per-frame in-frame hook (onPreRender) re-evaluate the graph when animated.
      from ork.hypergraph.assets.materials.hypermesh import GroupView, TopoView
      from ork.hypergraph.assets.materials.terrain.solid import Solid
      from ork.hypergraph.dflow.hypermesh import HmMaterial as _HmMat
      _white = vec3(1.0, 1.0, 1.0)
      # the asset's OWN materials become ONE "asset" [M] mode applied PER-GID (E.3): the gid-0/None
      # material is the main draw, gid>0 materials each get their own bucket draw (same mesh, per-gid
      # args slot). The inspect modes (white/mirror/...) apply a single material across ALL polys.
      # Each mode = (name, primary HmMaterial, {gid: HmMaterial}).
      try:    _asset_mats = list(self._asset.materials() or [])
      except Exception: _asset_mats = []
      _primary  = next((m for m in _asset_mats if m.gid in (None, 0)), None)
      _gid_mats = {int(m.gid): m for m in _asset_mats if m.gid not in (None, 0)}
      if _primary is None:   # asset declared 0 (or only gid>0) materials -> the viewer's own default
        _primary = _HmMat(type(self._asset).MATERIAL_CLASS, albedo=vec3(0.70, 0.74, 0.80), roughness=0.5)
      _asset_name = _primary.name + ("+%dgid" % len(_gid_mats) if _gid_mats else "")
      self._mat_modes = [(_asset_name, _primary, _gid_mats),
                         ("white",   _HmMat(Solid, albedo=_white, roughness=0.5, metallic=0.0), {}),
                         ("mirror",  _HmMat(Solid, albedo=_white, roughness=0.0, metallic=1.0), {}),
                         ("mirror2", _HmMat(Solid, albedo=_white, roughness=0.2, metallic=1.0), {}),
                         ("x3",      _HmMat(Solid, albedo=vec3(0.3, 0.7, 0.3), roughness=1.0, metallic=0.0), {}),
                         ("groups",  _HmMat(GroupView, roughness=0.9), {}),
                         ("faces",   _HmMat(TopoView,  roughness=0.8), {})]
      self._mat_idx   = 0                            # start on the asset's own material ([M] cycles modes)
      self.node       = None
      self._matctr    = 0
      self._wireframe  = False                       # [W] toggles a wireframe (polygon-edge LINE) overlay
      self._wire_color = vec3(0.0, 0.0, 0.0)         # definable wireframe color (default black)
      self._wire_bias  = 0.0001                      # clip-space depth bias toward viewer -> lines on top
      self._update_frozen = False                    # [SPACE] freezes the asset's onUpdate (hold a rotation/pose)
      self._apply_material(ctx)
      print("hypermesh viewer: [M] cycle material — asset / white / mirror / groups / faces", flush=True)
      print("hypermesh viewer: [W] toggle wireframe overlay  [SPACE] freeze/unfreeze onUpdate", flush=True)
      print("hypermesh viewer: [E] env map  [S] saturation  [G] gamma  [T] ACES exposure", flush=True)
      print("hypermesh viewer: [O] dump current mesh to /tmp/*.obj (verts/uvs/normals/faces)", flush=True)
      # on-screen HUD legend (same StringDrawableData HUD as ork.modelviewer.py)
      _hud_fonts = {0: "i24", 1: "i36", 2: "i48", 3: "i48", 4: "i48"}
      _ssaa = ssaa
      self._hud = lev2.StringDrawableData()
      self._hud.pos2D = vec2(10 * max(_ssaa, 1), 40+20 * max(_ssaa, 1))
      self._hud.color = vec4(0, 0, 0, 1)
      self._hud.font  = _hud_fonts.get(_ssaa, "i48")
      self._hud_node  = self.SGC.layer_fwd.createDrawableNodeFromData("hud_keys", self._hud)
      self._hud_node.sortkey = 2000
      # [P] perf-stats HUD (the HmPerf 5s window block, see lev2.hypermesh.perfStats)
      self._show_perf = True
      self._perf_hud  = lev2.StringDrawableData()
      self._perf_hud.anchor = vec2(0, 1)                 # bottom-left corner (block-height compensated)
      self._perf_hud.pos2D  = vec2(0,-192)
      self._perf_hud.color  = vec4(0, 0, 0, 1)
      self._perf_hud.font  = _hud_fonts.get(_ssaa, "i48")
      self._perf_node = self.SGC.layer_fwd.createDrawableNodeFromData("hud_perf", self._perf_hud)
      self._perf_node.sortkey = 2001
      self._update_hud()
      # --watch runtime state (the file-watch + validate-subprocess + live-reload loop rides on_iter)
      self._valproc      = None
      self._valout       = None
      self._val_started  = 0.0
      self._reload_pending   = False
      self._asset_reloading  = False
      self._spawn_cooldown_until = 0.0
      self._last_mtime   = 0.0
      if self._watch:
        try:
          self._last_mtime = os.path.getmtime(self._asset_path)
        except OSError:
          pass
        print("hypermesh viewer: --watch %s — edit + save to auto-validate + live-reload"
              % self._asset_path, flush=True)

    # (re)build the ComputeDrawable for the current material mode and swap it into the fwd layer. GPU
    # work inline on the event/main thread (the ork.modelviewer.py [M]-swap idiom). The live GraphInst
    # + its pooled mesh buffers are shared; only the material + render triangulator are rebuilt.
    def _apply_material(self, ctx):
      name, prim, gid_mats = self._mat_modes[self._mat_idx]   # primary HmMaterial + {gid: HmMaterial}
      if self.node is not None:
        self.SGC.layer_fwd.removeDrawableNode(self.node)
        self.node = None
      cdd, self._gmtl = make_drawable(
                                      self._live, ctx, animated=self._animated,
                                      material_cls=prim.material_cls, albedo=prim.albedo,
                                      roughness=prim.roughness, metallic=prim.metallic,   # None -> default
                                      vtx_displace=prim.vtx_displace,                  # VS animation (Wind) — SHARED
                                      gid_materials=(gid_mats or None),         # E.3: per-gid bucket draws
                                      wireframe=self._wireframe, wire_color=self._wire_color,
                                      wire_bias=self._wire_bias,
                                      instances=getattr(self._asset, "instances", None),  # asset opts into instancing
                                      cull=getattr(self._asset, "cull", False),           # E.4: asset opts into GPU frustum cull
                                      cull_bound=getattr(self._asset, "cull_bound", None)) # None -> auto (mesh-readback bound)
      # NB: a clock-driven displace (Wind reads Time) needs NO per-frame code here — the engine feeds
      # Time via the standard RCFD_TIME provider, declared once on the material at build. Viewer stays
      # general-purpose: no asset/displace-specific uniform names.
      self._sink_last = {}     # E.6/2.12 — fresh material: re-apply every MaterialParamSink value
      self._matctr += 1
      self.node = self.SGC.layer_fwd.createDrawableNodeFromData("hypermesh_%d" % self._matctr, cdd)
      print("hypermesh material: %s" % name, flush=True)
      if hasattr(self, "_hud"):
        self._update_hud()

    def _update_hud(self):
      mat  = self._mat_modes[self._mat_idx][0]
      env  = self._env_names[self._env_idx] if (0 <= self._env_idx < len(self._env_names)) else "default"
      exp  = self._expset[self._expi]
      self._hud.text = (
        "[M] Material: %s\n"      % mat +
        "[W] Wireframe: %s\n"     % ("on" if self._wireframe else "off") +
        ("[SPACE] pause: %s\n" % ("FROZEN (onUpdate)" if self._update_frozen else "running") if self._animated else "") +
        "[E] Envmap: %s\n"        % env +
        "[S] Saturation: %.1f\n"  % self._satset[self._sati] +
        "[G] Gamma: %.1f\n"       % self._gamset[self._gami] +
        "[T] ACES Exposure: %s\n"  % ("%.2f" % exp if exp > 0 else "OFF") +
        "[R] Turntable: %s\n"     % ("ON (1 turn/8s)" if self._turntable else "off") +
        "[P] Perf stats: %s"      % ("on" if self._show_perf else "off"))

    def _onUpdate(self, updinfo):
      self._last_abstime = updinfo.absolutetime   # for the [R] turntable toggle t0
      # animated does NOT imply a python onUpdate (S.time / offset_vel fields are C++-clock-driven);
      # only call it on assets that define one. [SPACE] freezes it -> the asset's plugs stop advancing.
      if self._animated and not self._update_frozen and hasattr(self._asset, "onUpdate"):
        self._asset.onUpdate(updinfo)                    # advancing, holding the current rotation/pose
      # E.6/2.12 — MaterialParamSink drain (python make_drawable path; parity with the C++
      # hm_drawable drain): apply each sink's pokeable value to the CURRENT material by param
      # name. bindParam-on-change only; the rebind contract makes it live in every cached
      # pipeline — including on a STATIC mesh (no geometry recompute involved).
      for snk in getattr(self._asset, "_param_sinks", []):
        v = snk.inputs.value.value
        key = snk.param_name
        if self._sink_last.get(key) != v:
          self._sink_last[key] = v
          self._gmtl.bindParam(key, float(v))
      if self._show_perf:
        txt = lev2.hypermesh.perfStats()
        self._perf_hud.text = txt if txt else "perf: collecting (first 5s window)..."
      if self._turntable:
        # 1 rotation / 8s about the subject; fixed elevation, orbit radius/height
        # taken from the initial eye. Overrides mouse orbit while active.
        ie = self.SGC.initial_eye
        radius = math.hypot(ie.x, ie.z)
        height = ie.y
        ang    = self._tt_base + 2.0 * math.pi * (updinfo.absolutetime - self._tt_t0) / 8.0
        eye    = vec3(radius * math.cos(ang), height, radius * math.sin(ang))
        self.SGC.uicam.lookAt(eye, self.SGC.initial_tgt, vec3(0, 1, 0))
        # sync uicam -> the cameralut camera the render reads (the mouse-orbit path
        # does exactly this after uiEventHandler; lookAt alone never reaches render)
        self.SGC.uicam.updateMatrices()
        self.SGC.camera.copyFrom(self.SGC.uicam.cameradata)
        self.SGC.SGVP.widget.setDirty()   # keep the dirty-driven render repainting for a static subject
      self.SGC.scenegraph.updateScene(self.SGC.cameralut)

    def _onUiEvent(self, uievent):
      if uievent.code == tokens.KEY_DOWN.hashed and uievent.keycode == ord(" "):
        self._update_frozen = not self._update_frozen               # freeze/unfreeze: the asset's onUpdate (plug
        lev2.hypermesh.set_clock_paused(self._update_frozen)        # pokes) AND the GLOBAL dflow clock (S.time
        print("hypermesh pause: %s" % ("FROZEN (onUpdate + clock)"  # etc.) — resume continues, no time jump
                                       if self._update_frozen else "running"), flush=True)
        return lev2.ui.HandlerResult()
      if uievent.code == tokens.KEY_DOWN.hashed and uievent.keycode == ord("P"):
        self._show_perf = not self._show_perf            # [P]: HmPerf stats HUD on/off
        if not self._show_perf:
          self._perf_hud.text = ""
        self._update_hud()
        return lev2.ui.HandlerResult()
      if uievent.code == tokens.KEY_DOWN.hashed and uievent.keycode == ord("M"):
        if not self._asset_reloading:                                # don't swap mid-reload
          self._mat_idx = (self._mat_idx + 1) % len(self._mat_modes) # cycle asset -> white -> mirror -> ...
          self._apply_material(self._ctx)
        return lev2.ui.HandlerResult()
      if uievent.code == tokens.KEY_DOWN.hashed and uievent.keycode == ord("W"):
        if not self._asset_reloading:                                # toggle the wireframe overlay (rebuild)
          self._wireframe = not self._wireframe
          self._apply_material(self._ctx)
          print("hypermesh wireframe: %s" % ("on" if self._wireframe else "off"), flush=True)
        return lev2.ui.HandlerResult()
      if uievent.code == tokens.KEY_DOWN.hashed and uievent.keycode == ord("E"):
        if self._env_paths:                                          # cycle the env map (IBL + reflections)
          self._env_idx = (self._env_idx + 1) % len(self._env_paths)
          p   = self._env_paths[self._env_idx]
          sky = self._env_cache.get(p) or lev2.PbrCommon.requestRadianceMapsAsync(p)
          self._env_cache[p] = sky
          self.SGC.pbr_common.RadianceMaps = sky
          print("hypermesh envmap: %s" % self._env_names[self._env_idx], flush=True)
        else:
          print("hypermesh envmap: none found in <staging>/assetcache/envmaps2/", flush=True)
        self._update_hud()
        return lev2.ui.HandlerResult()
      if uievent.code == tokens.KEY_DOWN.hashed and uievent.keycode == ord("S"):
        self._sati = (self._sati + 1) % len(self._satset)
        self._hsvg.saturation = self._satset[self._sati]
        self._update_hud()
        return lev2.ui.HandlerResult()
      if uievent.code == tokens.KEY_DOWN.hashed and uievent.keycode == ord("G"):
        self._gami = (self._gami + 1) % len(self._gamset)
        self._hsvg.gamma = self._gamset[self._gami]
        self._update_hud()
        return lev2.ui.HandlerResult()
      if uievent.code == tokens.KEY_DOWN.hashed and uievent.keycode == ord("T"):
        self._expi = (self._expi + 1) % len(self._expset)
        self._aces.exposure = self._expset[self._expi]
        self._update_hud()
        return lev2.ui.HandlerResult()
      if uievent.code == tokens.KEY_DOWN.hashed and uievent.keycode == ord("R"):
        self._turntable = not self._turntable
        if self._turntable:
          ie = self.SGC.initial_eye
          self._tt_base = math.atan2(ie.z, ie.x)   # start from the initial azimuth (no jump on the FIRST toggle)
          self._tt_t0   = getattr(self, "_last_abstime", 0.0)
        print("hypermesh turntable: %s (1 turn / 8s)" % ("ON" if self._turntable else "OFF"), flush=True)
        self._update_hud()
        return lev2.ui.HandlerResult()
      if uievent.code == tokens.KEY_DOWN.hashed and uievent.keycode == ord("O"):
        # [O] capture the CURRENT live mesh to CPU and dump a Wavefront .obj (verts/uvs/normals + n-gon
        # faces; binormals as `# b` comments) for inspection (winding, recomputed normals, topology).
        stem = os.path.splitext(os.path.basename(self._label))[0]
        path = os.path.join(tempfile.gettempdir(), "hypermesh_%s_%d.obj" % (stem, int(time.time())))
        nv = lev2.hypermesh.dump_obj(self._live.mesh, self._ctx, path)
        print("hypermesh viewer: [O] dumped %d verts -> %s" % (nv, path), flush=True)
        return lev2.ui.HandlerResult()
      self.SGC._onCameraUiEvent(uievent)
      return lev2.ui.HandlerResult()

    ##############################################
    # --watch: per-frame tick (rides mainThreadLoop(on_iter=...), main thread). Poll a running validator;
    # do a pending reload; else detect a debounced file change and spawn the validator subprocess.
    def _watch_tick(self):
      if not self._watch or not hasattr(self, "_ctx"):
        return False
      now = time.time()
      # (1) poll a running validator (kill it if it hangs, so a bad asset can't stall the watch loop)
      if self._valproc is not None:
        rc = self._valproc.poll()
        if rc is None and now - self._val_started > 30.0:
          print("hypermesh watch: validator exceeded 30s — killing (treated as failure)", flush=True)
          try: self._valproc.kill()
          except Exception: pass
          try: self._valproc.wait(timeout=2.0)
          except Exception: pass
          rc = self._valproc.returncode if self._valproc.returncode is not None else -1
        if rc is not None:
          ok = self._interpret_validation(rc)
          self._valproc = None
          self._reload_pending = ok
      # (2) perform a pending reload (here, on the main thread — materialize_live needs the GPU thread)
      if self._reload_pending and not self._asset_reloading:
        self._reload_pending = False
        self._do_reload(self._ctx)
      # (3) detect a file change (debounced); don't spawn while one is in flight
      if self._valproc is None and now >= self._spawn_cooldown_until:
        try:
          m = os.path.getmtime(self._asset_path)
        except OSError:
          m = self._last_mtime                                       # editor mid-write (atomic rename) — skip
        if m != self._last_mtime:
          self._last_mtime = m
          self._spawn_cooldown_until = now + 0.2                     # debounce multi-syscall saves
          self._valout  = open(self._valout_path, "w")               # capture child output to a FILE (no
          self._valproc = subprocess.Popen(self._val_argv,           # pipe-buffer deadlock)
                                           stdout=self._valout, stderr=subprocess.STDOUT)
          self._val_started = now
          print("hypermesh watch: change detected -> validating %s ..." % self._label, flush=True)
      return False

    def _interpret_validation(self, rc):
      # prefer the validator's stdout RESULT token (survives a teardown SIGABRT); fall back to returncode.
      try:
        self._valout.close()
      except Exception:
        pass
      out = ""
      try:
        with open(self._valout_path) as f:
          out = f.read()
      except Exception:
        pass
      tok = None
      for line in out.splitlines():
        if line.startswith(RESULT_TOKEN):
          tok = line[len(RESULT_TOKEN):].strip()
      if tok is not None:
        print("hypermesh watch: validation %s (rc=%d)%s"
              % (tok, rc, "" if tok == "PASS" else " — NOT reloading"), flush=True)
        return tok == "PASS"
      print("hypermesh watch: validator rc=%d, no result token — NOT reloading" % rc, flush=True)
      return False

    def _do_reload(self, ctx):
      # importlib.reload the asset module, re-instantiate, re-materialize, and rebuild the drawable for the
      # CURRENT material mode (reusing _apply_material). The old LiveHypermesh/GraphInst + pooled SSBOs free
      # by refcount. NOTE: reload picks up edits to the ASSET FILE only — not imported deps (selexpr, materials).
      self._asset_reloading = True
      try:
        if self.node is not None:
          self.SGC.layer_fwd.removeDrawableNode(self.node)
          self.node = None
        if self._path_mode:                                # -i: re-exec the file fresh
          asset_cls = load_asset_from_path(self._asset_path)
        else:                                              # search: reload the package module
          if self._asset_modname in sys.modules:
            importlib.reload(sys.modules[self._asset_modname])
          asset_cls = class_from_module(sys.modules[self._asset_modname])
        self._asset    = asset_cls()
        self._animated = bool(self._asset.is_animated)   # single source of truth (incl. S.time + animated fields)
        self._live     = None                              # drop old GraphInst/pool before re-materializing
        self._live     = self._asset.materialize_live(ctx)
        am = self._mat_modes[0]                            # the asset's MATERIAL_CLASS may have changed
        self._mat_modes[0] = ("asset", type(self._asset).MATERIAL_CLASS, am[2], am[3], am[4])
        self._apply_material(ctx)
        m = self._live.mesh
        print("hypermesh watch: RELOADED %s verts=%d faces=%d" % (self._label, m.num_verts, m.num_faces),
              flush=True)
      except Exception as e:
        import traceback; traceback.print_exc()
        print("hypermesh watch: reload FAILED in-process: %s (viewer left without a drawable)" % e, flush=True)
      finally:
        self._asset_reloading = False

  return HypermeshViewer()


################################################################################

if __name__ == "__main__":
  import argparse
  parser = argparse.ArgumentParser(description="LIVE hypermesh asset viewer")
  parser.add_argument("asset", nargs="?", help="hypermesh asset (filename stem in assets/hypermesh/)")
  parser.add_argument("-i", "--input", default=None,
                      help="explicit asset .py path (bypasses search; CREATED from a cube template if missing) "
                           "— e.g. -w -i /tmp/xxx.py to start + iterate on a new test asset")
  parser.add_argument("-f", "--fullscreen", action="store_true", default=False, help="fullscreen")
  parser.add_argument("-t", "--ssaa", type=int, default=0, help="supersample antialiasing (SSAA) factor; default 1")
  parser.add_argument("-m", "--msaa", type=int, default=2,
                      help="MSAA level (0=off,1=2x,2=4x,3=8x,4=16x); needed for alpha-to-coverage foliage")
  parser.add_argument("-w", "--watch", action="store_true", default=False,
                      help="watch the asset .py; on change, validate it offscreen in a subprocess and "
                           "live-reload only if validation passes (iterative edit workflow)")
  a = parser.parse_args()
  # watch-mode validation includes the MESHVET geometric tier (crossing self-intersection, buried faces,
  # collapse, fan-fold, VET expectations, golden baseline) — it needs an OBJ dump, so the validator argv
  # always requests one to a per-pid temp path. A vet FAIL overrides the result token -> reload is BLOCKED.
  _watch_obj = os.path.join(tempfile.gettempdir(), "ork.hypermesh.watch.%d.obj" % os.getpid())
  if a.input:                                            # -i: explicit path (create from template if missing)
    asset_path = os.path.abspath(a.input)
    if ensure_asset_file(asset_path):
      print("hypermesh viewer: created new asset from cube template -> %s" % asset_path)
    asset_cls = load_asset_from_path(asset_path)
    app = _build_app(asset_cls, asset_path=asset_path, path_mode=True, asset_modname=PATH_MODNAME,
                     val_argv=[_VALIDATOR, "-i", asset_path, "-o", _watch_obj], label=os.path.basename(asset_path),
                     fullscreen=a.fullscreen, watch=a.watch, ssaa=a.ssaa, msaa=a.msaa)
  elif a.asset:                                          # search mode (filename stem under assets/hypermesh/)
    asset_cls = resolve_asset(a.asset)
    app = _build_app(asset_cls, asset_path=os.path.join(assets_dir(), a.asset + ".py"), path_mode=False,
                     asset_modname="ork.hypergraph.assets.hypermesh." + a.asset,
                     val_argv=[_VALIDATOR, a.asset, "-o", _watch_obj], label=a.asset,
                     fullscreen=a.fullscreen, watch=a.watch, ssaa=a.ssaa, msaa=a.msaa)
  else:
    list_assets()       # nothing specified -> list + exit, no window
    sys.exit(0)
  if a.watch:
    app.ezapp.mainThreadLoop(on_iter=app._watch_tick)   # per-frame watch tick (complex.py idiom)
  else:
    app.ezapp.mainThreadLoop()
