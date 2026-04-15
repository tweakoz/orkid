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

from orkengine.core import fsm, vec4
from orkengine import lev2
from ork.app.application import ApplicationComponent

################################################################################

class MultiEcsSceneComponent(ApplicationComponent):
  """Currently owns only the fade-transition HFSM.

  The host app still owns everything the FSM's callbacks read or write
  (fade_node, _active, scene tables, _applyScenePbr, _syncNodeState);
  the component just holds the FsmData + FsmInstance and exposes
  begin_transition(tag) + tick(dt) + is_idle.

  The app installs the wiring after the fade_node exists by calling
  build_fsm(app, fade_duration). Subsequent migration steps will pull
  more state inward."""

  def __init__(self):
    super().__init__()
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

  ##############################################################################
  # Public API
  ##############################################################################

  @property
  def is_idle(self):
    return self.fsm is None or self.fsm.currentState == self._fsm_idle

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

  def build_fsm(self, app, fade_duration):
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
      dur = inst.vars.fade_duration
      t = min(inst.vars.t / dur, 1.0)
      a.fade_node.fadeAmount = t
      if inst.vars.t >= dur:
        inst.sendEvent("midpoint")

    def _on_enter_fade_in(inst):
      a = inst.vars.app
      a._active = inst.vars.target
      a._applyScenePbr(a._active)
      a._syncNodeState()
      inst.vars.t = 0.0
      a.fade_node.fadeAmount = 1.0

    def _on_update_fade_in(inst):
      inst.vars.t += inst.vars.dt
      a = inst.vars.app
      dur = inst.vars.fade_duration
      t = min(inst.vars.t / dur, 1.0)
      a.fade_node.fadeAmount = 1.0 - t
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
    While priming, ticks every runtime so each scene stages its
    entities. Once primed, ticks only the active runtime."""
    if self._primed:
      rt = self.runtimes.get(active_tag)
      if rt is not None:
        rt.update(updinfo)
    else:
      for rt in self.runtimes.values():
        if rt is not None:
          rt.update(updinfo)

  def gpu_tick_runtimes(self, ctx, active_tag):
    """Per-frame GPU tick. Same prime/active-only selection as
    tick_runtimes. Called from _onGpuUpdate."""
    if self._primed:
      rt = self.runtimes.get(active_tag)
      if rt is not None:
        rt.gpuUpdate(ctx)
    else:
      for rt in self.runtimes.values():
        if rt is not None:
          rt.gpuUpdate(ctx)

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
  # Skybox preload + pbr state application — step 4
  ##############################################################################

  def preload_skyboxes(self, paths):
    """Sync-preload every unique skybox asset path in `paths` via
    PbrCommon.requestRadianceMaps and stash the resulting handles in
    self.skybox_cache (strong refs for process lifetime). Idempotent
    — paths already in the cache are skipped."""
    for p in paths:
      if p not in self.skybox_cache:
        self.skybox_cache[p] = lev2.PbrCommon.requestRadianceMaps(p)
    print(f"[multi_ecs] preloaded skyboxes (sync): {len(self.skybox_cache)}")

  def apply_scene_pbr(self, skybox_path, skybox_intensity):
    """Apply a scene's pbr_common state to the shared scenegraph:
    RadianceMaps from the preloaded cache + skyboxLevel. Called at
    scene activation (fade midpoint) and every idle frame as a
    Gap-1 workaround against SGSData default clobbering."""
    pbc = self.scenegraph.pbr_common
    pbc.RadianceMaps = self.skybox_cache[skybox_path]
    pbc.skyboxLevel  = float(skybox_intensity)
