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
sys.path.append(str(thisdir())) # add parent dir to path
from common.cameras import *
from common.scenegraph import createSceneGraph

tokens = CrcStringProxy()

floatxf = dataflow.floatxf   

################################################

gradients = [GradientV4() for i in range(8)]

gradients[0].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(1,0,0,1),
  0.25: vec4(1,0,1,1),
  0.5: vec4(1,1,1,1),
  1.0: vec4(0,0,0,0),
})
gradients[1].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(1,0,0,1),
  0.25: vec4(1,0.8,0,1),
  0.5: vec4(1,1,0.5,1),
  1.0: vec4(0,0,0,0),
})
gradients[2].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(1,0,0,1),
  0.35: vec4(1,0.8,0,1),
  0.5: vec4(1,0,0,1),
  1.0: vec4(0,0,0,0),
})
gradients[3].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(0,0,1,1),
  0.35: vec4(0,0.8,1,1),
  0.5: vec4(0,0,1,1),
  1.0: vec4(0,0,0,0),
})
gradients[4].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(0,1,1,1),
  0.35: vec4(.5,1,1,1),
  0.5: vec4(0,1,1,1),
  1.0: vec4(0,0,0,0),
})
gradients[5].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(0,1,0,1),
  0.35: vec4(0,1,0,1),
  0.5: vec4(0,1,0,1),
  1.0: vec4(0,0,0,0),
})
gradients[6].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(1,1,1,1),
  0.35: vec4(1,1,1,1),
  0.5: vec4(1,1,1,1),
  1.0: vec4(0,0,0,0),
})
gradients[7].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(1,1,.5,1),
  0.35: vec4(1,1,.5,1),
  0.5: vec4(1,1,.5,1),
  1.0: vec4(0,0,0,0),
})

def presetGRAD(index):
  return gradients[index]  

################################################

def presetMaterial(grad=presetGRAD(0),texname="src://effect_textures/knob2"):
  material = particles.GradientMaterial.createShared()
  material.modulation_texture = Texture.load(texname)
  material.gradient = grad
  material.blending = tokens.ADDITIVE
  material.depthtest = tokens.LEQUALS
  return material

################################################

def presetTURB1(turbulence):
  turbulence.inputs.Amount = vec3(1.5,1.5,1.5)

################################################

def presetVORT1(vortex):
  vortex.inputs.VortexStrength = 1.0
  vortex.inputs.OutwardStrength = 1.0
  vortex.inputs.Falloff = 1.0

################################################

def presetGRAV1(gravity):
  gravity.inputs.G = 1
  gravity.inputs.Mass = 1
  gravity.inputs.OthMass = 1
  gravity.inputs.MinDistance = 1
  gravity.inputs.Center = vec3(0,0,0)

################################################

def presetEMITN1(emitter):
  emitter.inputs.LifeSpan = 10
  emitter.inputs.EmissionRate = 800
  emitter.inputs.EmissionVelocity = 1
  emitter.inputs.DispersionAngle = 45
  emitter.inputs.Offset = vec3(1,2,3)
  
################################################

def presetEMITR1(emitter):
  emitter.inputs.LifeSpan = 10
  emitter.inputs.EmissionRate = 800
  emitter.inputs.EmissionRadius = 2
  emitter.inputs.EmitterSpinRate = 1
  emitter.inputs.EmissionVelocity = 1
  emitter.inputs.DispersionAngle = 45
  emitter.inputs.Offset = vec3(0,4,0)
  
################################################

def presetEMITL1(emitter):
  emitter.inputs.LifeSpan = 10
  emitter.inputs.EmissionRate = 800
  emitter.inputs.EmissionVelocity = 1
  emitter.inputs.DispersionAngle = 45
  emitter.inputs.P1 = vec3(0,0,0)
  emitter.inputs.P2 = vec3(0,10,0)
  
################################################

def presetPOOL1(ptc_pool):
  ptc_pool.pool_size = 16384 # max number of particles in pool
  
################################################

class ParticleContainer:
  def __init__(self,scene,layer):
    self.graphdata = None
    self.drawable_data = None
    self.particlenode = None
    self.scene = scene
    self.layer = layer

################################################

def createParticleData( owner, 
                        ptc_data,
                        ptc_connections,
                        layer):

    owner.graphdata = dataflow.GraphData.createShared()

    # instantiate modules

    for name in ptc_data.keys():
      typ = ptc_data[name]
      owner.__dict__[name] = owner.graphdata.create(name,typ)
      
    for item in ptc_connections:
      n0 = item[0]
      n1 = item[1]
      owner.graphdata.connect( owner.__dict__[n1].inputs.pool, owner.__dict__[n0].outputs.pool )
    
    SG = owner.scene
    #LMD = SG.lightingmanager
    owner.drawable_data = ParticlesDrawableData()
    owner.drawable_data.graphdata = owner.graphdata
    ptc_drawable = owner.drawable_data.createSGDrawable(SG)
        
    owner.particlenode = layer.createDrawableNode("particle-node",ptc_drawable)

    #owner.particlenode.sortkey = 2;


