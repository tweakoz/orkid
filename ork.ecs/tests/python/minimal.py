#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random
from pathlib import Path
from obt import path as obt_path
from orkengine import core, lev2, ecs

this_dir = Path(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(str(obt_path.orkid()/"ork.lev2"/"examples"/"python")) # add parent dir to path
from ork import path as ork_path
sys.path.append(str(ork_path.lev2_pylib)) # add parent dir to path
from lev2utils import primitives as prims

print(ork_path.lev2_pylib)
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createFrustumPrim, createGridData
from lev2utils.scenegraph import createSceneGraph

tokens = core.CrcStringProxy()
################################################################################

class MinimalSceneGraphApp(object):

  ##############################################

  def __init__(self):
    super().__init__()
    self.ezapp = ecs.createApp(self,ssaa=2,fullscreen=False)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    setupUiCamera( app=self, eye = vec3(10,10,10)*3, tgt=vec3(0,5,0), constrainZ=True, up=vec3(0,1,0))

    self.ecsscene = ecs.SceneData()

    ##############################################
    # setup global physics 
    ##############################################

    systemdata_phys = self.ecsscene.createSystem("BulletSystem")
    systemdata_phys.expGravity = vec3(0,-9.8*2,0)
    systemdata_phys.timeScale = 1.0
    systemdata_phys.simulationRate = 240.0
    systemdata_phys.debug = True
    
    self.systemdata_phys = systemdata_phys

    ##############################################

  ##############################################

  def createBallData(self,ctx):
    self.arch_ball = self.ecsscene.createArchetype("BoxArchetype")
    self.spawn_ball = self.ecsscene.createSpawnData("spawn_ball")
    self.spawn_ball.archetype = self.arch_ball
    self.spawn_ball.autospawn = False
    self.comp_sg = self.arch_ball.createComponent("SceneGraphComponent")

    sphere = ecs.BulletShapeSphereData()
    sphere.radius = 1.0

    c_phys = self.arch_ball.createComponent("BulletObjectComponent")

    c_phys.mass = 1.0
    c_phys.friction = 0.1
    c_phys.restitution = 0.45
    c_phys.angularDamping = 0.1
    c_phys.linearDamping = 0.1
    c_phys.allowSleeping = False
    c_phys.isKinematic = False
    c_phys.disablePhysics = False
    c_phys.shape = sphere

    self.comp_phys = c_phys

    #self.cube = prims.createCubePrim(ctx=ctx, size=1.0)
    #pipeline = createPipeline( app = self, ctx = ctx, rendermodel="DeferredPBR", techname="std_mono_deferred" )
    self.ball_drawable = ModelDrawableData("data://tests/pbr_calib.glb")
    self.comp_sg.declareNodeOnLayer("cube1",self.ball_drawable,"layer1")
  ##############################################

  def createGroundData(self,ctx):
    self.arch_ground = self.ecsscene.createArchetype("GroundArchetype")
    self.spawn_ground = self.ecsscene.createSpawnData("spawn_ground")
    self.spawn_ground.archetype = self.arch_ground
    self.spawn_ground.autospawn = True

    ground = ecs.BulletShapePlaneData()
    ground.normal = vec3(0,1,0)
    ground.position = vec3(0,0,0)

    ground_sgc = self.arch_ground.createComponent("SceneGraphComponent")
    ground_phys = self.arch_ground.createComponent("BulletObjectComponent")

    ground_phys.mass = 0.0
    ground_phys.allowSleeping = True
    ground_phys.isKinematic = False
    ground_phys.disablePhysics = True
    ground_phys.shape = ground

    self.grid = prims.createGridData(extent=100)
    pipeline = createPipeline( app = self, ctx = ctx, rendermodel="DeferredPBR", techname="std_mono_deferred" )

    ground_sgc.declareNodeOnLayer("ground",self.grid,"layer1")

  ##############################################

  def onGpuInit(self,ctx):
    
    self.sysd_sg = self.ecsscene.createSystem("SceneGraphSystem")
    self.sysd_sg.declareLayer("layer1")
    
    self.createBallData(ctx)
    self.createGroundData(ctx)
    
    self.controller = ecs.Controller()
    self.controller.bindScene(self.ecsscene)

    ##################
        
    #self.controller.beginWriteTrace(str(obt_path.temp()/"ecstrace.json"));
    self.controller.createSimulation()

    self.controller.startSimulation()

    ##################

    self.sys_phys = self.controller.findSystem("BulletSystem")
    self.sys_sg = self.controller.findSystem("SceneGraphSystem")

    ##################

    self.controller.systemNotify(self.sys_phys,tokens.YO,vec3(0,0,0))
    self.controller.systemNotify( self.sys_sg,tokens.ResizeFromMainSurface,True)
    self.spawncounter = 0
    
  ##############################################

  def onDraw(self,drawevent):
    self.controller.renderSimulation(drawevent)

  ##############################################

  def onGpuExit(self,ctx):
    self.controller.stopSimulation()
    self.controller.beginWriteTrace

  ##############################################

  def onUpdate(self,updinfo):
    ##############################
    self.systemdata_phys.linGravity = vec3(0,-9.8,0)
    ##############################
    i = random.randint(-5,5)
    j = random.randint(-5,5)
    prob = random.randint(0,100)
    if prob < 5 and self.spawncounter < 250:
      self.spawncounter += 1
      SAD = ecs.SpawnAnonDynamic("spawn_ball")
      SAD.overridexf.orientation = quat(vec3(0,1,0),0)
      SAD.overridexf.scale = 1.0
      SAD.overridexf.translation = vec3(i,15,j)
      self.e1 = self.controller.spawnEntity(SAD)
      
    ##############################
    self.controller.systemNotify( self.sys_sg,
                                  tokens.UpdateCamera,{
                                    tokens.eye: self.uicam.cameradata.eye,
                                    tokens.tgt: self.uicam.cameradata.target,
                                    tokens.up: self.uicam.cameradata.up,
                                    tokens.near: self.uicam.cameradata.near,
                                    tokens.far: self.uicam.cameradata.far,
                                    tokens.fovy: self.uicam.cameradata.fovy
                                  }
                                 )
    ##############################
    self.controller.updateSimulation()

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

###############################################################################

MinimalSceneGraphApp().ezapp.mainThreadLoop()
