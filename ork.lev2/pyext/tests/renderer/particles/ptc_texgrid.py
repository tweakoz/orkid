#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random, numpy, argparse, signal
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
lev2_pyexdir.addToSysPath()
from lev2utils.scenegraph import createSceneGraph
from _ptc_harness import *

################################################################################

tokens = CrcStringProxy()
dflow = dataflow

################################################################################

class EllipticalParticleSystem(object):
  def __init__(self,app):
    super().__init__()
    self.app = app
    # create a dataflow graph
    self.graphdata = dflow.GraphData.createShared()

    # instantiate modules

    self.ptc_pool   = self.graphdata.create("POOL",particles.Pool)
    self.emitter    = self.graphdata.create("EMITN",particles.EllipticalEmitter)
    self.globals    = self.graphdata.create("GLOB",particles.Globals)
    self.turbulence = self.graphdata.create("TURB",particles.Turbulence)
    self.gravity = self.graphdata.create("GRAV",particles.Gravity)

    self.sprites       = self.graphdata.create("SPRI",particles.SpriteRenderer)

    self.ptc_pool.pool_size = 15000 # max number of particles in pool

    # connect modules in a chain configuration

    self.graphdata.connect( self.emitter.inputs.pool,    self.ptc_pool.outputs.pool )
    self.graphdata.connect( self.turbulence.inputs.pool, self.emitter.outputs.pool )
    self.graphdata.connect( self.gravity.inputs.pool,     self.turbulence.outputs.pool )
    self.graphdata.connect( self.sprites.inputs.pool,    self.gravity.outputs.pool )
    
    self.emitter.inputs.LifeSpan = 1
    self.emitter.inputs.EmissionRate = 250
    self.emitter.inputs.EmissionVelocity = 0.1
    self.emitter.inputs.MinU = 0
    self.emitter.inputs.MaxU = 1
    self.emitter.inputs.MinV = 0
    self.emitter.inputs.MaxV = 1

    self.gravity.inputs.G = 0.01
    self.gravity.inputs.Mass = 1
    self.gravity.inputs.OthMass = 1
    self.gravity.inputs.MinDistance = 1

    self.turbulence.inputs.Amount = vec3(1,1,1)

    self.drawable_data = ParticlesDrawableData()
    self.drawable_data.graphdata = self.graphdata

    self.material = particles.TexGridMaterial.createShared();
    self.material.color = vec4(2.0,0.7,.5,0.1)
    self.material.blending = tokens.ALPHA_ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.texture = Texture.load("lev2://textures/exp2.dds");
    self.material.gridDim = 5
    self.sprites.material = self.material
    self.sprites.depth_sort = True

    ##################
    # create particle sg node
    ##################

    ptc_drawable = self.drawable_data.createDrawable()
    self.particlenode = self.app.layer1.createDrawableNode("particle-node",ptc_drawable)
    self.particlenode.sortkey = 1;
    self.particlenode.worldTransform.translation = vec3(0,1,0)
    self.color = vec4(1, 1, 0, 1)
    self.counter = 0.0
    self.lerp = 0.0

  ################################################

  def onUpdate(self,updinfo):
    abstime = updinfo.absolutetime
    self.counter += updinfo.deltatime
    #self.material.color = self.color


    if hasattr(self,"emitter"):
      T = abstime
      
      f = 2
      
      EMI = self.emitter.inputs
      EMI.P1 = vec3(0,f,0)
      EMI.P2 = vec3(0,-f,0)
      EMI.EmissionVelocity = 5
      EMI.DispersionAngle = 0
      EMI.LifeSpan = 0.7
      EMI.EmissionRate = 2500
      EMI.Scalar = 1
      
      GRV = self.gravity.inputs
      GRV.Center = vec3(0,0,0)
      GRV.G = 0
      GRV.MinDistance = 10

      RENDERER = self.sprites.inputs
      RENDERER.Size = 1

      TRB = self.turbulence.inputs
      TRB.Amount = vec3(10)

################################################################################

class ParticlesApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=2)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)

    setupUiCamera( app=self, #
                   eye = vec3(0,0,30), #
                   constrainZ=True, #
                   up=vec3(0,1,0))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ################################################

  def onGpuInit(self,ctx):
    createSceneGraph(app=self,rendermodel="ForwardPBR")
    self.ptc = EllipticalParticleSystem(self)

  def onGpuUpdate(self,ctx):
    # just need a mainthread python callback
    # so python can process ctrl-c signals...
    pass 

  ################################################

  def onUpdate(self,updinfo):
    self.ptc.lerp = smooth_step(0.45,0.55,math.sin(updinfo.absolutetime*1)*0.5+0.5)
    self.ptc.onUpdate(updinfo)
    self.scene.updateScene(self.cameralut) 
    
  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

###############################################################################

ParticlesApp().ezapp.mainThreadLoop()
