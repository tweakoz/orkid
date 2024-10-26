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

args = vars(parser.parse_args())

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

    self.ptc_data = createParticleData(use_streaks=True)
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
