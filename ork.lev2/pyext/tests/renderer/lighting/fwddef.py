#!/usr/bin/env python3

################################################################################
# lev2 test which renders 2 UI panels (one with deferred and one with forward rendering)
#  used for deferred/forward subjective comparison...
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import vec2, vec3, vec4, quat, mtx4, Transform
from orkengine.core import lev2_pyexdir, CrcStringProxy, VarMap
from orkengine import lev2

################################################################################

lev2_pyexdir.addToSysPath()

from lev2utils.cameras import setupUiCameraX
from lev2utils.primitives import createGridData, createCubePrim
from lev2utils.shaders import createPipeline
from lev2utils.scenegraph import canonicalizeSG
from lev2utils.lighting import MySpotLight, MyCookie

################################################################################

tokens = CrcStringProxy()

################################################################################

class RenderTestApp(object):

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self,ssaa=1)
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

    self.gpu_update_handlers = []
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

    self.model = lev2.XgmModel("data://tests/chartest/char_mesh")
    self.anim = lev2.XgmAnim("data://tests/chartest/char_testanim1")

    ##################
    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    normal = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
    for mesh in self.model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.baseColor = vec4(1,1,1,1)
        copy.metallicFactor = 1.0
        copy.roughnessFactor = 0.0
        copy.assignImages(
          ctx,
          color = white,
          normal = normal,
          mtlruf = white,
          doConform=True
        )
        submesh.material = copy

    self.grid_data = createGridData()
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(2)
    self.grid_data.intensityA = 1.0
    self.grid_data.intensityB = 0.97
    self.grid_data.intensityC = 0.9
    self.grid_data.intensityD = 0.85
    self.grid_data.lineWidth = 0.2
    
    ########################################################
    # scenegraph init data
    ########################################################

    sg_params = VarMap()
    sg_params.SkyboxTexPathStr = "src://envmaps/blender_studio.dds"
    sg_params.SkyboxIntensity = 1.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(0)
    sg_params.DepthFogDistance = 10000.0

    ########################################################
    # create scenegraph / panels
    ########################################################

    self.shared_cameralut = lev2.CameraDataLut()
    self.shared_camera, self.shared_uicam = setupUiCameraX( cameralut=self.shared_cameralut, 
                                                            camname="SharedCamera",
                                                            eye = vec3(0,15,30),
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
        self.counter = 0

        griditem = parent.griditems[index]        
        self.cameralut = lev2.CameraDataLut()
        self.camera, self.uicam = setupUiCameraX( cameralut=self.cameralut, camname=self.camname )


        if index==0:
          rendermodel = "ForwardPBR"
        else:
          rendermodel = "DeferredPBR"

        sg_params_sub = sg_params.clone()
        sg_params_sub.preset = rendermodel
        self.scenegraph = lev2.scenegraph.Scene(sg_params_sub)
        canonicalizeSG(self,self.scenegraph,rendermodel)

        self.string_drwdata = lev2.StringDrawableData()
        self.string_drwdata.text = rendermodel
        self.string_drwdata.pos2D = vec2(10,10)
        self.string_drw = self.string_drwdata.createDrawable()
        #self.str_node = self.scenegraph.createDrawableNodeOnLayers(self.std_layers,"label",self.string_drw)

        self.grid_node = self.layer_std.createGridNode("grid",parent.grid_data)
        self.grid_node.sortkey = 1
        self.grid_node.worldTransform.translation = vec3(0,-5,0)
        self.use_event = True
        self.model_drawable = parent.model.createDrawable()
        self.modelinst = self.model_drawable.modelinst
        self.modelinst.enableSkinning()
        self.modelinst.enableAllMeshes()
        self.sgnode = self.scenegraph.createDrawableNodeOnLayers(self.std_layers,"modelnode",self.model_drawable)

        self.anim_inst = lev2.XgmAnimInst(parent.anim)
        self.anim_inst.mask.enableAll()
        self.anim_inst.use_temporal_lerp = True
        self.anim_inst.bindToSkeleton(parent.model.skeleton)

        self.localpose = self.modelinst.localpose
        self.worldpose = self.modelinst.worldpose
        griditem.widget.cameraName = "SharedCamera"
        griditem.widget.scenegraph = self.scenegraph
        self.cameralut = parent.shared_cameralut
        self.camera = parent.shared_camera
        self.uicam = parent.shared_uicam

        def handler(context):
          self.localpose.bindPose()
          self.anim_inst.currentFrame = parent.abstime*30.0
          self.anim_inst.weight = 1.0
          self.anim_inst.applyToPose(self.localpose)
          self.localpose.blendPoses()
          self.localpose.concatenate()
          self.worldpose.fromLocalPose(self.localpose,mtx4())

        parent.gpu_update_handlers += [handler]

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
        self.uicam.updateMatrices()
        self.camera.copyFrom( self.uicam.cameradata )
        self.scenegraph.updateScene(self.cameralut)
        self.counter = self.counter-1

    ##########################################################################

    self.panels = [
      Panel(self, 0),
      Panel(self, 1),
    ]
    
  ################################################

  def onGpuUpdate(self,context):
    for handler in self.gpu_update_handlers:
      handler(context)

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

RenderTestApp().ezapp.mainThreadLoop(on_iter=onRunLoopIteration)
