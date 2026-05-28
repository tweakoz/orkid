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
    self._sys_handle = None
    self._sys_ref = None
    self.cameralut = lev2.CameraDataLut()
    self.camera = None
    self.uicam = None
    self.dead_controllers = []

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

  def load_scene(self, path, ezapp=None):
    """Load scene_data from JSON file. Returns True on success.

    After deserialize, walks the SceneData's AssetSystemData and
    re-materializes every gen (building real GPU artifacts), then
    patches any SceneGraphComponentData node that has a
    drawable_asset_name back to the matching live drawable. Without
    this step the deserialized RigidPrimitiveDrawableData placeholders
    have empty _primitive/_pipeline/_material and crash in
    DrawableCache::fetch on first stage.
    """
    from orkengine.core import Object
    import os
    if not os.path.exists(path):
      print(f"File not found: {path}")
      return False
    try:
      json_str = open(path).read()
      obj = Object.deserializeJson(json_str)
      if obj is None:
        print(f"Failed to deserialize: {path}")
        return False
      self.scene_data = obj
      # Materialize + post-wire. The loading-context fallback (no
      # explicit ctx) routes material gens through the lev2 loader
      # thread context, matching how live Scene.build()-time materials
      # are constructed.
      from ork.ecs.scene.assets import wire_scene_data
      # ezapp is needed for HdriToXirGenData bake (PBR2 Phase 0). Other
      # gen kinds ignore it — back-compat with callers passing nothing.
      wire_scene_data(self.scene_data, ezapp=ezapp)
      return True
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
      sgsys_data.declareLayer("std_transparent")
      sgsys_data.declareLayer("hud_overlay")
    defaults = {
      "preset": "ForwardPBR",
      "ssaa": 1,
      "SkyboxIntensity": 1.0,
      "DiffuseIntensity": 1.0,
      "SpecularIntensity": 1.0,
      "AmbientLight": vec3(0.0),
      "enable_skybox": True,
      "clearcolor": vec3(0.08, 0.08, 0.1),
    }
    if params:
      defaults.update(params)
    sgsys_data.declareParams(defaults)

  def create_scenegraph(self, enable_pick=False, sg_params=None):
    """Create a fresh scenegraph + layer. Returns (sg, layer).

    If sg_params is None, generates default params from scene_data.
    """
    if sg_params is None:
      sg_params = self.scene_data.generateSceneGraphParams()
    sg = lev2.scenegraph.Scene(sg_params)
    if enable_pick:
      sg.enablePickHud()
    layer = sg.createLayer("std_forward")
    # PBR2 Phase 2 — std_transparent for transmissive/refractive materials.
    # Drawn after std_forward (opaques) so the opaque framebuffer is
    # available for the P2.7 transmission lobe to sample. Empty when no
    # materials opt in.
    transparent_layer = sg.createLayer("std_transparent")
    # HUD/overlay layer — excluded from probe cubemap captures because
    # ProbeComponent's renderLayer defaults to "std_forward". The main
    # viewport's forward compositor renders all declared layers, so the
    # HUD remains visible in the primary frame.
    hud_layer = sg.createLayer("hud_overlay")
    self.scenegraph       = sg
    self.layer            = layer
    self.transparent_layer = transparent_layer
    self.hud_layer        = hud_layer
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
    self._sys_handle = self.controller.findSystemHandle("SceneGraphSystem")

  def stage_simulation(self):
    """Create controller, bind scene, create+stage simulation (edit mode)."""
    self.destroy_simulation()
    self.ensure_scenegraph_system()
    self.controller = ecs.Controller()
    self.controller.bindScene(self.scene_data)
    self.controller.createSimulation(scenegraph=self.scenegraph)
    self.controller.stageSimulation()
    self._sys_ref = self.controller.findSystem("SceneGraphSystem")
    self._sys_handle = self.controller.findSystemHandle("SceneGraphSystem")

  def destroy_simulation(self):
    """Tear down current simulation."""
    if self.controller:
      try:
        self.controller.stopSimulation()
        #self.controller.terminateSimulation()
      except:
        pass
      self.dead_controllers += [self.controller]
      self.controller = None
      self._sys_handle = None
      self._sys_ref = None

  def gpuUpdate(self, ctx):
    """Per-frame GPU update: tick gpuUpdate FSM. Call from _onGpuUpdate."""
    if self.controller:
      self.controller.gpuUpdate(ctx)

  def update(self,updinfo):
    """Per-frame update: sync camera + tick simulation. Call from _onUpdate."""
    if self.controller and self._sys_ref:
      if hasattr(self,"uicam") and self.uicam!=None:
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
    """Bind scenegraph + camera to a SceneGraphViewport."""
    sgv.scenegraph = self.scenegraph
    sgv.cameraName = "spawncam"

  def handle_camera_event(self, uievent):
    """Process camera events. Call from sgv.camera_evhandler."""
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()
