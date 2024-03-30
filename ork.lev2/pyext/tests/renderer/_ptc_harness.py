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

def presetPOOL1(ptc_pool):
  ptc_pool.pool_size = 16384 # max number of particles in pool
  
################################################

def createParticleData( owner, ptc_data, ptc_connections ):

    owner.graphdata = dataflow.GraphData.createShared()

    # instantiate modules

    for name in ptc_data.keys():
      typ = ptc_data[name]
      owner.__dict__[name] = owner.graphdata.create(name,typ)
      
    for item in ptc_connections:
      n0 = item[0]
      n1 = item[1]
      owner.graphdata.connect( owner.__dict__[n1].inputs.pool, owner.__dict__[n0].outputs.pool )
    
    owner.drawable_data = ParticlesDrawableData()
    owner.drawable_data.graphdata = owner.graphdata
    ptc_drawable = owner.drawable_data.createDrawable()
    owner.particlenode = owner.layer1.createDrawableNode("particle-node",ptc_drawable)
    owner.particlenode.sortkey = 2;


