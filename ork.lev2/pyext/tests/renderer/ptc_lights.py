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
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)

    #self.materials = set()

    setupUiCamera( app=self, #
                   eye = vec3(0,0,30), #
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

    params_dict = {
      "SkyboxIntensity": float(1),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.1),
      "DepthFogDistance": float(1e6),
      "SkyboxTexPathStr": "src://envmaps/tozenv_caustic1.png",
    }

    createSceneGraph(app=self,rendermodel="ForwardPBR",params_dict=params_dict)

    ###################################
    # create particle drawable 
    ###################################

    ptc_data = {
      "POOL":particles.Pool,
      "EMITN":particles.NozzleEmitter,
      "EMITR":particles.RingEmitter,
      "GLOB":particles.Globals,
      "GRAV":particles.Gravity,
      "TURB":particles.Turbulence,
      "VORT":particles.Vortex,
      "LITE":particles.LightRenderer,
      "SPRI":particles.SpriteRenderer,
    }
    ptc_connections = [
      ("POOL","EMITN"),
      ("EMITN","EMITR"),
      ("EMITR","GRAV"),
      ("GRAV","TURB"),
      ("TURB","VORT"),
      ("VORT","LITE"),
      ("LITE","SPRI"),
    ]
    
    #######################################
    self.ptc1 = ParticleContainer(self.scene,self.layer1)
    createParticleData(self.ptc1,ptc_data,ptc_connections)
    self.ptc1.POOL.pool_size = 8192 # max number of particles in pool
    self.ptc1.SPRI.inputs.Size = 0.1
    self.ptc1.SPRI.inputs.GradientIntensity = 1
    self.ptc1.SPRI.material = presetMaterial(grad=presetGRAD2())
    self.ptc1.EMITN.inputs.EmissionVelocity = 0.1
    presetPOOL1(self.ptc1.POOL)
    presetEMITN1(self.ptc1.EMITN)
    presetEMITR1(self.ptc1.EMITR)
    presetTURB1(self.ptc1.TURB)
    presetVORT1(self.ptc1.VORT)
    presetGRAV1(self.ptc1.GRAV)
    self.ptc1.particlenode.worldTransform.translation = vec3(50,10,0)    
    self.ptc1.TURB.inputs.Amount = vec3(1,1,1)*5
    #######################################
    self.ptc2 = ParticleContainer(self.scene,self.layer1)
    createParticleData(self.ptc2,ptc_data,ptc_connections)
    self.ptc2.POOL.pool_size = 8192 # max number of particles in pool
    self.ptc2.SPRI.inputs.Size = 0.1
    self.ptc2.SPRI.inputs.GradientIntensity = 1
    self.ptc2.SPRI.material = presetMaterial(grad=presetGRAD1())
    self.ptc2.EMITN.inputs.EmissionVelocity = 0.1
    presetPOOL1(self.ptc2.POOL)
    presetEMITN1(self.ptc2.EMITN)
    presetEMITR1(self.ptc2.EMITR)
    presetTURB1(self.ptc2.TURB)
    presetVORT1(self.ptc2.VORT)
    presetGRAV1(self.ptc2.GRAV)
    self.ptc2.particlenode.worldTransform.translation = vec3(-50,10,0)    
    self.ptc2.TURB.inputs.Amount = vec3(1,1,1)*5
    #######################################
    self.ptc3 = ParticleContainer(self.scene,self.layer1)
    createParticleData(self.ptc3,ptc_data,ptc_connections)
    self.ptc3.POOL.pool_size = 8192 # max number of particles in pool
    self.ptc3.SPRI.inputs.Size = 0.1
    self.ptc3.SPRI.inputs.GradientIntensity = 1
    self.ptc3.SPRI.material = presetMaterial(grad=presetGRAD3())
    self.ptc3.EMITN.inputs.EmissionVelocity = 0.1
    presetPOOL1(self.ptc3.POOL)
    presetEMITN1(self.ptc3.EMITN)
    presetEMITR1(self.ptc3.EMITR)
    presetTURB1(self.ptc3.TURB)
    presetVORT1(self.ptc3.VORT)
    presetGRAV1(self.ptc3.GRAV)
    self.ptc3.particlenode.worldTransform.translation = vec3(0,10,50)    
    self.ptc3.TURB.inputs.Amount = vec3(1,1,1)*5
    #######################################
    self.ptc4 = ParticleContainer(self.scene,self.layer1)
    createParticleData(self.ptc4,ptc_data,ptc_connections)
    self.ptc4.POOL.pool_size = 8192 # max number of particles in pool
    self.ptc4.SPRI.inputs.Size = 0.1
    self.ptc4.SPRI.inputs.GradientIntensity = 1
    self.ptc4.SPRI.material = presetMaterial(grad=presetGRAD4())
    self.ptc4.EMITN.inputs.EmissionVelocity = 0.1
    presetPOOL1(self.ptc4.POOL)
    presetEMITN1(self.ptc4.EMITN)
    presetEMITR1(self.ptc4.EMITR)
    presetTURB1(self.ptc4.TURB)
    presetVORT1(self.ptc4.VORT)
    presetGRAV1(self.ptc4.GRAV)
    self.ptc4.particlenode.worldTransform.translation = vec3(0,10,-50)    
    self.ptc4.TURB.inputs.Amount = vec3(1,1,1)*5
    #######################################

    gmtl = PBRMaterial() 
    gmtl.texColor = Texture.load("src://effect_textures/white.dds")
    gmtl.texNormal = Texture.load("src://effect_textures/default_normal.dds")
    gmtl.texMtlRuf = Texture.load("src://effect_textures/white.dds")
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.baseColor = vec4(0.9,0.9,1,1)
    gmtl.gpuInit(ctx)
    gdata = GroundPlaneDrawableData()
    gdata.pbrmaterial = gmtl
    gdata.extent = 1000.0
    self.gdata = gdata
    self.drawable_ground = gdata.createSGDrawable(self.scene)
    self.groundnode = self.layer1.createDrawableNode("partgroundicle-node",self.drawable_ground)
    self.groundnode.worldTransform.translation = vec3(0,-5,0)


  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    h1 = 10+math.sin(updinfo.absolutetime*0.3)*10
    h2 = 10+math.sin(updinfo.absolutetime*0.43)*10
    h3 = 10+math.sin(updinfo.absolutetime*0.55)*10
    h4 = 10+math.sin(updinfo.absolutetime*0.67)*10
    self.ptc1.particlenode.worldTransform.translation = vec3(50,h1,0)
    self.ptc2.particlenode.worldTransform.translation = vec3(-50,h2,0)
    self.ptc3.particlenode.worldTransform.translation = vec3(0,h3,50)
    self.ptc4.particlenode.worldTransform.translation = vec3(0,h4,-50)
    
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
