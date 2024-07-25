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
from lev2utils.primitives import createParticleData
from lev2utils.scenegraph import createSceneGraph

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')
parser.add_argument('--dynaplugs', action="store_true", help='dynamic plug update' )

args = vars(parser.parse_args())
dynaplugs = args["dynaplugs"]

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
    self.emitter2plugs = self.ptc_data.emitter2.inputs
    self.vortexplugs = self.ptc_data.vortex.inputs
    self.gravityplugs = self.ptc_data.gravity.inputs
    self.turbulenceplugs = self.ptc_data.turbulence.inputs
    #self.spriteplugs = self.ptc_data.sprites.inputs

    self.emitterplugs.LifeSpan = 20
    self.emitterplugs.EmissionRate = 0
    self.emitterplugs.DispersionAngle = 0
    emitter_pos = vec3()
    emitter_pos.x = 0
    emitter_pos.y = 2
    emitter_pos.z = 0
    self.emitterplugs.Offset = emitter_pos

    self.material = particles.FlatMaterial.createShared();
    self.material2 = particles.TextureMaterial.createShared();

    self.material2.texture = Texture.load("src://effect_textures/spinner");

    self.cur_size = 1
    self.tgt_size = 1

    ##################
    # create particle sg node
    ##################

    ptc_drawable = self.ptc_data.drawable_data.createDrawable()
    self.particlenode = self.layer1.createDrawableNode("particle-node",ptc_drawable)
    self.particlenode.sortkey = 2;
    self.color = vec4(1, .5, 0, 1)
    self.counter = 0.0

  ################################################
  def configA(self,abstime):
    self.emitter2plugs.EmitterSpinRate = math.sin(abstime*0.25)*10
    self.emitter2plugs.EmissionRate = 800
    self.emitter2plugs.LifeSpan = 5
    self.vortexplugs.VortexStrength = 2
    self.vortexplugs.OutwardStrength = -2
    self.vortexplugs.Falloff = 0
    self.gravityplugs.G = .5
    self.turbulenceplugs.Amount = vec3(0,0,0)
    #self.ptc_data.sprites.material = self.material
    self.ptc_data.streaks.material = self.material
    self.emitter2plugs.Offset = vec3(0,3,0)
    if self.counter>1:
      self.tgt_size = random.uniform(4,8)
      self.counter = 0.0

  ################################################
  def configB(self,abstime):
    self.emitter2plugs.EmitterSpinRate = math.sin(abstime*0.25)*1
    self.emitter2plugs.EmissionRate = 800
    self.emitter2plugs.LifeSpan = 2
    self.vortexplugs.VortexStrength = .5
    self.vortexplugs.OutwardStrength = -.5
    self.vortexplugs.Falloff = 0
    self.gravityplugs.G = 1
    self.turbulenceplugs.Amount = vec3(2,2,2)
    #self.ptc_data.sprites.material = self.material
    self.ptc_data.streaks.material = self.material
    self.emitter2plugs.Offset = vec3(0,math.sin(abstime*1.5)*2,0)
    if self.counter>4:
      self.tgt_size = random.uniform(16,24)
      self.counter = 0.0

  ################################################
  def configC(self,abstime):
    self.emitter2plugs.EmitterSpinRate = math.sin(abstime)*30
    self.emitter2plugs.EmissionRate = 800
    self.emitter2plugs.LifeSpan = 5
    self.vortexplugs.VortexStrength = 0
    self.vortexplugs.OutwardStrength = 1
    self.vortexplugs.Falloff = 0
    self.gravityplugs.G = 1.1
    self.turbulenceplugs.Amount = vec3(8,8,8)
    #self.ptc_data.sprites.material = self.material2
    self.ptc_data.streaks.material = self.material
    self.emitter2plugs.Offset = vec3(0,math.sin(abstime*9.7)*3,0)
    if self.counter>5:
      self.tgt_size = random.uniform(16,32)
      self.counter = 0.0

  ################################################
  def configD(self,abstime):
    self.emitter2plugs.EmitterSpinRate = math.sin(abstime)*30
    self.emitter2plugs.EmissionRate = 1600
    self.emitter2plugs.LifeSpan = 7
    self.vortexplugs.VortexStrength = 3
    self.vortexplugs.OutwardStrength = -1
    self.vortexplugs.Falloff = 0
    self.gravityplugs.G = 1.1
    self.turbulenceplugs.Amount = vec3(12,12,12)
    #self.ptc_data.sprites.material = self.material2
    self.ptc_data.streaks.material = self.material
    self.emitter2plugs.Offset = vec3(0,math.sin(abstime*17.7)*4,0)
    if self.counter>5:
      self.tgt_size = random.uniform(16,32)
      self.counter = 0.0

  ################################################

  def onUpdate(self,updinfo):
    abstime = updinfo.absolutetime
    self.counter += updinfo.deltatime

    r = 0.5+math.sin(abstime*0.35)*0.5
    g = 0.5+math.sin(abstime*0.45)*0.5
    b = 0.5+math.sin(abstime*0.55)*0.5
    self.material.color = vec4(r,g,b, 1)
    self.material2.color = vec4(r,g,b, 1)
    self.cur_size = (self.cur_size*0.99) + (self.tgt_size*0.01)
    #self.spriteplugs.Size = self.cur_size

    ##########################################

    INDEX = int(math.fmod(abstime,32)/8)
    if(INDEX==0):
        self.configA(abstime)
    elif(INDEX==1):
        self.configB(abstime)
    elif(INDEX==2):
        self.configC(abstime)
    else:
        self.configD(abstime)


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
