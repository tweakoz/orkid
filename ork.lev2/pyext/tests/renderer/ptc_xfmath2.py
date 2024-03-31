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

    createSceneGraph(app=self,rendermodel="ForwardPBR")

    ###################################
    # create particle drawable 
    ###################################

    ptc_data = {
      "GLOB":particles.Globals,
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
    createParticleData(self,ptc_data,ptc_connections)

    self.POOL.pool_size = 65536 # max number of particles in pool

    #self.STRK.inputs.Size = 0.05
    self.STRK.inputs.GradientIntensity = 1
    self.STRK.material = presetMaterial(grad=presetGRAD2(),texname="src://effect_textures/ptc1.png")
    self.EMITN.inputs.EmissionVelocity = 0.1
    presetPOOL1(self.POOL)
    presetEMITN1(self.EMITN)
    presetEMITR1(self.EMITR)
    presetTURB1(self.TURB)
    presetVORT1(self.VORT)
    presetGRAV1(self.GRAV)
        
    self.TURB.inputs.Amount = vec3(1,1,1)*5
    
    self.EMITN.inputs.EmissionRate = 1500
    self.EMITR.inputs.EmissionRate = 1500
    self.EMITN.inputs.LifeSpan = 8
    self.EMITR.inputs.LifeSpan = 5
    self.STRK.inputs.Width = 0.01
    LEN = self.STRK.inputs.Length
    LENXF = LEN.transformer 
    WID = self.STRK.inputs.Width
    WIDXF = WID.transformer 

    LENXF.append(floatxf.quantize(0.025))
    #LENXF.append(floatxf.scale(4.0))
    LENXF.append(floatxf.sine())
    LENXF.append(floatxf.abs())
    #LENXF.append(floatxf.smoothstep(0.3,0.7))
    #LENXF.append(floatxf.power(8.0))
    LENXF.append(floatxf.scale(0.05))

    WIDXF.append(floatxf.quantize(1.0/16))
    #WIDXF.append(floatxf.scale(8.0))
    WIDXF.append(floatxf.sine())
    WIDXF.append(floatxf.abs())
    WIDXF.append(floatxf.scale(0.125))
    
    self.graphdata.connect(LEN,#
                           self.POOL.outputs.UnitAge)

    self.graphdata.connect(WID,#
                           self.POOL.outputs.UnitAge)

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
