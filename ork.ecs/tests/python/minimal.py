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
    self.jump = False

  ##############################################

  def createPlayerData(self,ctx):
    self.arch_player = self.ecsscene.createArchetype("PlayerArchetype")
    self.spawn_player = self.ecsscene.createSpawnData("spawn_player")
    self.spawn_player.archetype = self.arch_player
    self.spawn_player.autospawn = True
    self.spawn_player.transform.translation = vec3(0,0,0)
    self.spawn_player.transform.orientation = quat(vec3(1,0,0),math.pi*0.5)
    self.comp_sg = self.arch_player.createComponent("SceneGraphComponent")

    capsule = ecs.BulletShapeCapsuleData()
    capsule.radius = 1.0
    capsule.extent = 3.0

    c_phys = self.arch_player.createComponent("BulletObjectComponent")

    c_phys.mass = 10.0
    c_phys.friction = 1
    c_phys.restitution = 0.0
    c_phys.angularDamping = 1
    c_phys.linearDamping = 0.5
    c_phys.allowSleeping = False
    c_phys.isKinematic = False
    c_phys.disablePhysics = False
    c_phys.angularFactor = vec3(0,1,0)
    c_phys.shape = capsule

    self.comp_phys_player = c_phys

    viz_xf = Transform()
    viz_xf.nonUniformScale = vec3(1,1,3)

    self.player_drawable = ModelDrawableData("data://tests/pbr_calib.glb")
    self.comp_sg.declareNodeOnLayer( name="cube1",
                                     drawable=self.player_drawable,
                                     layer="layer1",
                                     transform=viz_xf)

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

    #self.cube = prims.createCubePrim(ctx=ctx, size=1.0)
    #pipeline = createPipeline( app = self, ctx = ctx, rendermodel="DeferredPBR", techname="std_mono_deferred" )
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
    # visible grid for room
    #########################

    #self.room_grid = prims.createGridData(extent=100)
    #pipeline = createPipeline( app = self, ctx = ctx, rendermodel="DeferredPBR", techname="std_mono_deferred" )

    #room_sg_component.declareNodeOnLayer( name="ground",
    #                                      drawable=self.room_grid,
    #                                      layer="layer1")

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
      "SkyboxIntensity": float(1.5),
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
    self.createPlayerData(ctx)
    
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

  def playerImpulse(self,impulse):
    self.controller.systemNotify( self.sys_phys,
                                  tokens.IMPULSE,
                                  {
                                    tokens.component: self.comp_phys_player,
                                    tokens.impulse: impulse
                                  })

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    elif uievent.code == tokens.KEY_DOWN.hashed:
      
      camdir = (self.uicam.cameradata.target-self.uicam.cameradata.eye).normalized(),
      print(camdir)
      
      # get projection of camdir on xz groundplane
      projdir = vec3(camdir[0].x,0,camdir[0].z).normalized()
      
      
      if uievent.keycode == ord(" "): 
        self.playerImpulse(vec3(0,80,0)) # JUMP
      elif uievent.keycode == ord("W"):
        self.playerImpulse(projdir*50.0) # FORWARD
      #elif uievent.keycode == ord("A"):
      #  self.playerImpulse(vec3(20,0,0)) # LEFT
      elif uievent.keycode == ord("S"):
        self.playerImpulse(projdir*-50.0) # BACKWARD
      #elif uievent.keycode == ord("D"):
      #  self.playerImpulse(vec3(-20,0,0)) # RIGHT
    elif uievent.code == tokens.KEY_UP.hashed:
      pass
    return ui.HandlerResult()

###############################################################################

MinimalSceneGraphApp().ezapp.mainThreadLoop()
