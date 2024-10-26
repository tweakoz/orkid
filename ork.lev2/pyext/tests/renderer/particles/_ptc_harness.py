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
from lev2utils.cameras import *
from lev2utils.scenegraph import createSceneGraph
from lev2utils.primitives import presetMaterial, presetGRAD

tokens = CrcStringProxy()

floatxf = dataflow.floatxf   

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

  #######################################

def gen_sys(scene=None,
            layer=None,
            ptc_data=None,
            ptc_connections=None,
            grad=None,
            frq=None,
            radius=None):
  ptc = ParticleContainer(scene,layer)
  createParticleData(ptc,ptc_data,ptc_connections,layer)
  ptc.POOL.pool_size = 4096 # max number of particles in pool
  ptc.SPRI.inputs.Size = 0.1
  ptc.SPRI.inputs.GradientIntensity = 1
  ptc.SPRI.material = presetMaterial(grad=grad)
  ptc.EMITN.inputs.EmissionVelocity = 0.1
  #presetPOOL1(ptc.POOL)
  presetEMITN1(ptc.EMITN)
  presetEMITR1(ptc.EMITR)
  ptc.EMITN.inputs.EmissionRate = 50
  ptc.EMITR.inputs.EmissionRate = 50
  ptc.EMITN.inputs.LifeSpan = 30
  ptc.EMITR.inputs.LifeSpan = 30
  presetTURB1(ptc.TURB)
  presetVORT1(ptc.VORT)
  ptc.VORT.inputs.VortexStrength = 0.0
  ptc.VORT.inputs.OutwardStrength = 0.0
  presetGRAV1(ptc.GRAV)
  ptc.particlenode.worldTransform.translation = vec3(50,10,0)    
  ptc.TURB.inputs.Amount = vec3(1,1,1)*5
  ptc.frq = frq
  ptc.radius = radius
  ptc.DRAG.inputs.drag = 0.999
  ptc.drawable_data.emitterIntensity = 8.0
  ptc.drawable_data.emitterRadius = 1.5
  return ptc

def gen_psys_set(scene,
                 layer,
                 count=32,
                 frqbase=0.4,
                 radbase=1.0):
  ptc_data = {
    "POOL":particles.Pool,
    "EMITN":particles.NozzleEmitter,
    "EMITR":particles.RingEmitter,
    "GLOB":particles.Globals,
    #"PNTA":particles.PointAttractor,
    "GRAV":particles.Gravity,
    "TURB":particles.Turbulence,
    "VORT":particles.Vortex,
    "DRAG":particles.Drag,
    "LITE":particles.LightRenderer,
    "SPRI":particles.SpriteRenderer,
  }
  ptc_connections = [
    ("POOL","EMITN"),
    ("EMITN","EMITR"),
    ("EMITR","GRAV"),
    ("GRAV","TURB"),
    ("TURB","VORT"),
    ("VORT","DRAG"),
    ("DRAG","LITE"),
    ("LITE","SPRI"),
  ]
  ptc_systems = []
  for i in range(count):
    fi = float(i)/float(count)
    frq = frqbase + (fi*2)
    radius = (35 + fi*35)*radbase
    g = i&7
    ptc_systems += [gen_sys(scene=scene,
                            ptc_data=ptc_data,
                            ptc_connections=ptc_connections,
                            layer=layer,
                            grad=presetGRAD(g),
                            frq=frq,
                            radius=radius)]
  return ptc_systems

def update_psys_set(psys_set,time,yval):
  for item in psys_set:
    prv_trans = item.particlenode.worldTransform.translation
    f = - item.frq
    x = item.radius*math.cos(time*f)
    y = yval #30+math.sin(time*f)*30
    z = item.radius*math.sin(time*f)*-1.0
    new_trans = vec3(x,y,z)    
    item.particlenode.worldTransform.translation = new_trans  
    
    
def createDefaultStreakSystem(app=None):    
    ptc_data = {
      "POOL":particles.Pool,
      "EMITN":particles.NozzleEmitter,
      "EMITR":particles.RingEmitter,
      "GLOB":particles.Globals,
      "GRAV":particles.Gravity,
      "TURB":particles.Turbulence,
      "VORT":particles.Vortex,
      "STRK":particles.StreakRenderer,
    }
    ptc_connections = [
      ("POOL","EMITN"),
      ("EMITN","EMITR"),
      ("EMITR","GRAV"),
      ("GRAV","TURB"),
      ("TURB","VORT"),
      ("VORT","STRK"),
    ]
    createParticleData(app,ptc_data,ptc_connections,app.layer1)
    app.POOL.pool_size = 16384 # max number of particles in pool

    app.STRK.inputs.Length = .1
    app.STRK.inputs.Width = .01
    app.STRK.material = presetMaterial()
    #app.STRK.material = particles.FlatMaterial.createShared()
    app.EMITN.inputs.EmissionVelocity = 0.1
    presetPOOL1(app.POOL)
    presetEMITN1(app.EMITN)
    presetEMITR1(app.EMITR)
    presetTURB1(app.TURB)
    presetVORT1(app.VORT)
    presetGRAV1(app.GRAV)
    
    app.TURB.inputs.Amount = vec3(1,1,1)*5    
    
    
    
def createDefaultSpriteSystem(app=None):
    ptc_data = {
      "POOL":particles.Pool,
      "EMITN":particles.NozzleEmitter,
      "EMITR":particles.RingEmitter,
      "GLOB":particles.Globals,
      "GRAV":particles.Gravity,
      "TURB":particles.Turbulence,
      "VORT":particles.Vortex,
      "SPRI":particles.SpriteRenderer,
    }
    ptc_connections = [
      ("POOL","EMITN"),
      ("EMITN","EMITR"),
      ("EMITR","GRAV"),
      ("GRAV","TURB"),
      ("TURB","VORT"),
      ("VORT","SPRI"),
    ]
    createParticleData(app,ptc_data,ptc_connections,app.layer1)
    app.POOL.pool_size = 16384 # max number of particles in pool

    app.SPRI.inputs.Size = 0.1
    app.SPRI.inputs.GradientIntensity = 1
    app.SPRI.material = presetMaterial()
    #app.SPRI.material = particles.FlatMaterial.createShared()
    app.EMITN.inputs.EmissionVelocity = 0.1
    presetPOOL1(app.POOL)
    presetEMITN1(app.EMITN)
    presetEMITR1(app.EMITR)
    presetTURB1(app.TURB)
    presetVORT1(app.VORT)
    presetGRAV1(app.GRAV)
    
    app.TURB.inputs.Amount = vec3(1,1,1)*5  