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
from lev2utils.primitives import createParticleData, createGridData
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

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
    setupUiCamera( app=self, eye = vec3(0,5,30), constrainZ=True, up=vec3(0,1,0))
    self._gradient_phase_A = 0.0
    self._gradient_phase_B = 0.0 
    self.phi = 0.0
    self.prev_phi = 0.0
    self.isect = vec3(0,0,0)
    self.prev_isect = vec3(0,0,0)
    self.ppos = vec3(0,0,0)
    self.gpos = vec3(0,0,0)
    self.aspect = 1280.0/720.0

  ################################################

  def onGpuInit(self,ctx):
    createSceneGraph(app=self,rendermodel="ForwardPBR")
    self.createParticles()
    self.createGrid()

  ################################################

  def createGrid(self,extent=1000):
     self.grid_data = createGridData()
     self.grid_data.extent =  extent
     self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
     self.grid_node.sortkey = 1
     self._ground_plane = plane(vec3(0,1,0),0)
     

  ################################################

  def createParticles(self):

      # create particle graph and modules

      self.psys_data = dataflow.GraphData.createShared()
      self.ptc_pool   = self.psys_data.create("POOL",particles.Pool)
      self.emitter    = self.psys_data.create("EMITN",particles.NozzleEmitter)
      self.globals    = self.psys_data.create("GLOB",particles.Globals)
      self.gravity    = self.psys_data.create("GRAV",particles.Gravity)
      self.turbulence    = self.psys_data.create("TURB",particles.Turbulence)

      self.streaks    = self.psys_data.create("STRK",particles.StreakRenderer)
      self.streaks.inputs.Length = .1
      self.streaks.material = particles.GradientMaterial.createShared()
      self.gradient = self.streaks.material.gradient

      self.streaks.material.blending = tokens.ADDITIVE

      self.ptc_pool.pool_size = 16384 # max number of particles in pool

      # connect modules in a chain configuration

      self.psys_data.connect( self.emitter.inputs.pool,    self.ptc_pool.outputs.pool )
      self.psys_data.connect( self.gravity.inputs.pool,    self.emitter.outputs.pool )
      self.psys_data.connect( self.turbulence.inputs.pool,    self.gravity.outputs.pool )

      self.psys_data.connect( self.streaks.inputs.pool,    self.turbulence.outputs.pool )

      # streak-renderer module settings

      self.streaks.inputs.Length = .1
      self.streaks.inputs.Width = .01

      # emitter module settings

      self.emitter.inputs.EmissionRate = 800
      self.emitter.inputs.EmissionVelocity = 1
      self.emitter.inputs.DispersionAngle = 45
      self.emitter.inputs.Offset = vec3(0,2,0)

      # gravity module settings

      self.gravity.inputs.G = 0
      self.gravity.inputs.Mass = 1
      self.gravity.inputs.OthMass = 1
      self.gravity.inputs.MinDistance = 3
      self.gravity.inputs.Center = vec3(0,0,0)

      # create drawable / sgnode

      self.ptc_draw_data = ParticlesDrawableData()
      self.ptc_draw_data.graphdata = self.psys_data
      self.ptc_drawable = self.ptc_draw_data.createDrawable()
      self.ptc_node = self.layer1.createDrawableNode("particle-node",self.ptc_drawable)
      self.ptc_node.sortkey = 2;
      self.phi = 0.0

  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.phi += updinfo.deltatime

    phA = self.phi
    phB = phA+1
    radius = 1

    ###########################
    # compute emitter position and direction
    ###########################

    self.prev_isect = self.prev_isect*0.9999+self.isect*0.0001

    Z_DIR = (self.isect - self.prev_isect)
    if Z_DIR.length < 0.001:
      Z_DIR = vec3(0,0,1)
    else:
      Z_DIR = Z_DIR.normalized
    Y_DIR = vec3(0,1,0)
    X_DIR = Y_DIR.cross(Z_DIR).normalized

    self.emitter.inputs.Offset = self.ppos
    self.emitter.inputs.DirectionX = X_DIR
    self.emitter.inputs.DirectionY = Y_DIR
    self.emitter.inputs.DirectionZ = Z_DIR
    self.emitter.inputs.EmissionVelocity = 0.4
    self.emitter.inputs.DispersionAngle = 45
    self.emitter.inputs.EmissionRate = 2000
    self.emitter.inputs.LifeSpan = 1.3

    ###########################
    # compute gravity center and direction
    ###########################

    self.gravity.inputs.Center = self.gpos
    self.gravity.inputs.G = 1.3

    ###########################

    self.turbulence.inputs.Amount = vec3(1,1,1)*15

    ###########################

    self.streaks.inputs.Length = .1
    self.streaks.inputs.Width = .03

    self._gradient_phase_A = phA 
    self._gradient_phase_B = phB 


  ##############################################

  def onGpuUpdate(self,context):
    phA = self._gradient_phase_A
    phB = self._gradient_phase_B
    RED0 = 0.5+math.sin(phA)*0.5
    GREEN0 = 0.5+math.sin(phA*1.3)*0.5
    BLUE0 = 0.5+math.sin(phA*1.7)*0.5
    MIDCOLOR = vec4(RED0,GREEN0,BLUE0,1)

    RED1 = 0.5+math.sin(phB)*0.5
    GREEN1 = 0.5+math.sin(phB*1.3)*0.5
    BLUE1 = 0.5+math.sin(phB*1.7)*0.5
    ENDCOLOR = vec4(RED1,GREEN1,BLUE1,1)
    self._the_gradient = {
        0.0: vec4(0,0,0,1),
        0.3: MIDCOLOR,
        0.5: vec4(.3,.3,.3,1),
        0.9: ENDCOLOR,
        1.0: vec4(0,0,0,1),
    }
    self.gradient.setColorStops(self._the_gradient)

  ##############################################

  def onUiEvent(self,uievent):

     dphi = self.phi-self.prev_phi
     self.prev_phi = self.phi
     #print(uievent.unit_pos)
     if uievent.code == tokens.RESIZED.hashed:
       dim = uievent.screen_dim
       self.aspect = dim.x/dim.y
       print(self.aspect)
     elif uievent.code == tokens.DRAG.hashed:
       ray = self.uicam.cameradata.projectDepthRay(uievent.unit_pos,self.aspect)
       isect_test = self._ground_plane.intersectWithRay(ray)
       did_intersect = isect_test["did_intersect"]
       if did_intersect:
        self.prev_isect = self.isect
        self.isect = isect_test["point"]
        self.isect_dir = (self.isect - self.prev_isect).normalized
        self.ppos = self.isect
        self.gpos = self.isect+vec3(0,1,0)#  (self.isect_dir*2)
      
     #handled = self.uicam.uiEventHandler(uievent)
     #if handled:
     # self.camera.copyFrom( self.uicam.cameradata )
     return ui.HandlerResult()
###############################################################################

app_top = ParticlesApp()
app_top.ezapp.mainThreadLoop()
