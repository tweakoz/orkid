#!/usr/bin/env python3

################################################################################
# ECS (Entity/Component/System) minimal sample 
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
from lev2utils.cameras import *

################################################################################
tokens = core.CrcStringProxy()
################################################################################

class ECS_MINIMAL(object):

  ##############################################

  def __init__(self):
    super().__init__()
    self.ezapp = ecs.createApp(self,ssaa=2,fullscreen=False)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    setupUiCamera( app=self, eye = vec3(50), tgt=vec3(0,0,1), constrainZ=True, up=vec3(0,1,0))

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
    c_phys.friction = 0.3
    c_phys.restitution = 0.45
    c_phys.angularDamping = 0.01
    c_phys.linearDamping = 0.01
    c_phys.allowSleeping = False
    c_phys.isKinematic = False
    c_phys.disablePhysics = False
    c_phys.shape = sphere

    self.comp_phys = c_phys

    self.ball_drawable = ModelDrawableData("data://tests/pbr_calib.glb")
    self.comp_sg.declareNodeOnLayer( name="cube1",drawable=self.ball_drawable,layer="layer1")

  ##############################################

  def createEnvironmentData(self,ctx):

    arch_env = self.ecsscene.createArchetype("RoomArchetype")
    spawn_env = self.ecsscene.createSpawnData("spawn_env")
    spawn_env.archetype = arch_env
    spawn_env.autospawn = True
    spawn_env.transform.translation = vec3(0,-10,0)
    spawn_env.transform.scale = 1.0

    #########################
    # physics for room
    #########################

    c_scenegraph = arch_env.createComponent("SceneGraphComponent")
    c_physics = arch_env.createComponent("BulletObjectComponent")

    shape = ecs.BulletShapeMeshData()
    shape.meshpath = "data://tests/environ/envtest2.obj"
    shape.scale = vec3(5,8,5)
    shape.translation = vec3(0,0.01,0)

    c_physics.mass = 0.0
    c_physics.allowSleeping = True
    c_physics.isKinematic = False
    c_physics.disablePhysics = True
    c_physics.shape = shape

    #########################
    # visible mesh for room
    #########################

    drawable = ModelDrawableData("data://tests/environ/roomtest.glb")
    
    mesh_transform = Transform()
    mesh_transform.nonUniformScale = vec3(5,8,5)
    mesh_transform.translation = vec3(0,-0.05,0)

    room_node = c_scenegraph.declareNodeOnLayer( name = "env_vis",
                                                 drawable = drawable,
                                                 layer = "layer1",
                                                 transform = mesh_transform)
    
  ##############################################

  def onGpuInit(self,ctx):
    
    ####################
    # create ECS scene
    ####################

    self.ecsscene = ecs.SceneData()

    ##############################################
    # setup global physics 
    ##############################################

    systemdata_phys = self.ecsscene.createSystem("BulletSystem")
    systemdata_phys.timeScale = 1.0
    systemdata_phys.simulationRate = 240.0
    systemdata_phys.debug = False
    systemdata_phys.linGravity = vec3(0,-9.8*3,0)

    ####################
    # create scenegraph
    ####################

    systemdata_SG = self.ecsscene.createSystem("SceneGraphSystem")
    systemdata_SG.declareLayer("layer1")
    systemdata_SG.declareParams({
      "SkyboxIntensity": float(2.0),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.1),
      "DepthFogDistance": float(2000),
      "DepthFogPower": float(1.25),
    })

    ####################
    # create archetype/entity data
    ####################

    self.createBallData(ctx)
    self.createEnvironmentData(ctx)
    
    ####################
    # create ECS controller
    ####################

    self.controller = ecs.Controller()
    self.controller.bindScene(self.ecsscene)

    ##################
    # launch simulation
    ##################
        
    #self.controller.beginWriteTrace(str(obt_path.temp()/"ecstrace.json"));
    self.controller.createSimulation()
    self.controller.startSimulation()

    ##################
    # retrieve simulation systems
    ##################

    self.sys_phys = self.controller.findSystem("BulletSystem")
    self.sys_sg = self.controller.findSystem("SceneGraphSystem")

    ##################
    # init systems
    ##################

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
    # spawn balls
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
    # camera update
    ##############################

    UIC = self.uicam.cameradata
   
    self.controller.systemNotify( self.sys_sg,
                                  tokens.UpdateCamera,{
                                    tokens.eye: UIC.eye,
                                    tokens.tgt: UIC.target,
                                    tokens.up: UIC.up,
                                    tokens.near: UIC.near,
                                    tokens.far: UIC.far,
                                    tokens.fovy: UIC.fovy
                                  }
                                 )

    ##############################
    # tick the simulation
    ##############################

    self.controller.updateSimulation()

  ##############################################

  def onUiEvent(self,uievent):

    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

    return ui.HandlerResult()

###############################################################################

ECS_MINIMAL().ezapp.mainThreadLoop()
