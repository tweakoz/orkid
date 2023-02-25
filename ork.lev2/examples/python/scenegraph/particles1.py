#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, sys, os, random, numpy, argparse
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.primitives import createParticleData
from common.scenegraph import createSceneGraph

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

    ##################
    # create particle sg node
    ##################

    self.particlenode = self.layer1.createDrawableNode("particle-node",ptc_drawable)
    self.particlenode.sortkey = 2;


  ################################################

  def onUpdate(self,updinfo):
    ########################################
    if dynaplugs: # dynamic update of plugs
      abstime = updinfo.absolutetime
      self.emitterplugs.LifeSpan = 20
      self.emitterplugs.EmissionRate = (math.sin(abstime*6)+1)*800
      self.emitterplugs.DispersionAngle = (math.sin(abstime*0.37)+1)*75
      emitter_pos = vec3()
      emitter_pos.x = (math.sin(abstime*0.01)+1)*2
      emitter_pos.y = (math.sin(abstime*0.027)+1)*2
      emitter_pos.z = (math.sin(abstime*0.035)+1)*2
      self.emitterplugs.Offset = emitter_pos
      self.vortexplugs.VortexStrength = .5
      self.vortexplugs.OutwardStrength = -.1
      self.vortexplugs.Falloff = 0
      self.gravityplugs.G = .2
      self.turbulenceplugs.Amount = vec3(3,3,3)
    ########################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    ########################################

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

###############################################################################

ParticlesApp().ezapp.mainThreadLoop()
