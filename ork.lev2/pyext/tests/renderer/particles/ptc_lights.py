#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random, numpy, argparse
from obt import path
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
lev2_pyexdir.addToSysPath()
from lev2utils.cameras import *
from lev2utils.scenegraph import createSceneGraph
from signal import signal, SIGINT

tokens = CrcStringProxy()

from _ptc_harness import *
SSAO_NUM_SAMPLES = 0
################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

################################################################################

class ParticlesApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=0, fullscreen=True)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)

    #self.materials = set()

    setupUiCamera( app=self, #
                   eye = vec3(0,100,150), #
                   constrainZ=True, #
                   up=vec3(0,1,0))

  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################
    sceneparams = VarMap() 
    sceneparams.preset = "ForwardPBR"
    sceneparams.SkyboxIntensity = float(1.5)
    sceneparams.SpecularIntensity = float(1.0)
    sceneparams.DiffuseIntensity = float(1.0)
    sceneparams.AmbientLight = vec3(0.0)
    sceneparams.DepthFogDistance = float(1e6)
    sceneparams.SkyboxTexPathStr = "src://envmaps/tozenv_caustic1.png"

    sceneparams.SSAONumSamples = SSAO_NUM_SAMPLES
    sceneparams.SSAONumSteps = 4
    sceneparams.SSAOBias = -1e-5
    sceneparams.SSAORadius = 4.0*2.54/100
    sceneparams.SSAOWeight = 1.0
    sceneparams.SSAOPower = 2.0

    ###################################
    # post fx node
    ###################################
    if False:
      postNode = PostFxNodeHSVG()
      postNode.gpuInit(ctx,8,8);
      postNode.hue = 0.0
      postNode.saturation = 0.5
      postNode.value = 2
      postNode.gamma = 0.7
    else:
      postNode = PostFxNodeDecompBlur()
      postNode.threshold = 0.99
      postNode.blurwidth = 8.0
      postNode.blurfactor = 0.15
      postNode.amount = 0.1
      postNode.gpuInit(ctx,8,8);
    postNode.addToSceneVars(sceneparams,"PostFxChain")
    self.post_node = postNode
    ###################################
    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]
    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useFloatColorBuffer = True
    ###################################
    # create particle drawable 
    ###################################
    self.ptc_systems = gen_psys_set(self.scene,
                                    self.layer_fwd)
    #######################################
    gmtl = PBRMaterial() 
    white = Image.createFromFile("src://effect_textures/white_64.dds")
    normal = Image.createFromFile("src://effect_textures/default_normal.dds")
    gmtl.assignImages(
      ctx,
      color = white,
      normal = normal,
      mtlruf = white,
      doConform=True
    )
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.baseColor = vec4(0.8,0.8,1.3,1)
    gmtl.doubleSided = True
    gmtl.gpuInit(ctx)
    gdata = GroundPlaneDrawableData()
    gdata.pbrmaterial = gmtl
    gdata.extent = 1000.0
    self.gdata = gdata
    self.drawable_ground = gdata.createSGDrawable(self.scene)
    self.groundnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"partgroundicle-node",self.drawable_ground)
    self.groundnode.worldTransform.translation = vec3(0,-5,0)
    #######################################
    self.model = XgmModel("data://tests/misc_gltf_samples/DamagedHelmet.glb")
    self.drawable_model = self.model.createDrawable()
    self.modelnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 35
    self.modelnode.worldTransform.translation = vec3(0,28,0)
    #######################################


  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

    amp = 0.0 # some lighting bugs when amp > 0
    pos = vec3(0,20+math.sin(updinfo.absolutetime*3)*amp,0)
    self.modelnode.worldTransform.translation = pos

    for item in self.ptc_systems:
      prv_trans = item.particlenode.worldTransform.translation
      f = - item.frq
      x = item.radius*math.cos(updinfo.absolutetime*f)
      y = 30+math.sin(updinfo.absolutetime*f)*30
      z = item.radius*math.sin(updinfo.absolutetime*f)*-1.0
      
      new_trans = vec3(x,y,z)

      #delta_dir = (new_trans-prv_trans).normalized
      #item.EMITN.inputs.Offset = new_trans
      #item.EMITR.inputs.Offset = new_trans
      #item.PNTA.inputs.position = new_trans
      
      item.particlenode.worldTransform.translation = new_trans
    
  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

###############################################################################

def sig_handler(signal_received, frame):
  print('SIGINT or CTRL-C detected. Exiting gracefully')
  sys.exit(0)

###############################################################################

signal(SIGINT, sig_handler)

ParticlesApp().ezapp.mainThreadLoop()
