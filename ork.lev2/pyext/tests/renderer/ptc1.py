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
sys.path.append(str(path.orkid()/"ork.lev2"/"examples"/"python")) # add parent dir to path
from common.cameras import *
from common.scenegraph import createSceneGraph

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

def createParticleData():

  class ImplObject(object):
    def __init__(self):
      super().__init__()
      # create a dataflow graph
      self.graphdata = dataflow.GraphData.createShared()

      # instantiate modules

      self.ptc_pool   = self.graphdata.create("POOL",particles.Pool)
      self.emitter    = self.graphdata.create("EMITN",particles.NozzleEmitter)
      self.emitter2    = self.graphdata.create("EMITR",particles.RingEmitter)
      self.globals    = self.graphdata.create("GLOB",particles.Globals)
      self.gravity    = self.graphdata.create("GRAV",particles.Gravity)
      self.turbulence = self.graphdata.create("TURB",particles.Turbulence)
      self.vortex     = self.graphdata.create("VORT",particles.Vortex)

      self.streaks    = self.graphdata.create("STRK",particles.StreakRenderer)

      self.ptc_pool.pool_size = 16384 # max number of particles in pool

      # connect modules in a chain configuration

      self.graphdata.connect( self.emitter.inputs.pool,    self.ptc_pool.outputs.pool )
      self.graphdata.connect( self.emitter2.inputs.pool,    self.emitter.outputs.pool )
      self.graphdata.connect( self.gravity.inputs.pool,    self.emitter2.outputs.pool )
      self.graphdata.connect( self.turbulence.inputs.pool, self.gravity.outputs.pool )
      self.graphdata.connect( self.vortex.inputs.pool,     self.turbulence.outputs.pool )

      self.graphdata.connect( self.streaks.inputs.pool,    self.vortex.outputs.pool )
      self.streaks.inputs.Length = .1
      self.streaks.inputs.Width = .01

      # emitter module settings

      self.emitter.inputs.LifeSpan = 10
      self.emitter.inputs.EmissionRate = 800
      self.emitter.inputs.EmissionVelocity = 1
      self.emitter.inputs.DispersionAngle = 45
      self.emitter.inputs.Offset = vec3(1,2,3)

      self.emitter2.inputs.LifeSpan = 10
      self.emitter2.inputs.EmissionRate = 800
      self.emitter2.inputs.EmissionRadius = 2
      self.emitter2.inputs.EmitterSpinRate = 1
      self.emitter2.inputs.EmissionVelocity = 1
      self.emitter2.inputs.DispersionAngle = 45
      self.emitter2.inputs.Offset = vec3(0,4,0)

      # gravity module settings

      self.gravity.inputs.G = 1
      self.gravity.inputs.Mass = 1
      self.gravity.inputs.OthMass = 1
      self.gravity.inputs.MinDistance = 1
      self.gravity.inputs.Center = vec3(0,0,0)

      # turbulence module settings

      self.turbulence.inputs.Amount = vec3(1.5,1.5,1.5)

      # vortex module settins

      self.vortex.inputs.VortexStrength = 1.0
      self.vortex.inputs.OutwardStrength = 1.0
      self.vortex.inputs.Falloff = 1.0

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
    setupUiCamera( app=self, eye = vec3(0,0,30), constrainZ=True, up=vec3(0,1,0))

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
    # create particle drawable 
    ###################################

    self.ptc_data = createParticleData()
    ptc_drawable = self.ptc_data.drawable_data.createDrawable()

    self.emitterplugs = self.ptc_data.emitter.inputs
    self.vortexplugs = self.ptc_data.vortex.inputs
    self.gravityplugs = self.ptc_data.gravity.inputs
    self.turbulenceplugs = self.ptc_data.turbulence.inputs

    self.emitterplugs.EmissionVelocity = 0.1
    self.turbulenceplugs.Amount = vec3(1,1,1)*5

    ##################
    # create particle sg node
    ##################

    self.particlenode = self.layer1.createDrawableNode("particle-node",ptc_drawable)
    self.particlenode.sortkey = 2;


  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()
###############################################################################

ParticlesApp().ezapp.mainThreadLoop()
