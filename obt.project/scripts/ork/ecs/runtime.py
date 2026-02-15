################################################################################
# ECS Runtime — reusable simulation lifecycle
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

from orkengine.core import vec2, vec3, vec4, VarMap, CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine import ecs

tokens = CrcStringProxy()

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCameraX

class EcsRuntime:
  """Reusable ECS simulation runtime.

  Manages: scene_data, scenegraph, controller, camera, sys_ref.
  Designed for embedding into any app with a SceneGraphViewport.

  Minimal example (~40 lines)::

    #!/usr/bin/env ork.python
    import os, argparse
    from orkengine.core import vec4, CrcStringProxy, lev2_pyexdir
    from orkengine import lev2, ecs
    from ork.app.application import ComponentizedApplication
    from ork.ecs import EcsRuntime

    lev2_pyexdir.addToSysPath()

    parser = argparse.ArgumentParser()
    parser.add_argument("-s", type=str, required=True)
    args = parser.parse_args()

    class App(ComponentizedApplication):
      def __init__(self):
        super().__init__()
        self.runtime = EcsRuntime()
        self.createEzApp(name="ecsplay", fullscreen=True,
                         pre_init_fns=[ecs.ecsInitCallback])

      def _onUiInit(self):
        lg = self.ezapp.topLayoutGroup
        lg.margin = 0
        lg.clearColorStd = vec4(0.08, 0.08, 0.1, 1)
        self.sgv = lg.makeChild(fill=True, margin=0,
          uiclass=lev2.ui.SceneGraphViewport,
          args=["vp", vec4(0.08, 0.08, 0.1, 1)]).widget

      def _onGpuInit(self, ctx):
        self.runtime.setup_camera()
        self.sgv.camera_evhandler = lambda ev: self.runtime.handle_camera_event(ev)
        self.sgv.forkDB()
        self.runtime.load_scene(args.s)
        self.runtime.create_scenegraph()
        self.runtime.start_simulation()
        self.runtime.bind_to_viewport(self.sgv)

      def _onUpdate(self, updinfo):
        self.runtime.update()
        self.sgv.setDirty()

    app = App()
    app.ezapp.mainThreadLoop()
    app.ezapp.shutdown()
  """

  def __init__(self):
    self.scene_data = ecs.SceneData()
    self.scenegraph = None
    self.layer = None
    self.controller = None
    self._sys_ref = None
    self.cameralut = lev2.CameraDataLut()
    self.camera = None
    self.uicam = None

  def setup_camera(self, camname="spawncam", eye=None, tgt=None, up=None):
    """One-time camera setup. Call from _onGpuInit."""
    if eye is None:
      eye = vec3(8, 6, 8)
    if tgt is None:
      tgt = vec3(0, 0, 0)
    if up is None:
      up = vec3(0, 1, 0)
    self.camera, self.uicam = setupUiCameraX(
      cameralut=self.cameralut, camname=camname)
    self.uicam.lookAt(eye, tgt, up)
    self.uicam.updateMatrices()
    self.camera.copyFrom(self.uicam.cameradata)

  def load_scene(self, path):
    """Load scene_data from JSON file. Returns True on success."""
    from orkengine.core import Object
    import os
    if not os.path.exists(path):
      print(f"File not found: {path}")
      return False
    try:
      json_str = open(path).read()
      obj = Object.deserializeJson(json_str)
      if obj is not None:
        self.scene_data = obj
        return True
      else:
        print(f"Failed to deserialize: {path}")
        return False
    except Exception as e:
      print(f"Load failed: {e}")
      return False

  def ensure_scenegraph_system(self, params=None):
    """Ensure scene_data has a SceneGraphSystemData with defaults.

    params: optional dict of overrides for declareParams.
    """
    sgsys_data = None
    for s in self.scene_data.systemDatas:
      if s.className == "SceneGraphSystemData":
        sgsys_data = s
        break
    if sgsys_data is None:
      sgsys_data = self.scene_data.addSceneGraphSystem()
      sgsys_data.declareLayer("std_forward")
    defaults = {
      "preset": "ForwardPBR",
      "ssaa": 4,
      "SkyboxIntensity": 2.0,
      "DiffuseIntensity": 1.0,
      "SpecularIntensity": 1.0,
      "AmbientLight": vec3(0.15),
      "enable_skybox": False,
      "clearcolor": vec3(0.08, 0.08, 0.1),
    }
    if params:
      defaults.update(params)
    sgsys_data.declareParams(defaults)

  def create_scenegraph(self, enable_pick=False):
    """Create a fresh scenegraph + layer. Returns (sg, layer)."""
    sg_params = VarMap()
    sg_params.preset = "ForwardPBR"
    sg_params.ssaa = 4
    sg = lev2.scenegraph.Scene(sg_params)
    if enable_pick:
      sg.enablePickHud()
    layer = sg.createLayer("std_forward")
    self.scenegraph = sg
    self.layer = layer
    return sg, layer

  def start_simulation(self):
    """Create controller, bind scene, create+start simulation."""
    self.destroy_simulation()
    self.ensure_scenegraph_system()
    self.controller = ecs.Controller()
    self.controller.bindScene(self.scene_data)
    self.controller.createSimulation(scenegraph=self.scenegraph)
    self.controller.startSimulation()
    self._sys_ref = self.controller.findSystem("SceneGraphSystem")

  def stage_simulation(self):
    """Create controller, bind scene, create+stage simulation (edit mode)."""
    self.destroy_simulation()
    self.ensure_scenegraph_system()
    self.controller = ecs.Controller()
    self.controller.bindScene(self.scene_data)
    self.controller.createSimulation(scenegraph=self.scenegraph)
    self.controller.stageSimulation()
    self._sys_ref = self.controller.findSystem("SceneGraphSystem")

  def destroy_simulation(self):
    """Tear down current simulation."""
    if self.controller:
      try:
        self.controller.stopSimulation()
        self.controller.terminateSimulation()
      except:
        pass
      self.controller = None
      self._sys_ref = None

  def update(self):
    """Per-frame update: sync camera + tick simulation. Call from _onUpdate."""
    if self.controller and self._sys_ref:
      UIC = self.uicam.cameradata
      self.controller.systemNotify(self._sys_ref, tokens.UpdateCamera, {
        tokens.eye: UIC.eye,
        tokens.tgt: UIC.target,
        tokens.up: UIC.up,
        tokens.near: UIC.near,
        tokens.far: UIC.far,
        tokens.fovy: UIC.fovy
      })
    if self.controller:
      self.controller.updateSimulation()

  def bind_to_viewport(self, sgv):
    """Bind scenegraph + camera + render callback to a SceneGraphViewport."""
    sgv.scenegraph = self.scenegraph
    sgv.cameraName = "spawncam"
    controller = self.controller
    sgv.onPreRender = lambda ctx: controller.gpuRender(ctx) if controller else None

  def handle_camera_event(self, uievent):
    """Process camera events. Call from sgv.camera_evhandler."""
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()
