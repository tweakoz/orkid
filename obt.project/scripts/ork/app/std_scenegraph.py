import sys, math
from ork import path as ork_path
from ork.app.application import ApplicationComponent
from orkengine.core import vec3, vec4, quat, VarMap, lev2_pyexdir
from orkengine import lev2 
sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path
lev2_pyexdir.addToSysPath()
from cameras import setupUiCameraX
from primitives import createGridData

#from orkengine.core import *
#from orkengine.lev2 import *

###############################################################################

class MyCookie: 
  def __init__(self,path):
    self.path = path
    self.tex = lev2.Texture.load(path)
    self.irr = lev2.PbrCommon.requestRadianceMaps(path)    

###############################################################################

class StdSpotLight:
  def __init__( self,
                SGC=None,
                index=0,
                model=None,
                frq=1.0,
                color=vec3(1),
                cookie=None,
                depth_cookie=None,
                fovbase=20.0,
                fovamp=20.0,
                voffset=1,
                vscale=1,
                bias=1e-5,
                dim=2048,
                range=100.0,
                radius=12):
          
    self.radius = radius
    self.voffset = voffset
    self.vscale = vscale
    self.frequency = frq
    self.fovamp = fovamp
    self.fovbase = fovbase
    self.drawable_model = model.createDrawable()
    self.modelnode = SGC.scenegraph.createDrawableNodeOnLayers( [SGC.layer_fwd],     # layers
                                                                "model-node",        # node name
                                                                self.drawable_model) # drawable
    self.modelnode.worldTransform.scale = 0.25
    self.modelnode.worldTransform.translation = vec3(0)
    self.spot_light = lev2.DynamicSpotLight()
    self.spot_light.data.color = color
    self.spot_light.data.fovy = 45
    self.spot_light.lookAt(
      vec3(0,2,1)*4, # eye
      vec3(0,0,0), # tgt 
      vec3(0,1,0)) # up
    self.spot_light.data.range = range
    self.spot_light.data.shadowBias = bias
    self.spot_light.data.shadowMapSize = dim
    self.spot_light.colorCookie = cookie
    self.spot_light.depthCookie = depth_cookie
    #self.spot_light.RadianceCookie = cookie.irr
    self.spot_light.shadowCaster = True
    #print(self.spot_light.shadowMatrix)
    self.lnode = SGC.layer_fwd.createLightNode("spotlight%d"%index,self.spot_light)
    pass
  def update(self,abstime):
    phase = abstime*self.frequency
    ########################################
    x = math.sin(phase)
    y = math.sin(phase*self.frequency*2.0)*self.vscale
    ty = math.sin(phase*2.0)
    z = math.cos(phase)
    fovy = self.fovbase+(1.0+math.sin(phase*3.5))*self.fovamp*0.5
    self.spot_light.data.fovy = fovy
    LPOS =       vec3(x*self.radius,self.voffset+y,z*self.radius)

    self.spot_light.lookAt(
      LPOS, # eye
      vec3(0,ty+1,0), # tgt 
      vec3(0,1,0)) # up
    
    self.modelnode.worldTransform.translation = LPOS
    self.modelnode.worldTransform.orientation = quat(vec3(1,1,1).normalized,  # axis
                                                     phase*self.frequency*16) # angle

###############################################################################

class StandardSceneGraphComponent(ApplicationComponent):

  ###############################################

  def __init__(self,
               enable_ui_camera = True,
               explicit_near_far = False,
               grid_variant="_V4",
               grid_data=None,
               eye=vec3(0,0,5),
               tgt=vec3(0),
               up=vec3(0,1,0),
               near = 0.25,
               far = 20000.0,
               sg_params=None,
               post_nodes=None,
               ssaa=0,
               use_float_color_buffer=True,
               layout_component=None):
    """Initialize StandardSceneGraphComponent.

    Args:
      layout_component: Optional UiLayoutComponent instance. If provided,
        SGC will request viewport placement from this component instead
        of creating its own grid layout. The layout component should
        define a "main" slot where the SceneGraphViewport will be created.
    """
    #print(eye)
    super().__init__()
    self.enable_ui_camera = enable_ui_camera
    self.explicit_near_far = explicit_near_far
    self.grid_variant = grid_variant
    self.grid_data = grid_data
    self.initial_eye = eye
    self.initial_tgt = tgt
    self.initial_up = up
    self.initial_near = near
    self.initial_far = far
    self.use_float_color_buffer = use_float_color_buffer
    self.layout_component = layout_component
    sgparam_vm = VarMap()
    sgparam_vm.SkyboxIntensity = 1.0
    sgparam_vm.DiffuseIntensity = 1.0
    sgparam_vm.SpecularIntensity = 1.0
    sgparam_vm.AmbientLevel = vec3(0)
    sgparam_vm.preset = "ForwardPBR"
    sgparam_vm.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"
    sgparam_vm.ssaa = int(ssaa)
    if sg_params != None:
      for k,v in sg_params.items():
        setattr(sgparam_vm, k, v)
    self.sg_params = sgparam_vm
    self.post_nodes = post_nodes
    if post_nodes is not None:
      assert isinstance(post_nodes, list)
      for item in post_nodes:
        print(f"adding postfx node {item} to scenevars")
        item.addToSceneVars(sgparam_vm,"PostFxChain")
    self.using_pbr = sgparam_vm.preset in ["ForwardPBR", "FWDPBR", "FWDPBRVRDM"]
    self.using_unlit = sgparam_vm.preset in ["UNLIT"]
  ###############################################

  def _onAppInit(self,app,initdata):
    self.app = app
    self.ezapp = self.app.ezapp

  ##################################################

  def _onEzAppCreated(self,app,ezapp):
    self.dbufcontext = ezapp.vars.dbufcontext
    self.cameralut = ezapp.vars.cameras

    # Defer viewport creation to _onGpuLink
    # Store reference to layout component or create default grid
    if self.layout_component is not None:
      # Layout component handles UI structure
      # Viewport will be created in _onGpuLink via layout_component
      self.griditems = None
    else:
      # Default: create 1x1 grid with SceneGraphViewport
      lg_group = ezapp.topLayoutGroup
      self.griditems = lg_group.makeGrid(
        width=1,
        height=1,
        margin = 4,
        uiclass = lev2.ui.SceneGraphViewport,
        args = ["SGVP",vec4(0.1,0.1,0.3,1)],
      )

  ##################################################

  def _onGpuInit(self,ctx):
    sg_params = self.sg_params

    if self.post_nodes is not None:
      for item in self.post_nodes:
        item.gpuInit(ctx,8,8)

    SG = lev2.scenegraph.Scene(sg_params)
    self.layer1 = SG.createLayer("std_forward")
    self.layer_std = self.layer1
    self.layer_fwd = self.layer1
    self.layer_dpp = SG.createLayer("depth_prepass")
    self.scenegraph = SG 
    self.fwd_layers = [self.layer_fwd,self.layer_dpp]

    self.camname = "Camera0"
    self.camera, self.uicam = setupUiCameraX( cameralut=self.cameralut, 
                                              camname=self.camname,
                                              eye=self.initial_eye,
                                              tgt=self.initial_tgt,
                                              up=self.initial_up,
                                              near=self.initial_near,
         
                                              far=self.initial_far)

    self.uicam.explicit_near_far = self.explicit_near_far
    if self.explicit_near_far:
      self.uicam.loc_min = self.initial_near
      self.uicam.loc_max = self.initial_far

    if self.using_pbr:
      self.pbr_common = SG.pbr_common
      self.pbr_common.useDepthPrepass = True
      self.pbr_common.useFloatColorBuffer = self.use_float_color_buffer

    self.rendernode = SG.compositorrendernode
    self.outputnode = SG.compositoroutputnode
    if self.using_pbr:
      self.pbrcommon  = SG.pbr_common

    self.uicam.lookAt( self.initial_eye,
                       self.initial_tgt,
                       self.initial_up )
    
    # EzUiCam exposes clamps (near_min / far_max), not direct near/far.
    # The actual near/far are derived from mfLoc * near_far_ratio in
    # updateMatrices, then clamped into [near_min, far_max].
    self.uicam.near_min = self.initial_near
    self.uicam.far_max  = self.initial_far
    
    ###################################
    # create grid
    ###################################

    if self.grid_data == None:
      if self.grid_variant != None:     
        self.grid_data = createGridData(extent=100.0)
        self.grid_data.shader_suffix = self.grid_variant

    if self.grid_data != None:
      self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
      self.grid_node.sortkey = 1
    
    ###################################
    # initialize lighting
    ###################################

    if self.using_pbr:
      SG.lightingmanager.gpuInit(ctx)

  ##################################################

  def _onGpuLink(self, ctx):
    ###########################
    SG = self.scenegraph

    # Get viewport widget - either from layout component or default grid
    if self.layout_component is not None:
      # Create viewport in layout component's "main" slot
      SGVPW = self.layout_component.provideWidgetForSlot(
        "main",
        lev2.ui.SceneGraphViewport,
        ["SGVP", vec4(0.1, 0.1, 0.3, 1)]
      )
      if SGVPW is None:
        raise RuntimeError("Layout component does not have a 'main' slot for SceneGraphViewport")
    else:
      # Default: use grid item
      SGVP = self.griditems[0]
      SGVPW = SGVP.widget

    SGVPW.cameraName = self.camname
    SGVPW.scenegraph = SG
    if self.enable_ui_camera:
      SGVPW.camera_evhandler = lambda x: self._onCameraUiEvent(x)
    SGVPW.forkDB()
    self.SGVPW = SGVPW  # Store widget reference

    # For backwards compatibility, also store SGVP
    if self.layout_component is not None:
      # Create a simple wrapper for compatibility
      class ViewportWrapper:
        def __init__(self, widget):
          self.widget = widget
      self.SGVP = ViewportWrapper(SGVPW)
    else:
      self.SGVP = self.griditems[0]

  ##################################################

  def _onUpdate(self,updinfo):
   if self.app._shutting_down:
     return
   try:
     self.scenegraph.updateScene(self.cameralut)  # update and enqueue all scenenodes
     self.SGVP.widget.setDirty()
   except RuntimeError:
     # Scenegraph may be destroyed during shutdown
     self.app._shutting_down = True
 
  def _onGpuUpdate(self,ctx):
    pass 

  ##################################################

  def _onCameraUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ##################################################

  def createBallNode(self,
                     name,
                     ctx=None,
                     position=vec3(0),
                     scale=0.3,
                     color=vec4(1, 1, 1, 1),
                     metallic=0.0,
                     roughness=0.5):
    """
    Create a colored ball (sphere) node for visualization.

    Uses pbr_calib.glb model with material overrides for color/metallic/roughness.
    Overrides textures with white so baseColor controls the actual color.
    Useful for visualizing emitters, markers, debug points, etc.

    Args:
      name: Node name (must be unique in scenegraph)
      ctx: Graphics context (required for texture assignment)
      position: Initial position (vec3)
      scale: Uniform scale factor (default: 0.3)
      color: Base color as vec4(r, g, b, a) (default: white)
      metallic: Metallic factor 0.0-1.0 (default: 0.0)
      roughness: Roughness factor 0.0-1.0 (default: 0.5)

    Returns:
      scenegraph.Node: The created scene node
    """
    # Load/cache the ball model and white textures
    if not hasattr(self, '_ball_model'):
      self._ball_model = lev2.XgmModel("data://tests/pbr_calib.glb")
      self._ball_white_tex = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
      self._ball_normal_tex = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    # Create drawable instance
    drawable = self._ball_model.createDrawable()
    modelinst = drawable.modelinst

    # Override material for all submeshes
    for subinst in modelinst.submeshinsts:
      mtl = subinst.material.clone()
      # Override textures with white so baseColor controls color
      if ctx is not None:
        mtl.assignImages(
          ctx,
          color=self._ball_white_tex,
          normal=self._ball_normal_tex,
          mtlruf=self._ball_white_tex,
          doConform=True
        )
      mtl.baseColor = color
      mtl.metallicFactor = metallic
      mtl.roughnessFactor = roughness
      subinst.overrideMaterial(mtl)

    # Create scene node
    node = self.scenegraph.createDrawableNodeOnLayers(
      self.fwd_layers,
      name,
      drawable
    )
    node.worldTransform.translation = position
    node.worldTransform.scale = scale

    return node

