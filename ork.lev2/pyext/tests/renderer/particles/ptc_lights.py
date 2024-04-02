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
    self.ezapp = OrkEzApp.create(self,ssaa=4)
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
    sceneparams.SkyboxIntensity = float(1)
    sceneparams.SpecularIntensity = float(1.2)
    sceneparams.DiffuseIntensity = float(1.0)
    sceneparams.AmbientLight = vec3(0.0)
    sceneparams.DepthFogDistance = float(1e6)
    sceneparams.SkyboxTexPathStr = "src://envmaps/tozenv_caustic1.png"
    ###################################
    # post fx node
    ###################################
    postNode = DecompBlurPostFxNode()
    postNode.threshold = 0.99
    postNode.blurwidth = 16.0
    postNode.blurfactor = 0.1
    postNode.amount = 0.1
    postNode.gpuInit(ctx,8,8);
    postNode.addToVarMap(sceneparams,"PostFxNode")
    #self.post_node = postNode
    ###################################
    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]
    ###################################
    # create particle drawable 
    ###################################
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
    #######################################
    def gen_sys(grad,frq,radius):
      ptc = ParticleContainer(self.scene,self.layer_fwd)
      createParticleData(ptc,ptc_data,ptc_connections,self.layer_fwd)
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
    #######################################
    self.ptc_systems = []
    count = 32
    for i in range(count):
      fi = float(i)/float(count)
      frq = 0.4 + (fi*2)
      radius = 35 + fi*35
      g = i&7
      self.ptc_systems += [gen_sys(presetGRAD(g),frq,radius)]
    #self.ptc_systems[0].EMITN.inputs.EmissionVelocity = 0.1
    #######################################
    gmtl = PBRMaterial() 
    gmtl.texColor = Texture.load("src://effect_textures/white.dds")
    gmtl.texNormal = Texture.load("src://effect_textures/default_normal.dds")
    gmtl.texMtlRuf = Texture.load("src://effect_textures/white.dds")
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.baseColor = vec4(0.8,0.8,1,1)
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
    for item in self.ptc_systems:

      prv_trans = item.particlenode.worldTransform.translation
      f = - item.frq
      x = item.radius*math.cos(updinfo.absolutetime*f)
      y = 30+math.sin(updinfo.absolutetime*f)*30
      z = item.radius*math.sin(updinfo.absolutetime*f)*-1.0
      
      new_trans = vec3(x,y,z)

      #delta_dir = (new_trans-prv_trans).normalized()
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
