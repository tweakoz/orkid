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

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

################################################################################

class ParticlesApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=1,fullscreen=True)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)

    setupUiCamera( app=self, #
                   eye = vec3(0,0,30), #
                   constrainZ=True, #
                   up=vec3(0,1,0))
    
    self.pending_timer = 2.0
    self.P1 = vec3(0,1,0)
    self.P2 = vec3(0,-1,0)
    self.elev_cur = 0.0
    self.elev_tgt = 1
    self.azim_cur = 0.0
    self.azim_tgt = 1
    self.scale = 2.0
    self.counter = 0

  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # scene params
    ###################################

    sceneparams = VarMap() 
    sceneparams.preset = "ForwardPBR"
    sceneparams.SkyboxIntensity = float(0.5)
    sceneparams.SpecularIntensity = float(1.2)
    sceneparams.DiffuseIntensity = float(1.0)
    sceneparams.AmbientLight = vec3(0.0)
    sceneparams.DepthFogDistance = float(1e6)
    #sceneparams.SkyboxTexPathStr = "src://envmaps/tozenv_caustic1.png"

    ###################################
    # post fx node
    ###################################

    postNode = PostFxNodeDecompBlur()
    postNode.threshold = 0.99
    postNode.blurwidth = 8
    postNode.blurfactor = 0.15
    postNode.amount = 0.5
    postNode.gpuInit(ctx,8,8);
    postNode.addToSceneVars(sceneparams,"PostFxChain")

    ###################################
    # create scenegraph
    ###################################

    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_fwd = self.scene.createLayer("std_forward")
    #createSceneGraph(app=self,rendermodel="ForwardPBR")

    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useFloatColorBuffer = True

    ###################################
    # create particle drawable 
    ###################################

    ptc_data = {
      "POOL":particles.Pool,
      "EMITL":particles.LineEmitter,
      "GRAV":particles.Gravity,
      "TURB":particles.Turbulence,
      "STRK":particles.StreakRenderer,
    }
    ptc_connections = [
      ("POOL","EMITL"),
      ("EMITL","TURB"),
      ("TURB","GRAV"),
      ("GRAV","STRK"),
    ]
    createParticleData(self,ptc_data,ptc_connections,self.layer_fwd)
    self.POOL.pool_size = 131072 # max number of particles in pool

    gradient = GradientV4()
    gradient.setColorStops({
      0.0: vec4(0,0,0,0),
      0.25: vec4(1,0,0,1),
      0.25: vec4(1,0.5,0.8,1),
      0.5: vec4(1,0.5,1,1),
      0.7: vec4(0.5,1,0,1),
      0.9: vec4(0,1,0,1),
      1.0: vec4(0,0,0,0),
    })

    self.STRK.inputs.Length = 1.5
    self.STRK.inputs.Width = 0.03
    self.STRK.inputs.GradientIntensity = 1
    self.STRK.material = presetMaterial(grad=gradient,texname="src://effect_textures/ptc3")
    presetPOOL1(self.POOL)
    presetEMITL1(self.EMITL)
    presetGRAV1(self.GRAV)
    presetTURB1(self.TURB)
    self.EMITL.inputs.LifeSpan = 20
    self.EMITL.inputs.EmissionRate = 3000
    self.EMITL.inputs.EmissionVelocity = 0.1
    self.GRAV.inputs.G = 1e-3
    self.GRAV.inputs.Mass = 1e-5
    self.GRAV.inputs.OthMass = 1e-5
    self.GRAV.inputs.MinDistance = 1

    self.TURB.inputs.Amount = vec3(0.5)

  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.pending_timer -= updinfo.deltatime

    self.counter = self.counter + 1

    cubeP1 = vec3(1,1,-1) # top right back
    cubeP2 = vec3(1,1,1) # top right front
    cubeP3 = vec3(-1,1,1) # top left front
    cubeP4 = vec3(-1,1,-1) # top left back
    cubeP5 = vec3(1,-1,-1) # bottom right back
    cubeP6 = vec3(1,-1,1) # bottom right front
    cubeP7 = vec3(-1,-1,1) # bottom left front
    cubeP8 = vec3(-1,-1,-1) # bottom left back
    
    Qelev = quat(vec3(1,0,0),self.elev_cur)
    Qazim = quat(vec3(0,1,0),self.azim_cur)
    Q = Qelev * Qazim
    R = mtx4(Q)
    
    cubeP1 = vec4(cubeP1,0).transform(R).xyz
    cubeP2 = vec4(cubeP2,0).transform(R).xyz
    cubeP3 = vec4(cubeP3,0).transform(R).xyz
    cubeP4 = vec4(cubeP4,0).transform(R).xyz
    cubeP5 = vec4(cubeP5,0).transform(R).xyz
    cubeP6 = vec4(cubeP6,0).transform(R).xyz
    cubeP7 = vec4(cubeP7,0).transform(R).xyz
    cubeP8 = vec4(cubeP8,0).transform(R).xyz

    rand = (self.counter>>2) % 12
    if rand==0: # top left back to top right back
      self.P1 = cubeP4
      self.P2 = cubeP1
    elif rand==1: # top right back to top right front
      self.P1 = cubeP1
      self.P2 = cubeP2
    elif rand==2: # top right front to top left front
      self.P1 = cubeP2
      self.P2 = cubeP3
    elif rand==3: # top left front to top left back
      self.P1 = cubeP3
      self.P2 = cubeP4
    elif rand==4: # top left back to bottom left back
      self.P1 = cubeP4
      self.P2 = cubeP8
    elif rand==5: # top right back to bottom right back
      self.P1 = cubeP1
      self.P2 = cubeP5
    elif rand==6: # top right front to bottom right front
      self.P1 = cubeP2
      self.P2 = cubeP6
    elif rand==7: # top left front to bottom left front
      self.P1 = cubeP3
      self.P2 = cubeP7
    elif rand==8: # bottom left back to bottom right back
      self.P1 = cubeP8
      self.P2 = cubeP5
    elif rand==9: # bottom right back to bottom right front
      self.P1 = cubeP5
      self.P2 = cubeP6
    elif rand==10: # bottom right front to bottom left front
      self.P1 = cubeP6
      self.P2 = cubeP7
    elif rand==11: # bottom left front to bottom left back
      self.P1 = cubeP7
      self.P2 = cubeP8

    self.EMITL.inputs.P1 = self.P1*self.scale
    self.EMITL.inputs.P2 = self.P2*self.scale

    if self.pending_timer<0.0:
      self.elev_cur = random.uniform(-math.pi,math.pi)
      self.azim_cur = random.uniform(-math.pi,math.pi)
      
      # compute 6 points on a cube (rotated by elev/azim)
            
      self.pending_timer = random.uniform(2,3)
      self.scale = random.uniform(2,4)
      range = 20

      

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
