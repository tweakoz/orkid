import sys 
from ork import path as ork_path
from ork.app.application import ApplicationComponent
from orkengine.core import vec3, vec4, VarMap, lev2_pyexdir
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
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )

  ##################################################

  def _onGpuInit(self,ctx):
    sg_params = VarMap()
    sg_params.SkyboxIntensity = 1.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(.125)
    sg_params.preset = "ForwardPBR"
    sg_params.SkyboxTexPathStr = "nebula"
    self.sg_params = sg_params

    SG = lev2.scenegraph.Scene(sg_params)
    self.layer1 = SG.createLayer("std_forward")
    self.layer_std = self.layer1
    self.layer_dpp = SG.createLayer("depth_prepass")
    self.scenegraph = SG 

    self.camname = "Camera0"
    self.camera, self.uicam = setupUiCameraX( cameralut=self.cameralut, 
                                              camname=self.camname )

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1
    self.scenegraph.lightingmanager.gpuInit(ctx)

  ##################################################

  def _onGpuLink(self, ctx):
    ###########################
    SG = self.scenegraph
    SGVP = self.griditems[0]
    SGVPW = SGVP.widget
    SGVPW.cameraName = self.camname
    SGVPW.scenegraph = SG
    SGVPW.evhandler = lambda x: self._onCameraUiEvent(x)
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

