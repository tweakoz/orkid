#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a mapplotlib plot to a UI widget
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, signal, random
import numpy as np
from obt import path
from orkengine.core import vec2, vec3, vec4, mtx4, quat, VarMap, CrcStringProxy
from orkengine import lev2
import matplotlib.pyplot as plt
from matplotlib.backends.backend_agg import FigureCanvasAgg

tokens = CrcStringProxy()

################################################################################

l2exdir = (lev2.lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createGridData, createCubePrim
from lev2utils.scenegraph import createSceneGraph

################################################################################

class PackWidgets(object):

  def __init__(self):
    super().__init__()
    self.done = False

    self.box_height = 0.0

    self.ezapp = lev2.OrkEzApp.create(self,
                                      fullscreen=False,
                                      enable_audio=False,
                                      enable_audio_output=False,
                                      enable_audio_synth=False)

    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorGuide = vec4(0.8,0.6,0.2,1)

    ############################################
    # start out with a 2x2 grid of boxes
    ############################################

    self.griditems = lg_group.makeGrid(
      width=2,
      height=1,
      margin = 4,
      uiclass = lev2.ui.Box,
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )
    
    ############################################

    self.lg_group = lg_group
    lg_group.margin = 4

    ############################################
    # replace box 0 with an imageview
    #  (which will get it's image from matplotlib)
    ############################################

    imv1 = lg_group.makeChild( uiclass=lev2.ui.ImageView, args=["vpack1"])
    self.lg_group.replaceChild( self.griditems[0].layout, imv1 )
    self.imv1w = imv1.widget
    self.imv1w.maintain_aspect_ratio = True

    self.prev_w = self.imv1w.width
    self.prev_h = self.imv1w.height
    self.prev_image = None
    self.fig = plt.figure(figsize=(self.prev_w/100.0,self.prev_h/100.0), dpi=100)
    self.canvas = FigureCanvasAgg(self.fig)
    # Plot something
    self.ax = self.fig.add_subplot(111)
    self.ax.plot([1, 2, 3, 4], [1, 4, 2, 3])

    ############################################
    
    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def imageProviderMatPlotLib(self):

    ############################
    # cache previous size/image ?
    ############################
    w = self.imv1w.width
    h = self.imv1w.height
    #if w == self.prev_w and h == self.prev_h:
    #  return self.prev_image
    ############################
    # widget resized, update figure/canvas
    ############################
    self.prev_w = w
    self.prev_h = h
    self.fig.set_size_inches(w/100.0, h/100.0, forward=True)
    ############################
    # animate the plot
    ############################
    self.ax.clear()
    t = self.abstime
    x = np.linspace(0, 4 * np.pi, 100)
    y = np.sin(x + t + np.sin(x + t * 2.5))
    self.ax.plot(x, y)
    self.ax.set_ylim(-1.5, 1.5)
    self.ax.set_title("MatPlotLib Plot")
    self.ax.set_xlabel("x")
    self.ax.set_ylabel("sin(x + t + sin(x+t*2.5))")
    ############################
    # Render to canvas
    ############################
    self.canvas.draw()
    ############################
    # grab the RGBA buffer from the figure
    ############################
    rgba_buf = np.asarray(self.canvas.buffer_rgba())
    ############################
    # create an ork image from the RGBA buffer
    ############################
    w = rgba_buf.shape[1]
    h = rgba_buf.shape[0]
    # prefer RGBA8 as no conversion is needed (faster)
    image = lev2.Image.createFromBuffer( w,h, tokens.RGBA8, rgba_buf)
    self.prev_image = image
    return image

  ##############################################

  def onGpuInit(self,ctx):         
    ############################
    # imgview1 widget gets its image 
    #   from the matplotlib provider
    ############################
    prov = lev2.ImageProvider.createFromLambda( lambda: self.imageProviderMatPlotLib() )
    self.imv1w.setImageProvider( prov )

    ########################################################
    # shared geometry (for scenegraph viewport)
    ########################################################
    
    self.grid_data = createGridData()
    cube_prim = createCubePrim(ctx=ctx,size=2.0)
    pipeline_cube = createPipeline( app = self, ctx = ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd" )
    mesh = lev2.meshutil.Mesh()
    mesh.readFromWavefrontObj("data://tests/simple_obj/cone.obj")
    submesh = mesh.submesh_list[0]
    submesh_prim = lev2.RigidPrimitive(submesh,ctx)
    pipeline_mesh = createPipeline( app = self, ctx = ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd" )

    ########################################################
    # scenegraph init data
    ########################################################

    sg_params = VarMap()
    sg_params.SkyboxIntensity = 3.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(.125)
    sg_params.preset = "ForwardPBR"

    self.scenegraph = lev2.scenegraph.Scene(sg_params)
    self.layer = self.scenegraph.createLayer("std_forward")
    self.grid_node = self.layer.createDrawableNodeFromData("grid",self.grid_data)
    self.grid_node.sortkey = 1
    self.cube_node = cube_prim.createNode("cube",self.layer,pipeline_cube)

    ########################################################
    # setup a camera for the scenegraph 
    ########################################################

    self.camname = "Camera0"

    self.cameralut = CameraDataLut()
    self.camera, self.uicam = setupUiCameraX( cameralut=self.cameralut, camname=self.camname )
    self.cur_eye = vec3(0,0,0)
    self.cur_tgt = vec3(0,0,1)
    self.dst_eye = vec3(0,0,0)
    self.dst_tgt = vec3(0,0,0)
    self.counter = 0

    ########################################################
    # finally, add a scenegraph viewport to the tabbed widget
    ########################################################

    self.sgvl = self.lg_group.makeChild( uiclass=lev2.ui.SceneGraphViewport, args=["sg",vec4(0,0,0,1)] )   
    self.sgvw = self.sgvl.widget
    self.sgvw.cameraName = self.camname
    self.sgvw.scenegraph = self.scenegraph
    self.sgvw.forkDB()
    self.scenegraph.lightingmanager.gpuInit(ctx)
    self.lg_group.replaceChild( self.griditems[1].layout, self.sgvl )

  ################################################

  def onUpdate(self,updinfo):

    abstime = updinfo.absolutetime
    self.abstime = abstime
    self.sgvw.setDirty()

    def genpos():
      r = vec3(0)
      r.x = random.uniform(-20,20)
      r.z = random.uniform(-20,20)
      r.y = random.uniform(10,30)
      return r 
  
    if self.counter<=0:
      self.counter = int(random.uniform(1,1000))
      self.dst_eye = genpos()
      self.dst_tgt = vec3(0,0,0)

    self.cur_eye = self.cur_eye*0.9995 + self.dst_eye*0.0005
    self.cur_tgt = self.cur_tgt*0.9995 + self.dst_tgt*0.0005
    self.uicam.distance = 2
    self.uicam.lookAt( self.cur_eye,
                      self.cur_tgt,
                      vec3(0,1,0))

    self.counter = self.counter-1
    self.uicam.updateMatrices()
    self.camera.copyFrom( self.uicam.cameradata )
    self.scenegraph.updateScene(self.cameralut)

    
  ##############################################

  def onUiEvent(self,uievent):
    return lev2.ui.HandlerResult()

###############################################################################

w = PackWidgets()
rval = w.ezapp.mainThreadLoop()
sys.exit(-1)


