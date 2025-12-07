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
    self.spot_light.data.fovy = math.radians(45)
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
    self.spot_light.data.fovy = math.radians(fovy)
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
               grid_variant="_V4",
               grid_data=None,
               eye=vec3(0,0,5),
               tgt=vec3(0),
               up=vec3(0,1,0),
               sg_params=None,
               post_nodes=None):
    print(eye)
    super().__init__()
    self.enable_ui_camera = enable_ui_camera
    self.grid_variant = grid_variant
    self.grid_data = grid_data
    self.initial_eye = eye
    self.initial_tgt = tgt
    self.initial_up = up
    sgparam_vm = VarMap()
    sgparam_vm.SkyboxIntensity = 1.0
    sgparam_vm.DiffuseIntensity = 1.0
    sgparam_vm.SpecularIntensity = 1.0
    sgparam_vm.AmbientLevel = vec3(.125)
    sgparam_vm.preset = "ForwardPBR"
    sgparam_vm.SkyboxTexPathStr = "cold"
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
                                              far=10000.0)
    if self.using_pbr:
      self.pbr_common = SG.pbr_common
      self.pbr_common.useDepthPrepass = True

    self.rendernode = SG.compositorrendernode
    self.outputnode = SG.compositoroutputnode
    if self.using_pbr:
      self.pbrcommon  = SG.pbr_common

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
    SGVP = self.griditems[0]
    SGVPW = SGVP.widget
    SGVPW.cameraName = self.camname
    SGVPW.scenegraph = SG
    if self.enable_ui_camera:
      SGVPW.camera_evhandler = lambda x: self._onCameraUiEvent(x)
    SGVPW.forkDB()
    self.SGVP = SGVP

  ##################################################

  def _onUpdate(self,updinfo):
   self.scenegraph.updateScene(self.cameralut)  # update and enqueue all scenenodes
   self.SGVP.widget.setDirty()
 
  def _onGpuUpdate(self,ctx):
    pass 

  ##################################################

  def _onCameraUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

