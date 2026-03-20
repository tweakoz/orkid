#!/usr/bin/env ork.python

################################################################################
# ECS State Hooks Test
# Registers callbacks on every Controller lifecycle hook and logs invocations.
# Usage: statehooks.py [-s scene.json]
################################################################################

import os, sys, argparse
from orkengine.core import vec2, vec3, vec4, VarMap, CrcStringProxy, lev2_pyexdir, logger
from orkengine import lev2
from orkengine import ecs
from ork.app.application import ComponentizedApplication
from ork.ecs import EcsRuntime

tokens = CrcStringProxy()
lev2_pyexdir.addToSysPath()
from lev2utils.primitives import createGridData

################################################################################

parser = argparse.ArgumentParser(description="ECS State Hooks Test")
parser.add_argument("--scene", "-s", type=str, help="Scene file to load (.json)")
args = parser.parse_args()

################################################################################

LOG = logger()
log_channel = LOG.configureChannel("HOOKS", vec3(0.3, 1.0, 0.6))

################################################################################

class StateHooksTest(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.runtime = EcsRuntime()
    self._playing = False
    self.createEzApp(
      name="StateHooksTest",
      fullscreen=False,
      pre_init_fns=[ecs.ecsInitCallback])

  ##############################################################################

  def _register_hooks(self, controller):
    """Register a logging callback on every lifecycle hook."""

    # Update-thread hooks
    for name in [
      "onUpdPreCompose",  "onUpdPostCompose",
      "onUpdPreLink",     "onUpdPostLink",
      "onUpdPreStage",    "onUpdPostStage",
      "onUpdPreActivate", "onUpdPostActivate",
      "onUpdPreDeactivate","onUpdPostDeactivate",
      "onUpdPreUnstage",  "onUpdPostUnstage",
    ]:
      hook_fn = getattr(controller, name)
      # capture name by value via default arg
      hook_fn(lambda sim, n=name: log_channel.log(f"[UPD] {n} fired"))

    # GPU-thread hooks
    for name in ["onGpuPostInit", "onGpuPostLink"]:
      hook_fn = getattr(controller, name)
      hook_fn(lambda sim, ctx, n=name: log_channel.log(f"[GPU] {n} fired"))

  ##############################################################################
  # UI Setup
  ##############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    lg.clearColorStd = vec4(0.08, 0.08, 0.1, 1)

    vp_item = lg.makeChild(
      fill=True, margin=0,
      uiclass=lev2.ui.SceneGraphViewport,
      args=["Viewport", vec4(0.08, 0.08, 0.1, 1)])
    self.sgv = vp_item.widget

  ##############################################################################
  # GPU Init
  ##############################################################################

  def _onGpuInit(self, ctx):
    self.runtime.setup_camera()
    self.grid_data = createGridData()

    self.sgv.cameraName = "spawncam"
    self.sgv.camera_evhandler = lambda ev: self.runtime.handle_camera_event(ev)
    self.sgv.forkDB()

    if args.scene and os.path.exists(args.scene):
      self.runtime.load_scene(args.scene)

    log_channel.log("=== Staging scene (edit mode) ===")
    self._stage_scene()
    log_channel.log("=== Stage complete — press SPACE to start simulation ===")

  ##############################################################################

  def _create_scenegraph_with_grid(self):
    self.runtime.create_scenegraph()
    grid_node = self.runtime.layer.createDrawableNodeFromData("grid", self.grid_data)
    grid_node.sortkey = 1
    grid_node.pickable = False

  def _stage_scene(self):
    self._create_scenegraph_with_grid()
    # Register hooks BEFORE staging so pre/post stage fires
    self.runtime.ensure_scenegraph_system()
    self.runtime.controller = ecs.Controller()
    self.runtime.controller.bindScene(self.runtime.scene_data)
    self._register_hooks(self.runtime.controller)
    self.runtime.controller.createSimulation(scenegraph=self.runtime.scenegraph)
    self.runtime.controller.stageSimulation()
    self.runtime._sys_ref = self.runtime.controller.findSystem("SceneGraphSystem")
    self.runtime._sys_handle = self.runtime.controller.findSystemHandle("SceneGraphSystem")
    self.runtime.bind_to_viewport(self.sgv)
    self._playing = False

  def _start_simulation(self):
    self._create_scenegraph_with_grid()
    self.runtime.ensure_scenegraph_system()
    self.runtime.controller = ecs.Controller()
    self.runtime.controller.bindScene(self.runtime.scene_data)
    self._register_hooks(self.runtime.controller)
    self.runtime.controller.createSimulation(scenegraph=self.runtime.scenegraph)
    log_channel.log("=== Starting simulation (active mode) ===")
    self.runtime.controller.startSimulation()
    self.runtime._sys_ref = self.runtime.controller.findSystem("SceneGraphSystem")
    self.runtime._sys_handle = self.runtime.controller.findSystemHandle("SceneGraphSystem")
    self.runtime.bind_to_viewport(self.sgv)
    self._playing = True

  ##############################################################################

  def _onCameraEvent(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed:
      kc = uievent.keycode
      if kc == 32:  # SPACE
        log_channel.log("=== SPACE pressed — restarting simulation ===")
        if self._playing:
          self.runtime.destroy_simulation()
        self._start_simulation()
        return lev2.ui.HandlerResult()
    return self.runtime.handle_camera_event(uievent)

  ##############################################################################

  def _onGpuUpdate(self, ctx):
    self.runtime.gpuUpdate(ctx)

  def _onUpdate(self, updinfo):
    if self.runtime.controller:
      self.runtime.update()
    self.sgv.setDirty()

################################################################################

app = StateHooksTest()
app.sgv.camera_evhandler = lambda ev: app._onCameraEvent(ev)
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
