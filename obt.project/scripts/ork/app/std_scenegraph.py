import sys 
from ork import path as ork_path
from ork.app.application import ApplicationComponent
from orkengine.core import vec3, VarMap, lev2_pyexdir
from orkengine import lev2 
sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path
lev2_pyexdir.addToSysPath()
from cameras import setupUiCameraX
from primitives import createGridData

class StandardSceneGraphComponent(ApplicationComponent):

  ###############################################

  def __init__(self):
    pass

  ###############################################

  def _onAppInit(self,app,initdata):

    self.ezapp = app.ezapp
    sg_params = VarMap()
    sg_params.SkyboxIntensity = 1.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(.125)
    sg_params.preset = "ForwardPBR"
    sg_params.SkyboxTexPathStr = "ork_envmaps|tozenv_nebula"
    self.sg_params = sg_params

    #createSceneGraph(app=self, rendermodel="ForwardPBR", params_dict=sg_params)
    #setupUiCamera(app=self, eye=vec3(6,6,6), constrainZ=True, up=vec3(0,1,0))
    SG = lev2.scenegraph.Scene(sg_params)
    self.layer1 = SG.createLayer("std_forward")
    self.layer_std = self.layer1
    self.layer_dpp = SG.createLayer("depth_prepass")
    self.scenegraph = SG 

    self.camname = "Camera0"
    self.cameralut = lev2.CameraDataLut()
    self.camera, self.uicam = setupUiCameraX( cameralut=self.cameralut, 
                                              camname=self.camname )

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1

  ##################################################

  def _onGpuInit(self,ctx):
    self.scenegraph.lightingmanager.gpuInit(ctx)

  ##################################################

  def _onUpdate(self,updinfo):
   self.uicam.updateMatrices()
   self.camera.copyFrom( self.uicam.cameradata )
   self.scenegraph.updateScene(self.cameralut)  # update and enqueue all scenenodes

  ##################################################

  def _onUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()

  ##################################################

  def onCameraUiEvent(self, uievent):
    if hasattr(self, 'uicam') and hasattr(self, 'camera'):
      handled = self.uicam.uiEventHandler(uievent)
      if handled:
        self.uicam.updateMatrices()
        self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ###############################################
    
  def _onGpuPostFrame(self, ctx):
    pass
