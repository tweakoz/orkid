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

from lev2utils.cameras import *
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
    setupUiCamera( app=self, eye = vec3(50), tgt=vec3(0,0,1), constrainZ=True, up=vec3(0,1,0))

    self.ecsscene = ecs.SceneData()

    ##############################################
    # setup global physics 
    ##############################################

    systemdata_phys = self.ecsscene.createSystem("BulletSystem")
    systemdata_phys.timeScale = 1.0
    systemdata_phys.simulationRate = 240.0
    systemdata_phys.debug = False
    
    self.systemdata_phys = systemdata_phys
    ##############################################
    self.player_transform = None

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

  def createRoomData(self,ctx):
    self.arch_room = self.ecsscene.createArchetype("RoomArchetype")
    self.spawn_room = self.ecsscene.createSpawnData("spawn_room")
    self.spawn_room.archetype = self.arch_room
    self.spawn_room.autospawn = True
    self.spawn_room.transform.translation = vec3(0,-10,0)
    self.spawn_room.transform.scale = 1.0

    #########################
    # physics for room
    #########################

    room_sg_component = self.arch_room.createComponent("SceneGraphComponent")
    room_phys = self.arch_room.createComponent("BulletObjectComponent")

    room_shape = ecs.BulletShapeMeshData()
    room_shape.meshpath = "data://tests/environ/envtest2.obj"
    room_shape.scale = vec3(5,8,5)
    room_shape.translation = vec3(0,0.01,0)

    room_phys.mass = 0.0
    room_phys.allowSleeping = True
    room_phys.isKinematic = False
    room_phys.disablePhysics = True
    room_phys.shape = room_shape

    #########################
    # visible mesh for room
    #########################

    self.room_drawable = ModelDrawableData("data://tests/environ/roomtest.glb")
    
    room_mesh_transform = Transform()
    room_mesh_transform.nonUniformScale = vec3(5,8,5)
    room_mesh_transform.translation = vec3(0,-0.05,0)

    room_node = room_sg_component.declareNodeOnLayer( name = "roomvis",
                                                      drawable = self.room_drawable,
                                                      layer = "layer1",
                                                      transform = room_mesh_transform)
    
  ##############################################

  def onGpuInit(self,ctx):
    
    ####################
    # create scenegraph
    ####################

    self.sysd_sg = self.ecsscene.createSystem("SceneGraphSystem")
    self.sysd_sg.declareLayer("layer1")
    self.sysd_sg.declareParams({
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
    self.createRoomData(ctx)
    
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
    self.systemdata_phys.linGravity = vec3(0,-9.8*3,0)
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

    self.controller.updateSimulation()

  ##############################################

  def onUiEvent(self,uievent):

    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

    return ui.HandlerResult()

###############################################################################

MinimalSceneGraphApp().ezapp.mainThreadLoop()
