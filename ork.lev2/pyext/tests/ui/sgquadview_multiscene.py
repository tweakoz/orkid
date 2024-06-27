#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, random, signal, numpy, obt.path, os
from orkengine.core import *
from orkengine.lev2 import *
from PIL import Image

################################################################################

l2exdir = (lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createGridData, createCubePrim
from lev2utils.scenegraph import createSceneGraph

################################################################################

save_images = False 
do_offscreen = False 

################################################################################

class UiSgQuadViewTestApp(object):

  def __init__(self):
    super().__init__()

    self.ezapp = OrkEzApp.create(self, offscreen=do_offscreen)
    self.ezapp.setRefreshPolicy(RefreshFixedFPS, 30)

    # enable UI draw mode
    self.ezapp.topWidget.enableUiDraw()

    # make a grid of scenegraph viewports

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 4
    self.griditems = lg_group.makeGrid( width = 2,
                                        height = 2,
                                        margin = 4,
                                        uiclass = ui.SceneGraphViewport,
                                        args = ["box",vec4(1,0,1,1)] )

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
    mesh = meshutil.Mesh()
    mesh.readFromWavefrontObj("data://tests/simple_obj/cone.obj")
    submesh = mesh.submesh_list[0]
    submesh_prim = RigidPrimitive(submesh,ctx)
    pipeline_mesh = createPipeline( app = self, ctx = ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd" )

    ########################################################
    # scenegraph init data
    ########################################################

    sg_params = VarMap()
    sg_params.SkyboxIntensity = 3.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(.125)
    #sg_params.preset = "DeferredPBR"
    sg_params.preset = "ForwardPBR"
    #sg_params.dbufcontext = self.dbufcontext

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
        self.scenegraph = scenegraph.Scene(sg_params)
        self.layer = self.scenegraph.createLayer("std_forward")
        self.grid_node = self.layer.createGridNode("grid",parent.grid_data)
        self.grid_node.sortkey = 1
        self.cube_node = cube_prim.createNode("cube",self.layer,pipeline_cube)
        #
        self.cameralut = CameraDataLut()
        self.camera, self.uicam = setupUiCameraX( cameralut=self.cameralut, camname=self.camname )
        self.cur_eye = vec3(0,0,0)
        self.cur_tgt = vec3(0,0,1)
        self.dst_eye = vec3(0,0,0)
        self.dst_tgt = vec3(0,0,0)
        self.counter = 0
        self.use_event = False

        griditem = parent.griditems[index]
        
        griditem.widget.cameraName = self.camname
        griditem.widget.scenegraph = self.scenegraph
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
          
          return ui.HandlerResult()

        griditem.widget.evhandler = lambda ev: onPanelEvent(index,ev)

        ########################################### 

        if save_images:
          self.capbuf = CaptureBuffer()
          self.frame_index = 0

          def _on_render():
            rtgroup = griditem.widget.rtgroup
            rtbuffer = rtgroup.buffer(0) #rtg's MRT buffer 0
            FBI = ctx.FBI()
            FBI.captureAsFormat(rtbuffer,self.capbuf,"RGBA8")
            as_np = numpy.array(self.capbuf,dtype=numpy.uint8).reshape( rtgroup.height, rtgroup.width, 4 )
            img = Image.fromarray(as_np, 'RGBA')
            flipped = img.transpose(Image.FLIP_TOP_BOTTOM)
            out_path = ork.path.temp()/("%s-%003d.png"%(camname,self.frame_index))
            flipped.save(out_path)
            self.frame_index += 1

          griditem.widget.onPostRender(_on_render)

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
      Panel(self, 2),
      Panel(self, 3),
    ]
    
    ##########################################################################

    self.panels[0].griditem.widget.decoupleFromUiSize(4096,4096)
    self.panels[0].griditem.widget.aspect_from_rtgroup = True

    if True: # try out widget replacement
      lg_group = self.ezapp.topLayoutGroup
      item = lg_group.makeEvTestBox( w=100, #
                                     h=100, #
                                     x=100, #
                                     y=100, #
                                     color_normal=vec4(0.75,0.75,0.75,0.5), #
                                     color_click=vec4(0.5,0.0,0.0,0.5), #
                                     color_doubleclick=vec4(0.5,1.0,0.5,0.5), #
                                     color_drag=vec4(0.5,0.5,1.0,0.5), #
                                     name="testbox1")
      self.uicontext.dumpWidgets("UI1")
      lg_group.layout.dump()
      #lg_group.removeChild(self.panels[0].griditem.layout)
      lg_group.replaceChild(self.panels[0].griditem.layout,item)
      self.uicontext.dumpWidgets("UI2")
      lg_group.layout.dump()
      lg_group.clearColor = vec4(1,0,1,1)

  ################################################

  def onUpdate(self,updinfo):

    abstime = updinfo.absolutetime

    cube_y = 0.4+math.sin(abstime)*0.2
    #self.cube_node1.worldTransform.translation = vec3(0,cube_y,0) 
    #self.cube_node1.worldTransform.orientation = quat(vec3(0,1,0),abstime*90*constants.DTOR) 
    #self.cube_node1.worldTransform.scale = 0.1

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
