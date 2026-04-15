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

from orkengine.core import fsm
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

  ##############################################################################
  # Public API
  ##############################################################################

  @property
  def is_idle(self):
    return self.fsm is None or self.fsm.currentState == self._fsm_idle

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
