#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random, numpy, argparse
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')
parser.add_argument('--dynaplugs', action="store_true", help='dynamic plug update' )

args = vars(parser.parse_args())
dynaplugs = args["dynaplugs"]

def createParticleData( use_streaks = True ):
  dflow = dataflow

  class ImplObject(object):
    def __init__(self):
      super().__init__()
      # create a dataflow graph
      self.graphdata = dflow.GraphData.createShared()

      # instantiate modules

      self.ptc_pool   = self.graphdata.create("POOL",particles.Pool)
      self.emitter    = self.graphdata.create("EMITN",particles.RingEmitter)
      self.globals    = self.graphdata.create("GLOB",particles.Globals)
      self.turbulence = self.graphdata.create("TURB",particles.Turbulence)
      self.sphereatr = self.graphdata.create("SPHR",particles.SphAttractor)

      self.streaks    = self.graphdata.create("STRK",particles.StreakRenderer)

      self.ptc_pool.pool_size = 16384 # max number of particles in pool

      # connect modules in a chain configuration

      self.graphdata.connect( self.emitter.inputs.pool,    self.ptc_pool.outputs.pool )
      self.graphdata.connect( self.turbulence.inputs.pool, self.emitter.outputs.pool )
      self.graphdata.connect( self.sphereatr.inputs.pool,     self.turbulence.outputs.pool )
      self.graphdata.connect( self.streaks.inputs.pool,    self.sphereatr.outputs.pool )

      self.streaks.inputs.Length = .03
      self.streaks.inputs.Width = .01
      
      self.emitter.inputs.LifeSpan = 1
      self.emitter.inputs.EmissionRate = 1000
      self.emitter.inputs.EmissionRadius = 0.1
      self.emitter.inputs.EmitterSpinRate = 5
      self.emitter.inputs.EmissionVelocity = 5
      self.emitter.inputs.DispersionAngle = 180
      self.emitter.inputs.Offset = vec3(0,0,0)
      self.emitter.inputs.Direction = vec3(0,0,0)

      self.sphereatr.inputs.Radius = 1
      self.sphereatr.inputs.Inertia = 1/1.0
      self.sphereatr.inputs.Center = vec3(0,0,0)
      self.sphereatr.inputs.Dampening = 0.999

      self.turbulence.inputs.Amount = vec3(1000)


      # create and return ParticlesDrawableData object

      self.drawable_data = ParticlesDrawableData()
      self.drawable_data.graphdata = self.graphdata

  return ImplObject()


################################################################################

class ParticlesApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera( app=self, 
                   eye = vec3(0.5,-0.2,-2.5).normalized*15, 
                   constrainZ=True, 
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

    createSceneGraph(app=self,rendermodel="ForwardPBR")

    ###################################
    # create particle data 
    ###################################

    self.ptc_data = createParticleData()

    self.emitterplugs = self.ptc_data.emitter.inputs
    self.turbulenceplugs = self.ptc_data.turbulence.inputs

    self.material = particles.GradientMaterial.createShared();
    self.material.blending = tokens.ADDITIVE
    self.material.gradient.setColorStops({
      0.0:vec4(0,0,0,1),
      #0.8:vec4(0,0,0,1),
      0.4:vec4(0,0,0,1),
      0.5:vec4(1,1,.7,1),
      0.7:vec4(0,0,0,1),
      1.0:vec4(0,0,0,1)
    })
    
    self.material2 = particles.TextureMaterial.createShared();
    self.material2.texture = Texture.load("src://effect_textures/spinner");

    ##################
    # create particle sg node
    ##################

    ptc_drawable = self.ptc_data.drawable_data.createDrawable()
    self.particlenode = self.layer1.createDrawableNode("particle-node",ptc_drawable)
    self.particlenode.sortkey = 2;
    self.color = vec4(1, 1, 0, 1)
    self.counter = 0.0

  ################################################
  def configA(self,abstime):
    self.turbulenceplugs.Amount = vec3(0,0,0)
    self.ptc_data.streaks.material = self.material
    if self.counter>1:
      self.tgt_size = random.uniform(4,8)
      self.counter = 0.0

  ################################################

  def onUpdate(self,updinfo):
    abstime = updinfo.absolutetime
    self.counter += updinfo.deltatime

    self.material.color = self.color

    ##########################################

    self.configA(abstime)

    ########################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    ########################################

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()
###############################################################################

ParticlesApp().ezapp.mainThreadLoop()
