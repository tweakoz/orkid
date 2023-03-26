#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, random, signal, numpy, ork.path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/".."/".."/"examples"/"python").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData, createCubePrim
from common.scenegraph import createSceneGraph
from PIL import Image

save_images = False 
do_offscreen = False 

################################################################################

class panel:
  def __init__(self,cam,uicam):
    self.cur_eye = vec3(0,0,0)
    self.cur_tgt = vec3(0,0,1)
    self.dst_eye = vec3(0,0,0)
    self.dst_tgt = vec3(0,0,0)
    self.counter = 0
    self.camera = cam 
    self.uicam = uicam 

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

    self.cur_eye = self.cur_eye*0.9995 + self.dst_eye*0.0005
    self.cur_tgt = self.cur_tgt*0.9995 + self.dst_tgt*0.0005
    self.uicam.distance = 1
    self.uicam.lookAt( self.cur_eye,
                       self.cur_tgt,
                       vec3(0,1,0))

    self.counter = self.counter-1

    self.uicam.updateMatrices()

    self.camera.copyFrom( self.uicam.cameradata )

################################################################################

class UiSgQuadViewTestApp(object):

  def __init__(self):
    super().__init__()

    self.ezapp = OrkEzApp.create(self,offscreen=do_offscreen)
    self.ezapp.setRefreshPolicy(RefreshFixedFPS, 30)

    # enable UI draw mode
    self.ezapp.topWidget.enableUiDraw()

    # make a grid of scenegraph viewports

    lg_group = self.ezapp.topLayoutGroup
    self.griditems = lg_group.makeGrid( width = 2,
                                        height = 2,
                                        margin = 1,
                                        uiclass = ui.UiSceneGraphViewport,
                                        args = ["box",vec4(1,0,1,1)] )

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    self.dbufcontext = self.ezapp.vars.dbufcontext
    self.cameralut = self.ezapp.vars.cameras

    # shared geometry
    
    self.grid_data = createGridData()
    cube_prim = createCubePrim(ctx=ctx,size=2.0)
    pipeline_cube = createPipeline( app = self, ctx = ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd" )
    mesh = meshutil.Mesh()
    mesh.readFromWavefrontObj("data://tests/simple_obj/cone.obj")
    submesh = mesh.submesh_list[0]
    submesh_prim = meshutil.RigidPrimitive(submesh,ctx)
    pipeline_mesh = createPipeline( app = self, ctx = ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd" )


    # create scenegraph 1   

    sg_params1 = VarMap()
    sg_params1.SkyboxIntensity = 3.0
    sg_params1.DiffuseIntensity = 1.0
    sg_params1.SpecularIntensity = 1.0
    sg_params1.AmbientLevel = vec3(.125)
    #sg_params1.preset = "DeferredPBR"
    sg_params1.preset = "ForwardPBR"
    #sg_params1.dbufcontext = self.dbufcontext

    self.scenegraph1 = scenegraph.Scene(sg_params1)
    self.layer1 = self.scenegraph1.createLayer("layer")
    self.grid_node1 = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node1.sortkey = 1
    self.cube_node1 = cube_prim.createNode("cube",self.layer1,pipeline_cube)

    # create scenegraph 2   

    sg_params2 = VarMap()
    sg_params2.SkyboxIntensity = 0.5
    sg_params2.DiffuseIntensity = 1.0
    sg_params2.SpecularIntensity = 1.0
    sg_params2.AmbientLevel = vec3(.125)
    #sg_params2.preset = "DeferredPBR"
    sg_params2.preset = "ForwardPBR"
    #sg_params2.dbufcontext = self.dbufcontext

    self.scenegraph2 = scenegraph.Scene(sg_params2)
    self.layer2 = self.scenegraph2.createLayer("layer")
    self.grid_node2 = self.layer2.createGridNode("grid",self.grid_data)
    self.grid_node2.sortkey = 1
    self.submesh_prim = submesh_prim
    self.mesh_node = submesh_prim.createNode("mesh",self.layer2,pipeline_mesh)
    #self.cube_node2 = cube_prim.createNode("cube",self.layer2,pipeline_cube)


    # assign shared scenegraph and creat cameras for all sg viewports

    def createPanel(camname, griditem, scenegraph ):

      camera, uicam = setupUiCameraX( cameralut=self.cameralut,
                                        camname=camname)
      griditem.widget.cameraName = camname
      griditem.widget.scenegraph = scenegraph
      griditem.widget.forkDB()

      the_panel = panel(camera, uicam)

      if save_images:
        the_panel.capbuf = CaptureBuffer()
        the_panel.frame_index = 0

        def _on_render():
          rtgroup = griditem.widget.rtgroup
          rtbuffer = rtgroup.buffer(0) #rtg's MRT buffer 0
          FBI = ctx.FBI()
          FBI.captureAsFormat(rtbuffer,the_panel.capbuf,"RGBA8")
          as_np = numpy.array(the_panel.capbuf,dtype=numpy.uint8).reshape( rtgroup.height, rtgroup.width, 4 )
          img = Image.fromarray(as_np, 'RGBA')
          flipped = img.transpose(Image.FLIP_TOP_BOTTOM)
          out_path = ork.path.temp()/("%s-%003d.png"%(camname,the_panel.frame_index))
          flipped.save(out_path)
          the_panel.frame_index += 1

        griditem.widget.onPostRender(_on_render)

      the_panel.griditem = griditem
      return the_panel

    self.panels = [
      createPanel("cameraA",self.griditems[0],self.scenegraph1),
      createPanel("cameraB",self.griditems[1],self.scenegraph1),
      createPanel("cameraC",self.griditems[2],self.scenegraph2),
      createPanel("cameraD",self.griditems[3],self.scenegraph2),
    ]
    
    self.panels[0].griditem.widget.decoupleFromUiSize(4096,4096)
    self.panels[0].griditem.widget.aspect_from_rtgroup = True

  ################################################

  def onUpdate(self,updinfo):

    abstime = updinfo.absolutetime

    cube_y = 0.4+math.sin(abstime)*0.2
    self.cube_node1.worldTransform.translation = vec3(0,cube_y,0) 
    self.cube_node1.worldTransform.orientation = quat(vec3(0,1,0),abstime*90*constants.DTOR) 
    self.cube_node1.worldTransform.scale = 0.1

    for p in self.panels:
      p.update()

    self.scenegraph1.updateScene(self.cameralut) 
    self.scenegraph2.updateScene(self.cameralut) 

    for g in self.griditems:
      g.widget.setDirty()
    
###############################################################################

def onRunLoopIteration():
  # we just need this in-python runloop iteration 
  #  in order to catch ctrl-c from python
  #  so the python signal handler can trigger it's designated callback
  pass

UiSgQuadViewTestApp().ezapp.mainThreadLoop(on_iter=onRunLoopIteration)
