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

################################################################################

tokens = CrcStringProxy()

################################################################################

class NODE(object):

  def __init__(self,model,layer, index):

    super().__init__()
    self.model = model
    self.sgnode = model.createNode("node%d"%index,layer)
    self.modelinst = self.sgnode.user.pyext_retain_modelinst
    self.sgnode.worldTransform.scale = 1

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
      self.emitter    = self.graphdata.create("EMITN",particles.EllipticalEmitter)
      self.globals    = self.graphdata.create("GLOB",particles.Globals)
      self.turbulence = self.graphdata.create("TURB",particles.Turbulence)
      self.elliptical = self.graphdata.create("SPHR",particles.EllipticalAttractor)
      self.gravity = self.graphdata.create("GRAV",particles.Gravity)

      self.streaks       = self.graphdata.create("STRK",particles.StreakRenderer)
 
      self.ptc_pool.pool_size = 16384 # max number of particles in pool

      # connect modules in a chain configuration

      self.graphdata.connect( self.emitter.inputs.pool,    self.ptc_pool.outputs.pool )
      self.graphdata.connect( self.turbulence.inputs.pool, self.emitter.outputs.pool )
      self.graphdata.connect( self.elliptical.inputs.pool,     self.turbulence.outputs.pool )
      self.graphdata.connect( self.gravity.inputs.pool,     self.elliptical.outputs.pool )
      self.graphdata.connect( self.streaks.inputs.pool,    self.gravity.outputs.pool )

      self.streaks.inputs.Length = .02
      self.streaks.inputs.Width = .05
      
      self.emitter.inputs.LifeSpan = 1
      self.emitter.inputs.EmissionRate = 250
      self.emitter.inputs.EmissionVelocity = 0.1
      #self.emitter.inputs.DispersionAngle = 180
      #self.emitter.inputs.Offset = vec3(0,0,0)

      self.gravity.inputs.G = 0.01
      self.gravity.inputs.Mass = 1
      self.gravity.inputs.OthMass = 1
      self.gravity.inputs.MinDistance = 1

      self.elliptical.inputs.Inertia = 1/100.0
      self.elliptical.inputs.P1 = vec3(0,1,0)
      self.elliptical.inputs.P2 = vec3(0,-1,.1)
      self.elliptical.inputs.Dampening = 0.999

      self.turbulence.inputs.Amount = vec3(99)

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

    params_dict = {
      "SkyboxIntensity": float(1),
      "SpecularIntensity": float(10),
      "DiffuseIntensity": float(10),
      "AmbientLight": vec3(0.1),
      "DepthFogDistance": float(10000)
    }

    createSceneGraph(app=self,rendermodel="ForwardPBR",params_dict=params_dict)

    self.model = XgmModel("data://tests/pbr_calib.glb")
    self.nodeP1 = NODE(self.model,self.layer1,0)
    self.nodeP2 = NODE(self.model,self.layer1,1)

    ###################################
    # create particle data 
    ###################################

    self.ptc_data = createParticleData()

    self.emitterplugs = self.ptc_data.emitter.inputs
    self.turbulenceplugs = self.ptc_data.turbulence.inputs

    self.material = particles.GradientMaterial.createShared();
    self.material.blending = tokens.ADDITIVE
    self.material.gradient.setColorStops({
      #0.0:vec4(1,1,1,1),
      0.0:vec4(0,0,0,1),
      0.2:vec4(0,0,0,1),
      0.5:vec4(1,1,.7,1),
      0.7:vec4(1,1,1,1),
      1.0:vec4(0,0,0,1)
      #1.0:vec4(1,1,1,1)
    })
    self.material.modulation_texture = Texture.load("src://effect_textures/knob2");

    self.material2 = particles.TextureMaterial.createShared();
    self.material2.texture = Texture.load("src://effect_textures/spinner");

    self.ptc_data.streaks.material = self.material

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
    if self.counter>1:
      self.tgt_size = random.uniform(4,8)
      self.counter = 0.0

  ################################################

  def onUpdate(self,updinfo):
    abstime = updinfo.absolutetime
    self.counter += updinfo.deltatime

    self.material.color = self.color


    if hasattr(self.ptc_data,"elliptical"):
      T = abstime
      P1 = vec3(0,math.sin(T)+1.0,0)
      P2 = vec3(0,math.sin(T+math.pi)-1.0,0)
      
      
      self.nodeP1.sgnode.worldTransform.translation = P1
      self.nodeP1.sgnode.worldTransform.scale = 0.1
      self.nodeP2.sgnode.worldTransform.translation = P2
      self.nodeP2.sgnode.worldTransform.scale = 0.1
      
      self.ptc_data.elliptical.inputs.P1 = P1
      self.ptc_data.elliptical.inputs.P2 = P2
      DY = (P1-P2).normalized
      DX = DY.cross(vec3(0,1,1)).normalized
      DZ = DX.cross(DY).normalized
      #DX = DZ.cross(DY).normalized
            
      EMI = self.ptc_data.emitter.inputs
      EMI.P1 = P1
      EMI.P2 = P2
      EMI.EmissionVelocity = 0.1
      EMI.DispersionAngle = 180
      EMI.LifeSpan = 2.5
      EMI.EmissionRate = 5000

      ELI = self.ptc_data.elliptical.inputs
      ELI.Inertia = 1e6
      
      GRV = self.ptc_data.gravity.inputs
      GRV.Center = P2
      GRV.G = 1
      GRV.MinDistance = 10

      STR = self.ptc_data.streaks.inputs
      STR.Length = 1
      STR.Width = .025

      TRB = self.ptc_data.turbulence.inputs
      TRB.Amount = vec3(0)

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
