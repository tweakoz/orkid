################################################################################
# ork.app.multi_ecs — ApplicationComponent for hosting multiple ECS runtimes
# on a single shared lev2 scenegraph, with fade transitions between them.
#
# Baby-step construction: responsibilities are migrated out of the host
# app one at a time, testing between each step.
#
# Migration plan:
#   step 0 : empty stub                                                   done
#   step 1 : component owns the HFSM + fade tick loop                     [current]
#   step 2 : component owns the shared scenegraph + fade post-fx creation
#   step 3 : component owns the runtime pool (build/prime/active-tick)
#   step 4 : component owns skybox cache + pbr-state application
#   step 5 : component owns the node enable/disable loop with node filters
#   step 6 : component owns the camera rig (opt-out for external camera)
################################################################################

import json
import math
import re

from orkengine.core import fsm, vec3, vec4, lev2_pyexdir
from orkengine import lev2
from ork.app.application import ApplicationComponent

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCameraX

################################################################################
# CodedEcsScene — base class for a scene that manages lev2 scenegraph
# nodes directly (no ECS archetype). Subclasses implement the
# lifecycle methods; MultiEcsSceneComponent fires them based on
# registration + FSM-driven scene-activation state.
################################################################################

class CodedEcsScene:
  """Lifecycle:

    __init__                  Python object exists; no GPU yet.

    onGpuInit(ctx, component) Build materials / drawables / scenegraph
                              nodes / lights. Called once from the
                              app's _onGpuInit pipeline via
                              component.initCodedScenes(ctx).

    onActivate()              Called by the component's FSM at the
                              fade_in midpoint when this scene has
                              just become the active one.

    onDeactivate()            Called at the same fade_in midpoint
                              when this scene is losing active
                              status (fires before the active flip).

    onUpdate(updinfo)         Per-frame CPU tick. Fired by the
                              component's tick_runtimes only while
                              active_tag == self.tag.

    onGpuUpdate(ctx)          Per-frame GPU tick. Same gating.
  """

  # Universal default skybox. Subclasses can override by setting their
  # own class-attr `skybox_path`; scenes that load a `.ecs` file with a
  # declared skybox will overwrite this at __init__ time via the scan
  # below; YAML `skybox_path:` and runtime `setSkybox(...)` override
  # above both of those.
  skybox_path      = "ork_envmaps|tozenv_nebula"
  skybox_intensity = 1.0

  def __init__(self, tag, ecs_path=None):
    self.tag     = tag
    # Subclasses that build their own ECS runtime should assign it
    # here in onGpuInit. initCodedScenes harvests the result into
    # the component's runtime pool so the standard tick / priming /
    # node-visibility plumbing handles the scene identically to a
    # code-declared or file-loaded runtime.
    self.runtime = None
    # Optional per-scene camera rig. If left None, the scene uses
    # the component's shared rig. Scenes opt in by populating these
    # in onGpuInit (using component.cameralut as the SHARED LUT and
    # registering under a tag-specific camname, e.g. f"{self.tag}_cam").
    self.cameralut = None
    self.camera    = None
    self.uicam     = None
    # Optional per-scene skybox (asset path + intensity). Set by the
    # subclass via class attribute OR by assigning in __init__ after
    # super() (use self.setSkybox(path, intensity) for token expansion).
    # Hosts harvest these to preload skybox cache up front and to drive
    # apply_scene_pbr on scene activation.
    self.skybox_path      = self._expandSkyboxPath(
      getattr(type(self), "skybox_path", None))
    self.skybox_intensity = float(
      getattr(type(self), "skybox_intensity", 1.0))
    # Optional .ecs file path. Subclasses opt in by passing ecs_path to
    # super().__init__; subclass onGpuInit is responsible for the actual
    # runtime.load_scene(self._ecs_path) call plus any augmentations.
    # We do the path-token expansion + skybox scan here so every
    # .ecs-loading scene gets its declared skybox picked up uniformly.
    self._ecs_path = self._expandEcsPath(ecs_path)
    if self._ecs_path:
      sp, si = self._scan_ecs_skybox(self._ecs_path)
      if sp:
        self.setSkybox(sp, si)
    # Optional per-scene initial camera placement applied by the host
    # when this scene becomes active. `initial_camera_eye` is the world
    # position the viewer should occupy; `initial_camera_target` is the
    # look-at point used to derive facing. Either may be a 3-tuple/list
    # or a vec3. Both None → host falls back to its own config default.
    self.initial_camera_eye    = getattr(type(self), "initial_camera_eye",    None)
    self.initial_camera_target = getattr(type(self), "initial_camera_target", None)

  @staticmethod
  def _expandSkyboxPath(path):
    """Expand `<assetcache>/...` or `$VAR/...` tokens via obt.path so
    PbrCommon.requestRadianceMaps and sg_params.SkyboxTexPathStr both
    see a real filesystem path. Passes through None / plain strings."""
    if path and ("<" in path or "$" in path):
      from obt import path as _obt_path
      return str(_obt_path.Path(path).expanded)
    return path

  @staticmethod
  def _expandEcsPath(path):
    """Expand `<...>` / `$VAR` / `{VAR}` tokens in a .ecs file path so
    subclasses can pass YAML-style paths and rely on a filesystem-ready
    value in `self._ecs_path`. Mirrors _expandSkyboxPath's contract.
    Passes through None / plain strings."""
    if path and ("<" in path or "$" in path or "{" in path):
      from obt import path as _obt_path
      return str(_obt_path.Path(path).expanded)
    return path

  @staticmethod
  def _scan_ecs_skybox(ecs_path):
    """Walk a .ecs JSON for SceneGraphSystemData.userparams keys
    `SkyboxTexPathStr` + `SkyboxIntensity`. Returns
    (expanded_path_or_None, intensity_float). Machine-baked absolute
    paths containing '/assetcache/...' are rewritten to
    '<assetcache>/...' and then expanded via obt.path so the cache
    key matches what PbrCommon.requestRadianceMaps resolves on this
    machine. Returns (None, 1.0) when the file is unparseable, has
    no SGSData entry, or declares no skybox — callers skip preload
    and _applyScenePbr no-ops."""
    try:
      with open(ecs_path) as f:
        data = json.load(f)
    except (OSError, ValueError) as e:
      print(f"[CodedEcsScene] skybox scan failed for {ecs_path}: {e}")
      return None, 1.0

    found_path = [None]
    found_int  = [1.0]

    def walk(node):
      if isinstance(node, dict):
        up = node.get("userparams")
        if isinstance(up, dict):
          p = up.get("SkyboxTexPathStr")
          i = up.get("SkyboxIntensity")
          if isinstance(p, str) and p.startswith("string:"):
            found_path[0] = p[len("string:"):]
          if isinstance(i, str) and i.startswith("float:"):
            try:
              found_int[0] = float(i[len("float:"):])
            except ValueError:
              pass
        for v in node.values():
          walk(v)
      elif isinstance(node, list):
        for v in node:
          walk(v)

    walk(data)

    path = found_path[0]
    if not path:
      return None, found_int[0]
    m = re.search(r"/assetcache/(.+)$", path)
    if m:
      path = f"<assetcache>/{m.group(1)}"
    return CodedEcsScene._expandSkyboxPath(path), found_int[0]

  def setSkybox(self, path, intensity=1.0):
    """Override per-instance skybox after construction. Expands tokens
    in `path`. Useful for subclasses that differ only by asset (e.g.
    ModelScene instantiated N times with different paths)."""
    self.skybox_path      = self._expandSkyboxPath(path)
    self.skybox_intensity = float(intensity)
    print(f"[{self.tag}] setSkybox: path={self.skybox_path} intensity={self.skybox_intensity}")

  def onGpuInit(self, ctx, component):
    pass

  def onActivate(self):
    pass

  def onDeactivate(self):
    pass

  def onUpdate(self, updinfo):
    pass

  def onGpuUpdate(self, ctx):
    pass

  def onFadeTick(self, fadeAmount):
    pass

################################################################################

class MultiEcsSceneImpl:
  """Host-agnostic implementation of the multi-ECS-scene machinery.

  This class holds all the actual logic (FSM, runtime pool, scenegraph
  wiring, camera rig, skybox cache, node visibility, coded scene
  registry). It is NOT an ApplicationComponent — it can be embedded
  anywhere: as the state of `MultiEcsSceneComponent` (for plain
  ComponentizedApplication hosts, below), or directly inside a hydra
  client for hosted-ECS scenes.

  The app installs the wiring after the fade_node exists by calling
  buildFsm(app, fade_duration).

  Public attributes (read/write by callers):
    scenegraph, layer_fwd, fade_node    — shared render graph
    camera, uicam, cameralut            — shared camera rig
    runtimes        dict tag->EcsRuntime
    coded_scenes    dict tag->CodedEcsScene
    scene_nodes     dict tag->list[node]  (populated by sync_node_state)
    skybox_cache    dict path->RadianceMaps
    primed          bool property — True once every runtime has staged nodes
    is_idle         bool property — FSM state
  """

  def __init__(self):
    self.fsm              = None
    self._fsm_idle        = None
    self._fsm_fade_out    = None
    self._fsm_fade_in     = None
    self._fade_duration   = 0.5
    # Step 2 state
    self.scenegraph       = None
    self.layer_fwd        = None
    self.fade_node        = None
    # Step 3 state
    self.runtimes         = {}     # tag -> EcsRuntime (populated by app)
    self._primed          = False  # flips to True once every scene has nodes
    # Step 4 state
    self.skybox_cache     = {}     # full asset path -> RadianceMap handle
    # Step 5 state
    self.scene_nodes      = {}     # tag -> list[node] classified each frame
    # Step 6 state
    self.cameralut        = None
    self.camera           = None
    self.uicam            = None
    # Optional VR device handle. The host app publishes this after it
    # has constructed its VR device so scenes can resolve `observer_eye`
    # without knowing whether they are running under VR or not. None in
    # non-VR mode — `observer_eye` then falls through to `camera.eye`.
    self.vrdev            = None
    # Step 7 state — non-ECS scenes registered by the app
    self.coded_scenes   = {}     # tag -> CodedEcsScene instance

  ##############################################################################
  # Public API
  ##############################################################################

  @property
  def is_idle(self):
    return self.fsm is None or self.fsm.currentState == self._fsm_idle

  @property
  def observer_eye(self):
    """World-space viewer eye for scenes that need it (e.g. reflection
    math). Prefers the VR headset pose when `vrdev` is published and the
    pose is valid; otherwise returns the shared UI camera's eye. Returns
    vec3(0,0,0) if neither is available (exceptional — typically means
    the host hasn't finished setting up its camera rig yet)."""
    vrdev = self.vrdev
    if vrdev is not None:
      try:
        vp = vrdev.view_pos
        if not (math.isnan(vp.x) or math.isnan(vp.y) or math.isnan(vp.z)):
          return vp
      except Exception:
        pass
    cam = self.camera
    if cam is not None:
      try:
        return cam.eye
      except Exception:
        pass
    return vec3(0, 0, 0)

  def build_scenegraph(self, ctx, sg_params, fade_color=vec4(0, 0, 0, 1)):
    """Create the shared lev2 scenegraph from sg_params, installing
    a PostFxNodeFadeToColor in the PostFxChain BEFORE scene
    construction so the compositor picks it up. Creates the
    std_forward layer. Stores scenegraph / layer_fwd / fade_node on
    the component.

    The caller owns sg_params and its initial values (preset, skybox
    path, intensity, etc.) — the component just injects the fade
    post-fx and invokes Scene(sg_params).
    """
    self.fade_node            = lev2.PostFxNodeFadeToColor()
    self.fade_node.fadeColor  = fade_color
    self.fade_node.fadeAmount = 0.0
    self.fade_node.gpuInit(ctx, 8, 8)
    self.fade_node.addToSceneVars(sg_params, "PostFxChain")

    self.scenegraph = lev2.scenegraph.Scene(sg_params)
    self.layer_fwd  = self.scenegraph.createLayer("std_forward")
    # depth_prepass is auto-created by SceneGraphSystem._onStage when
    # any runtime stages; no need to create it here.

  def buildFsm(self, app, fade_duration):
    """Construct the HFSM and bind its callbacks to `app`. Called by
    the host app in _onGpuInit after it has created `app.fade_node`.
    Callbacks close over inst.vars.app to reach:
       app.fade_node
       app._active
       app._applyScenePbr(tag)
       app._syncNodeState()
    Per-instance state lives in inst.vars (t, dt, target)."""
    self._fade_duration = fade_duration

    data = fsm.FsmData()
    idle     = data.createState(None, "idle")
    fade_out = data.createState(None, "fade_out")
    fade_in  = data.createState(None, "fade_in")
    data.addTransition(idle,     "start",    fade_out)
    data.addTransition(fade_out, "midpoint", fade_in)
    data.addTransition(fade_in,  "complete", idle)

    def _on_enter_idle(inst):
      a = inst.vars.app
      a.fade_node.fadeAmount = 0.0
      if inst.vars.target is not None:
        print(f"[multi_ecs] transition done, active = {a._active}")
      inst.vars.target = None

    def _on_enter_fade_out(inst):
      inst.vars.t = 0.0
      a = inst.vars.app
      print(f"[multi_ecs] transition start {a._active} -> {inst.vars.target}")

    def _on_update_fade_out(inst):
      inst.vars.t += inst.vars.dt
      a = inst.vars.app
      comp = a.multiecs
      dur = inst.vars.fade_duration
      t = min(inst.vars.t / dur, 1.0)
      a.fade_node.fadeAmount = t
      # Fade outgoing scene's audio down with the visual fade
      outgoing = comp.coded_scenes.get(a._active)
      if outgoing is not None:
        outgoing.onFadeTick(t)
      if inst.vars.t >= dur:
        inst.sendEvent("midpoint")

    def _on_enter_fade_in(inst):
      a    = inst.vars.app
      comp = a.multiecs
      # Deactivate outgoing non-ECS scene (if any). Fires BEFORE the
      # active flip so the scene still reads its own tag as active
      # inside onDeactivate if it wants to.
      old = comp.coded_scenes.get(a._active)
      if old is not None:
        old.onDeactivate()
      # Flip active tag — single source of truth for "which scene
      # owns this frame".
      a._active = inst.vars.target
      a._applyScenePbr(a._active)
      a._syncNodeState()
      # Activate incoming non-ECS scene (if any).
      new = comp.coded_scenes.get(a._active)
      if new is not None:
        new.onActivate()
      # Rebind viewport to the active scene's camera if the scene
      # opted in to a per-scene rig. Shared-rig scenes stay on the
      # component's "spawncam" name. Single-lut invariant: every
      # camera lives in comp.cameralut, so this is a pure name flip.
      if hasattr(a, 'sgv') and a.sgv is not None:
        if new is not None and new.uicam is not None:
          a.sgv.cameraName = f"{new.tag}_cam"
        else:
          a.sgv.cameraName = "spawncam"
      inst.vars.t = 0.0
      a.fade_node.fadeAmount = 1.0

    def _on_update_fade_in(inst):
      inst.vars.t += inst.vars.dt
      a = inst.vars.app
      comp = a.multiecs
      dur = inst.vars.fade_duration
      t = min(inst.vars.t / dur, 1.0)
      a.fade_node.fadeAmount = 1.0 - t
      # Fade incoming scene's audio up with the visual fade
      incoming = comp.coded_scenes.get(a._active)
      if incoming is not None:
        incoming.onFadeTick(1.0 - t)
      if inst.vars.t >= dur:
        inst.sendEvent("complete")

    idle.onEnter      = _on_enter_idle
    fade_out.onEnter  = _on_enter_fade_out
    fade_out.onUpdate = _on_update_fade_out
    fade_in.onEnter   = _on_enter_fade_in
    fade_in.onUpdate  = _on_update_fade_in

    self._fsm_idle     = idle
    self._fsm_fade_out = fade_out
    self._fsm_fade_in  = fade_in

    self.fsm = fsm.FsmInstance(data)
    self.fsm.vars.app           = app
    self.fsm.vars.t             = 0.0
    self.fsm.vars.dt            = 0.0
    self.fsm.vars.target        = None
    self.fsm.vars.fade_duration = fade_duration
    self.fsm.changeState(idle)
    self.fsm.update()

  def begin_transition(self, target_tag):
    """Send 'start' event to the FSM with target_tag. No-op if not
    idle (mid-transition spacebar is ignored)."""
    if not self.is_idle:
      return
    self.fsm.vars.target = target_tag
    self.fsm.sendEvent("start")
    self.fsm.update()

  def tick(self, dt):
    """Feed frame deltatime into the FSM and step it once. Called by
    the host app each frame from _onUpdate."""
    if self.fsm is None:
      return
    self.fsm.vars.dt = dt
    self.fsm.update()

  ##############################################################################
  # Runtime pool — step 3
  ##############################################################################

  @property
  def primed(self):
    """True once every registered runtime has at least one drawable
    node on the shared scenegraph. Before that, tick_runtimes /
    gpu_tick_runtimes drive every runtime; after, only the active
    one. The app flips this via check_priming(counts)."""
    return self._primed

  def tick_runtimes(self, updinfo, active_tag):
    """Per-frame CPU tick. Called by the host app from _onUpdate.
    While priming, ticks every ECS runtime so each scene stages its
    entities. Once primed, ticks only the active runtime. Always
    fires the active CodedEcsScene's onUpdate (non-ECS scenes don't
    participate in priming — they exist as soon as onGpuInit runs)."""
    if self._primed:
      rt = self.runtimes.get(active_tag)
      if rt is not None:
        rt.update(updinfo)
    else:
      for rt in self.runtimes.values():
        if rt is not None:
          rt.update(updinfo)
    ne = self.coded_scenes.get(active_tag)
    if ne is not None:
      ne.onUpdate(updinfo)

  def gpu_tick_runtimes(self, ctx, active_tag):
    """Per-frame GPU tick. Same prime/active-only selection as
    tick_runtimes for ECS runtimes; always fires the active
    CodedEcsScene's onGpuUpdate. Called from _onGpuUpdate."""
    if self._primed:
      rt = self.runtimes.get(active_tag)
      if rt is not None:
        rt.gpuUpdate(ctx)
    else:
      for rt in self.runtimes.values():
        if rt is not None:
          rt.gpuUpdate(ctx)
    ne = self.coded_scenes.get(active_tag)
    if ne is not None:
      ne.onGpuUpdate(ctx)

  def check_priming(self, scene_node_counts):
    """Flip self._primed to True once every entry in
    scene_node_counts (dict tag->int) is positive. The host app
    supplies the counts after its per-frame node classification."""
    if self._primed:
      return
    if scene_node_counts and all(n > 0 for n in scene_node_counts.values()):
      self._primed = True
      pretty = " ".join(f"{t}={n}" for t, n in scene_node_counts.items())
      print(f"[multi_ecs] primed — switching to active-only tick ({pretty})")

  ##############################################################################
  # Coded scene registry — step 7
  ##############################################################################

  def registerCodedScene(self, scene):
    """Register a CodedEcsScene instance. Call from the app's __init__
    (before createEzApp) so the scene is present when _onGpuInit
    fires. The scene's onGpuInit runs later when the app calls
    initCodedScenes(ctx)."""
    self.coded_scenes[scene.tag] = scene

  def initCodedScenes(self, ctx):
    """Run onGpuInit on every registered CodedEcsScene. The app should
    call this from its _onGpuInit AFTER build_scenegraph +
    setup_camera but BEFORE buildFsm, so (a) the scenegraph / camera
    are valid when scene builders need them and (b) the FSM has a
    valid CodedEcsScene set to transition to / from.

    After each scene's onGpuInit runs, if it has populated
    `scene.runtime` we harvest it into `self.runtimes[scene.tag]` so
    the standard tick_runtimes / check_priming / sync_node_state path
    handles it identically to a code-declared runtime. Scenes that
    stay pure non-ECS (no runtime) get a None slot."""
    for scene in self.coded_scenes.values():
      scene.onGpuInit(ctx, self)
      self.runtimes[scene.tag] = scene.runtime

  ##############################################################################
  # Skybox preload + pbr state application — step 4
  ##############################################################################

  def preload_skyboxes(self, ctx, paths):
    """Synchronously preload every unique skybox asset path in `paths`
    and stash the resulting handles in self.skybox_cache (strong refs
    for process lifetime). Idempotent — paths already in the cache are
    skipped. Uses PbrCommon.requestRadianceMapsSync which blocks until
    the asset's LoadRequest partial-load counter hits zero (i.e. every
    deferred GPU upload for the radiance maps has run), so every entry
    in skybox_cache is guaranteed GPU-resident on return. After this,
    apply_scene_pbr is a pure pointer swap that takes effect on the
    next frame without any upload-in-flight race."""
    for p in paths:
      if p not in self.skybox_cache:
        self.skybox_cache[p] = lev2.PbrCommon.requestRadianceMapsSync(p, ctx)
    print(f"[multi_ecs] preloaded skyboxes (sync, resident): {len(self.skybox_cache)}")

  def apply_scene_pbr(self, skybox_path, skybox_intensity):
    """Apply a scene's pbr_common state to the shared scenegraph:
    RadianceMaps from the preloaded cache + skyboxLevel. Called at
    scene activation (fade midpoint) and every idle frame as a
    Gap-1 workaround against SGSData default clobbering."""
    pbc = self.scenegraph.pbr_common
    pbc.RadianceMaps = self.skybox_cache[skybox_path]
    pbc.skyboxLevel  = float(skybox_intensity)

  ##############################################################################
  # Node enable/disable (scene visibility toggle) — step 5
  ##############################################################################

  def sync_node_state(self, active_tag, filters, catchall_tag=None):
    """Walk every drawable node in the shared scenegraph, classify
    each by calling the caller-supplied filters, and apply
    node.enabled = (tag == active_tag) to the result.

    filters       : dict { tag -> predicate(node) -> bool }
    catchall_tag  : optional tag that claims any node no filter matched
                    (used e.g. for SceneFromFile loaded scenes whose
                    node names we don't control)

    Per-tag node lists are stashed on self.scene_nodes so the caller
    can use their counts for check_priming(). Entity spawns are
    async — node lists rebuild each call for self-healing."""
    tags = list(filters.keys())
    if catchall_tag is not None and catchall_tag not in tags:
      tags.append(catchall_tag)
    self.scene_nodes = { t: [] for t in tags }

    def _classify(node):
      for tag, fn in filters.items():
        if fn(node):
          self.scene_nodes[tag].append(node)
          return
      if catchall_tag is not None:
        self.scene_nodes[catchall_tag].append(node)

    for layer_name, layer in self.scenegraph.layers.items():
      for node in layer.drawable_nodes:
        _classify(node)
    # Light nodes live in a separate per-layer collection from drawable
    # nodes; the scenegraph exposes them flattened via lightNodes(). Run
    # them through the same tag filters so multiple scenes sharing one
    # scenegraph don't bleed lights across transitions.
    for lnode in self.scenegraph.lightNodes():
      _classify(lnode)

    # Gate the log: only emit on transitions so we get a sparse history
    # instead of per-frame spam. Track last active_tag on self.
    changed = getattr(self, "_last_sync_active_tag", "<unset>") != active_tag
    if changed:
      self._last_sync_active_tag = active_tag
    for tag in tags:
      want = (active_tag == tag)
      for n in self.scene_nodes[tag]:
        n.enabled = want
        if changed:
          print(f"[SGS sync_node_state] tag={tag} active={active_tag} "
                f"enabled={want} name={n.name} repr={n!r}")

  ##############################################################################
  # Camera rig — step 6
  ##############################################################################

  def setup_camera(self, eye=vec3(0, 2, 8), tgt=vec3(0, 0, 0),
                   up=vec3(0, 1, 0), camname="spawncam"):
    """Create the cameralut / camera / uicam rig and perform the
    initial lookAt. Stores all three on the component for the app
    and each EcsRuntime to share. Does NOT touch the viewport — the
    host app is responsible for binding its SceneGraphViewport to
    the cameraName and routing UI events via handle_camera_event.

    If the host has pre-populated self.cameralut (e.g. the hydra
    embedding adopts the hydra app's rig before calling setup_camera),
    the fresh-create branch is skipped and only the initial lookAt
    is re-applied on the adopted rig."""
    if self.cameralut is None:
      self.cameralut = lev2.CameraDataLut()
      self.camera, self.uicam = setupUiCameraX(
        cameralut=self.cameralut, camname=camname)
    if hasattr(self,"uicam") and self.uicam!=None:
      self.uicam.lookAt(eye, tgt, up)
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)

  def _activeRig(self):
    """Return (cameralut, camera, uicam) tuple for the active scene,
    falling back to the shared component rig if the active scene
    didn't opt in to its own rig."""
    scene = None
    if self.fsm is not None:
      scene = self.coded_scenes.get(self.fsm.vars.app._active) \
              if hasattr(self.fsm.vars, 'app') else None
    if scene is not None and scene.uicam is not None:
      return scene.cameralut, scene.camera, scene.uicam
    return self.cameralut, self.camera, self.uicam

  def handle_camera_event(self, uievent):
    """Dispatch a UI event to the active rig's uicam and copy the
    updated cameradata back to its main camera. Returns True if the
    uicam consumed the event, False otherwise."""
    _, cam, uicam = self._activeRig()
    if uicam is None:
      return False
    handled = uicam.uiEventHandler(uievent)
    if handled:
      uicam.updateMatrices()
      cam.copyFrom(uicam.cameradata)
    return bool(handled)


################################################################################
# MultiEcsSceneComponent — ApplicationComponent wrapper over MultiEcsSceneImpl.
#
# Callers that host inside a ComponentizedApplication (e.g. standalone_ocean*,
# multiscene.py) register this via `app.addComponent("multiecs",
# MultiEcsSceneComponent)`. API is 100% inherited from MultiEcsSceneImpl —
# this subclass exists only so the object also satisfies the
# ApplicationComponent base-class contract for component registration.
#
# Hydra clients that want the same machinery without the ApplicationComponent
# baggage should import and instantiate `MultiEcsSceneImpl` directly.
################################################################################

class MultiEcsSceneComponent(MultiEcsSceneImpl, ApplicationComponent):
  def __init__(self):
    ApplicationComponent.__init__(self)
    MultiEcsSceneImpl.__init__(self)
