#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import vec3, vec4, quat, mtx4, Transform
from orkengine.core import lev2_pyexdir, CrcStringProxy, VarMap
from orkengine import lev2

################################################################################

lev2_pyexdir.addToSysPath()

from lev2utils.cameras import setupUiCameraX
from lev2utils.primitives import createGridData, createCubePrim
from lev2utils.shaders import createPipeline
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

################################################################################

tokens = CrcStringProxy()

################################################################################

class UiSgQuadViewTestApp(object):

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFixedFPS, 30)

    # enable UI draw mode
    self.ezapp.topWidget.enableUiDraw()

    # make a grid of scenegraph viewports

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 5
    self.griditems = lg_group.makeGrid( width = 2,
                                        height = 1,
                                        margin = 8,
                                        uiclass = lev2.ui.SceneGraphViewport,
                                        args = ["box",vec4(1,0,1,1)] )

    self.abstime = 0.0

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    self.dbufcontext = self.ezapp.vars.dbufcontext
    self.cameralut = self.ezapp.vars.cameras
    self.uicontext = self.ezapp.uicontext

    ########################################################
    # shared geometry
    ########################################################
    
    self.grid_data = createGridData()
    cube_prim = createCubePrim(ctx=ctx,size=2.0)
    pipeline_cube = createPipeline( app = self, ctx = ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd" )
    mesh = lev2.meshutil.Mesh()
    mesh.readFromWavefrontObj("data://tests/simple_obj/cone.obj")
    submesh = mesh.submesh_list[0]
    submesh_prim = lev2.RigidPrimitive(submesh,ctx)
    pipeline_mesh = createPipeline( app = self, ctx = ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd" )

    self.model = lev2.XgmModel("data://tests/chartest/char_mesh")
    self.anim = lev2.XgmAnim("data://tests/chartest/char_testanim1")

    ########################################################
    # scenegraph init data
    ########################################################

    sg_params_fwd = VarMap()
    sg_params_fwd.SkyboxIntensity = 3.0
    sg_params_fwd.DiffuseIntensity = 1.0
    sg_params_fwd.SpecularIntensity = 1.0
    sg_params_fwd.AmbientLevel = vec3(.125)
    sg_params_fwd.DepthFogDistance = 10000.0
    sg_params_fwd.preset = "ForwardPBR"


    ########################################################
    # create scenegraph / panels
    ########################################################

    self.shared_cameralut = lev2.CameraDataLut()
    self.shared_camera, self.shared_uicam = setupUiCameraX( cameralut=self.shared_cameralut, 
                                                            camname="SharedCamera",
                                                            eye = vec3(0,5,20),
                                                            tgt = vec3(0,5,0),
                                                            up = vec3(0,1,0) )


    class Panel:

      ####################################################################################

      def __init__(self,parent,index):
        #
        self.parent = parent
        self.index = index
        self.camname = "Camera%d"%index
        self.use_event = False

        #

        self.cameralut = lev2.CameraDataLut()
        self.cur_eye = vec3(0,0,0)
        self.cur_tgt = vec3(0,0,1)
        self.dst_eye = vec3(0,0,0)
        self.dst_tgt = vec3(0,0,0)
        self.counter = 0

        griditem = parent.griditems[index]        
        self.cameralut = lev2.CameraDataLut()
        self.camera, self.uicam = setupUiCameraX( cameralut=self.cameralut, camname=self.camname )

        if True:

          if index==0:
            sg_params_def = VarMap()
            sg_params_def.SkyboxIntensity = 3.0
            sg_params_def.DiffuseIntensity = 1.0
            sg_params_def.SpecularIntensity = 1.0
            sg_params_def.AmbientLevel = vec3(.125)
            sg_params_def.DepthFogDistance = 10000.0
            sg_params_def.preset = "DeferredPBR"
            self.scenegraph = lev2.scenegraph.Scene(sg_params_def)
          else:
            comp_tek = lev2.NodeCompositingTechnique()
            comp_tek.renderNode = lev2.DeferredPbrRenderNode()
            comp_tek.outputNode = lev2.ScreenOutputNode()

            comp_data = lev2.CompositingData()
            comp_scene = comp_data.createScene("scene1")
            comp_sceneitem = comp_scene.createSceneItem("item1")
            comp_sceneitem.technique = comp_tek

            # OVERRIDES

            pbr_common = comp_tek.renderNode.pbr_common
            pbr_common.requestSkyboxTexture("src://envmaps/tozenv_hellscape")
            pbr_common.environmentIntensity = 1
            pbr_common.environmentMipBias = 10
            pbr_common.environmentMipScale = 1
            pbr_common.diffuseLevel = 1
            pbr_common.specularLevel = 1
            pbr_common.specularMipBias = 1
            pbr_common.skyboxLevel = .5
            pbr_common.depthFogDistance = 100
            pbr_common.depthFogPower = 1
            #comp_tek.renderNode.overrideShader(str(this_dir/"sgdualview.glfx"))

            sg_params_xxx = VarMap()
            sg_params_xxx.preset = "USER"
            sg_params_xxx.compositordata = comp_data
            self.scenegraph = lev2.scenegraph.Scene(sg_params_xxx)

          self.use_event = True
          self.layer = self.scenegraph.createLayer("layer")
          self.sgnode = parent.model.createNode("modelnode",self.layer)

          self.anim_inst = lev2.XgmAnimInst(parent.anim)
          self.anim_inst.mask.enableAll()
          self.anim_inst.use_temporal_lerp = True
          self.anim_inst.bindToSkeleton(parent.model.skeleton)

          self.modelinst = self.sgnode.user.pyext_retain_modelinst
          self.modelinst.enableSkinning()
          self.modelinst.enableAllMeshes()
          self.localpose = self.modelinst.localpose
          self.worldpose = self.modelinst.worldpose
          griditem.widget.cameraName = "SharedCamera"
          griditem.widget.scenegraph = self.scenegraph
          self.cameralut = parent.shared_cameralut
          self.camera = parent.shared_camera
          self.uicam = parent.shared_uicam

        #

        griditem.widget.forkDB()

        ########################################### 
        # route events to panels ui camera ?
        ########################################### 

        def onPanelEvent(index, event):
          if event.code == tokens.KEY_DOWN.hashed or event.code == tokens.KEY_REPEAT.hashed:
            if event.keycode == 32: # spacebar
              self.use_event = not self.use_event

          if self.use_event:
            self.uicam.uiEventHandler(event)
          
          return lev2.ui.HandlerResult()

        griditem.widget.evhandler = lambda ev: onPanelEvent(index,ev)
        self.griditem = griditem

      ####################################################################################

      def update(self):
        def genpos():
          r = vec3(0)
          r.x = random.uniform(-10,10)
          r.z = random.uniform(-10,10)
          r.y = random.uniform(  0,10)
          return r 
      
        if self.counter<=0:
          self.counter = int(random.uniform(1,1000))
          self.dst_eye = genpos()
          self.dst_tgt = vec3(0,random.uniform(  0,2),0)

        if not self.use_event:
          self.cur_eye = self.cur_eye*0.9995 + self.dst_eye*0.0005
          self.cur_tgt = self.cur_tgt*0.9995 + self.dst_tgt*0.0005
          self.uicam.distance = 1
          self.uicam.lookAt( self.cur_eye,
                            self.cur_tgt,
                            vec3(0,1,0))

        self.counter = self.counter-1

        self.uicam.updateMatrices()
        self.camera.copyFrom( self.uicam.cameradata )
        self.scenegraph.updateScene(self.cameralut)

    ##########################################################################

    self.panels = [
      Panel(self, 0),
      Panel(self, 1),
    ]
    
  ################################################

  def onUpdate(self,updinfo):

    self.abstime = updinfo.absolutetime

    for panel in self.panels:
      panel.update()

    for g in self.griditems:
      g.widget.setDirty()
    

###############################################################################

def onRunLoopIteration():
  # we just need this in-python runloop iteration 
  #  in order to catch ctrl-c from python
  #  so the python signal handler can trigger it's designated callback
  pass

UiSgQuadViewTestApp().ezapp.mainThreadLoop(on_iter=onRunLoopIteration)
