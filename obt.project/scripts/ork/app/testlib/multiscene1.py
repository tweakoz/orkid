import sys, random, math
from ork.app.application import ApplicationComponent
from orkengine.core import vec2, vec3, vec4, quat, CrcStringProxy, VarMap
from orkengine import lev2

l2exdir = (lev2.lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createGridData, createCubePrim
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

class MultiScene1Component(ApplicationComponent):

  def __init__(self,use_8k_textures=False):
    super().__init__()
    self.u8kt = use_8k_textures

  ###############################################

  def _onAppInit(self,app,initdata):
    self.ezapp = app.ezapp
    self.ezapp.topWidget.enableUiDraw()
    lg_group = self.ezapp.topLayoutGroup
    self.lg_group = lg_group

  ##############################################

  def _onUpdateInit(self):
    # seed update thread random for determinism
    # this random generator is used for update thread only
    self.upd_randgen = random.Random()
    self.upd_randgen.seed(123456)

################################################

  def _onUpdate(self,updinfo):
    abstime = updinfo.absolutetime
    self.absolutetime = abstime
    #print("dt: %.3f at: %.3f sec"%(updinfo.deltatime,abstime))
    cube_y = 0.4+math.sin(abstime)*0.2
    for panel in self.panels:
      panel.update(updinfo)
    for g in self.griditems:
      g.widget.setDirty()

  ##############################################

  def _onGpuInit(self,ctx):

    self.dbufcontext = self.ezapp.vars.dbufcontext
    self.cameralut = self.ezapp.vars.cameras
    self.uicontext = self.ezapp.uicontext

    ########################################################
    # shared geometry
    ########################################################
    
    self.grid_data = createGridData()
    cube_prim = createCubePrim(ctx=ctx,size=2.0)
    cube_mtl = createPbrMaterialWithColor( ctx=ctx, 
                                           color=vec4(1,1,1,1), 
                                           roughness=1.0, 
                                           metallic=0.0)
    permu = lev2.FxPipelinePermutation(rendermodel="FORWARD_PBR")
    pipeline_cube = cube_mtl.fxcache.findPipeline(permu) 
    self.cube_mtl = cube_mtl

    # make a grid of scenegraph viewports

    lg_group = self.ezapp.topLayoutGroup
    self.lg_group = lg_group
    self.griditems = lg_group.makeGrid( width = 2,
                                        height = 2,
                                        margin = 4,
                                        uiclass = lev2.ui.SceneGraphViewport,
                                        args = [f"sgview",vec4(1,0,1,1)] )

    lg_group.clearColorGuide = vec4(1,1,.5,1)    
    

    ########################################################
    # create scenegraph / panels
    ########################################################

    class Panel:

      ####################################################################################

      def __init__(self,parent,index):
        #
        self.parent = parent
        self.index = index
        self.camname = "Camera%d"%index
        #
        sg_params = VarMap()
        sg_params.SkyboxIntensity = 1.0
        sg_params.DiffuseIntensity = 1.0
        sg_params.SpecularIntensity = 1.0
        sg_params.AmbientLevel = vec3(0.0)
        sg_params.preset = "ForwardPBR"
        sg_params.ssaa = 4 # 4x4 SuperSample AntiAliasing
        match index:
          case 0:
            sg_params.SkyboxTexPathStr = "arena"
          case 1:
            sg_params.SkyboxTexPathStr = "pillars8k" if parent.u8kt else "pillars"
          case 2:
            sg_params.SkyboxTexPathStr = "nebula"
          case 3:
            sg_params.SkyboxTexPathStr = "futcity8k" if parent.u8kt else "futcity"
        #
        self.scenegraph = lev2.scenegraph.Scene(sg_params)
        self.layer = self.scenegraph.createLayer("std_forward")
        self.grid_node = self.layer.createDrawableNodeFromData("grid",parent.grid_data)
        self.grid_node.sortkey = 0
        self.cube_node = cube_prim.createNode("cube",self.layer,pipeline_cube)
        self.cube_node.sortkey = 1
        #
        self.cameralut = lev2.CameraDataLut()
        self.camera, self.uicam = setupUiCameraX( cameralut=self.cameralut, camname=self.camname )
        self.prv_eye = vec3(3,3,3)
        self.prv_tgt = vec3(3,3,6)
        self.cur_eye = vec3(3,3,3)
        self.cur_tgt = vec3(3,3,6)
        self.dst_eye = self.cur_eye
        self.dst_tgt = self.cur_tgt
        self.cam_time = 1.0
        self.counter = 0
        self.autocam = True
        griditem = parent.griditems[index]
        
        griditem.widget.cameraName = self.camname
        griditem.widget.scenegraph = self.scenegraph
        griditem.widget.forkDB()
        self.scenegraph.lightingmanager.gpuInit(ctx)

        self.griditem = griditem

      ####################################################################################

      def update(self,updinfo):
        dt = updinfo.deltatime
        at = updinfo.absolutetime

        randgen = self.parent.upd_randgen

        if self.autocam:
          def genpos():
            r = vec3(0)
            r.x = randgen.uniform(-30,30)
            r.z = randgen.uniform(-30,30)
            r.y = randgen.uniform( 10,15)
            return r 
        
          if self.counter<=0:
            self.counter = randgen.uniform(3.0,10.0)
            self.prv_eye = self.cur_eye
            self.prv_tgt = self.cur_tgt
            self.dst_eye = genpos()
            Y = randgen.uniform(  0, self.dst_eye.y-3 )
            self.dst_tgt = vec3(0,Y,0)
            self.cam_time = randgen.uniform(2.0,5.0)
            self.cam_time_base = at
          reltime = at - self.cam_time_base
          index = reltime / self.cam_time
          self.cur_eye = (self.prv_eye*(1.0-index)) + (self.dst_eye*index)
          self.cur_tgt = (self.prv_tgt*(1.0-index)) + (self.dst_tgt*index)
          self.uicam.distance = 0.1
          self.uicam.lookAt( self.cur_eye,
                             self.cur_tgt,
                             vec3(0,1,0))

        self.counter -= dt
        
        y = math.sin(self.parent.absolutetime*self.index)*0.85
        q = quat(vec3(0,1,0), self.parent.absolutetime*self.index*0.44)
        self.cube_node.worldTransform.translation = vec3(0,y,0)
        self.cube_node.worldTransform.orientation = q
        self.grid_node.worldTransform.translation = vec3(0)

        self.camera.copyFrom( self.uicam.cameradata )
        self.scenegraph.updateScene(self.cameralut)

    ##########################################################################

    self.panels = [
      Panel(self, 0),
      Panel(self, 1),
      Panel(self, 2),
      Panel(self, 3),
    ]
    
    ##########################################################################

    self.uicontext.dumpWidgets("UI2")
    
