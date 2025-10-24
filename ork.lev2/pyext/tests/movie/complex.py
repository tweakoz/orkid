#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, time, random, signal, numpy, obt.path, os, argparse
from orkengine.core import *
from orkengine.lev2 import *
from obt import host
from ork.singularity import testlib

parser = argparse.ArgumentParser()
parser.add_argument('--freerun', '-f', action='store_true', help='Enable freerun mode (async)')
args = parser.parse_args()

################################################################################

l2exdir = (lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createGridData, createCubePrim
from lev2utils.scenegraph import createSceneGraph

################################################################################

class ComplexMovieApp(object):

  def __init__(self):
    super().__init__()

    self.absolutetime = 0.0
    self.freerun = args.freerun
    self.FPS = 60.0 # frames per second
    self.LEN = 30.0  # seconds
    self.NUMFRAMES = int(self.FPS * self.LEN)
    self.NUMFRAMESP1 = self.NUMFRAMES + 1

    ########################################
    # lockstep mode ?, use STREAM audio device (for movie capture)
    ########################################

    W = 1280 if self.freerun else 1920
    H = 720  if self.freerun else 1080

    self.ezapp = lev2.OrkEzApp.create(
        self,
        enable_audio_synth=True,
        audio_stream_sync=not self.freerun,
        enable_graphics=True,
        freerun=self.freerun,
        target_ups = self.FPS,
        target_fps = self.FPS,
        width=W,
        height=H
    )
    
    if not self.freerun:
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

  ##############################################

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onSynthInit(self,synth):
    testlib.bindSynthToApp(synth,             # synth instance
                           self,              # app instance
                           initial_gain=0.0,  # initial gain in dB
                           main_fx="ShifterChorus")  # main bus effect
    self.waveprog = testlib.WaveformsProgram()
    P = self.waveprog.program
    synth.programbus.uiprogram = P
    mods = None
    self.v1 = synth.keyOn(24,127,P,mods)
    time.sleep(0.1)
    self.v2 = synth.keyOn(36,127,P,mods)
    time.sleep(0.1)
    self.v3 = synth.keyOn(12,127,P,mods)
    time.sleep(0.1)
    self.v4 = synth.keyOn(48,127,P,mods)
    time.sleep(0.1)
    self.v5 = synth.keyOn(60,127,P,mods)

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
    cube_mtl = createPbrMaterialWithColor( ctx=ctx, 
                                           color=vec4(1,1,1,1), 
                                           roughness=1.0, 
                                           metallic=0.0)
    permu = lev2.FxPipelinePermutation(rendermodel="FORWARD_PBR")
    pipeline_cube = cube_mtl.fxcache.findPipeline(permu) 
    self.cube_mtl = cube_mtl

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
          case 1:
            sg_params.SkyboxTexPathStr = "cold"
          case 2:
            sg_params.SkyboxTexPathStr = "nebula"
          case 3:
            sg_params.SkyboxTexPathStr = "futcity"
        #
        self.scenegraph = scenegraph.Scene(sg_params)
        self.layer = self.scenegraph.createLayer("std_forward")
        self.grid_node = self.layer.createDrawableNodeFromData("grid",parent.grid_data)
        self.grid_node.sortkey = 0
        self.cube_node = cube_prim.createNode("cube",self.layer,pipeline_cube)
        self.cube_node.sortkey = 1
        #
        self.cameralut = CameraDataLut()
        self.camera, self.uicam = setupUiCameraX( cameralut=self.cameralut, camname=self.camname )
        self.cur_eye = vec3(3,3,3)
        self.cur_tgt = vec3(3,3,6)
        self.dst_eye = self.cur_eye
        self.dst_tgt = self.cur_tgt
        self.counter = 0

        griditem = parent.griditems[index]
        
        griditem.widget.cameraName = self.camname
        griditem.widget.scenegraph = self.scenegraph
        griditem.widget.forkDB()
        self.scenegraph.lightingmanager.gpuInit(ctx)

        self.griditem = griditem

      ####################################################################################

      def update(self):
        def genpos():
          r = vec3(0)
          r.x = random.uniform(-30,30)
          r.z = random.uniform(-30,30)
          r.y = random.uniform( 10,15)
          return r 
      
        if self.counter<=0:
          self.counter = int(random.uniform(1,500))
          self.dst_eye = genpos()
          Y = random.uniform(  0, self.dst_eye.y-3 )
          self.dst_tgt = vec3(0,Y,0)

        self.cur_eye = (self.cur_eye*0.995) + (self.dst_eye*0.005)
        self.cur_tgt = (self.cur_tgt*0.995) + (self.dst_tgt*0.005)
        self.uicam.distance = 0.1
        self.uicam.lookAt( self.cur_eye,
                           self.cur_tgt,
                           vec3(0,1,0))

        self.counter = self.counter-1
        
        y = math.sin(self.parent.absolutetime*self.index)*0.85
        q = quat(vec3(0,1,0), self.parent.absolutetime*self.index*0.44)
        self.cube_node.worldTransform.translation = vec3(0,y,0)
        self.cube_node.worldTransform.orientation = q
        self.grid_node.worldTransform.translation = vec3(0)

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

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 4
    item = lg_group.makeEvTestBox( w=100, #
                                   h=100, #
                                   x=100, #
                                   y=100, #
                                   color_normal=vec4(0.75,0.75,0.75,0.5), #
                                   color_click=vec4(0.5,0.0,0.0,0.5), #
                                   color_doubleclick=vec4(0.5,1.0,0.5,0.5), #
                                   color_drag=vec4(0.5,0.5,1.0,0.5), #
                                   name="testbox1")
    lg_group.replaceChild(self.panels[0].griditem.layout,item)
    self.uicontext.dumpWidgets("UI2")
    lg_group.clearColorGuide = vec4(1,0,1,1)

    self.rencount = 0

  ################################################

  def onUpdate(self,updinfo):
    abstime = updinfo.absolutetime
    self.absolutetime = abstime
    cube_y = 0.4+math.sin(abstime)*0.2
    for panel in self.panels:
      panel.update()
    for g in self.griditems:
      g.widget.setDirty()
    

  ##############################################

  def onGpuPostFrame(self, ctx):
    self.rencount += 1
    if self.freerun == False:
      match self.rencount:
        case 2:
          self.mcc = self.ezapp.enableMovieRecording( output_path="/tmp/str_audio_test_movie.mp4",
                                                      preset="ultra",
                                                      fps=self.FPS,
                                                      max_queue_size=180,
                                                      audio_test_tone=False )
        case self.NUMFRAMES:
          self.ezapp.finishMovieRecording()
        case self.NUMFRAMESP1:
          self.ezapp.signalExit()

###############################################################################

app = ComplexMovieApp()
app.ezapp.mainThreadLoop(on_iter=lambda : False)

if host.IsOsx and not app.freerun:
  time.sleep(1)
  os.system("open /tmp/str_audio_test_movie.mp4")
