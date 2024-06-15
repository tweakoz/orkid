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
from lev2utils.cameras import *

tokens = core.CrcStringProxy()
################################################################################

class ECS_FIRST_PERSON_SHOOTER(object):

  ##############################################

  def __init__(self):
    super().__init__()
    self.ezapp = ecs.createApp(self,ssaa=2,fullscreen=False)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    setupUiCamera( app=self, eye = vec3(0,0,0), tgt=vec3(0,0,1), constrainZ=True, up=vec3(0,1,0))

    ##############################################

    self.player_transform = None

  ##############################################

  def createPlayerData(self,ctx):

    self.arch_player = self.ecsscene.createArchetype("PlayerArchetype")
    c_scenegraph = self.arch_player.createComponent("SceneGraphComponent")
    c_physics = self.arch_player.createComponent("BulletObjectComponent")

    ######################################
    # scenegraph setup for player
    ######################################

    if False: # dont really need in first person mode
      drawable = ModelDrawableData("data://tests/pbr_calib.glb")
      viz_xf = Transform() # non uniform scale for sphere (to make it a capsule)
      viz_xf.nonUniformScale = vec3(1,1,3)

      c_scenegraph.declareNodeOnLayer( name="cube1",
                                       drawable=drawable,
                                       layer="layer1",
                                       transform=viz_xf)

    ######################################
    # physics setup for player
    ######################################
    
    capsule = ecs.BulletShapeCapsuleData()
    capsule.radius = 1.0
    capsule.extent = 3.0

    c_physics.mass = 10.0
    c_physics.friction = 0.1
    c_physics.restitution = 0.0
    c_physics.angularDamping = 1
    c_physics.linearDamping = 0.5
    c_physics.allowSleeping = False
    c_physics.isKinematic = False
    c_physics.disablePhysics = False
    c_physics.angularFactor = vec3(0,1,0)
    c_physics.shape = capsule

    self.playerforce = ecs.DirectionalForceData()
    c_physics.declareForce("playerforce",self.playerforce)
    self.playerforce_rot = 0.0

    ######################################
    # statically spawned player entity
    ######################################

    spawn_player = self.ecsscene.createSpawnData("spawn_player")
    spawn_player.archetype = self.arch_player
    spawn_player.autospawn = True
    spawn_player.transform.translation = vec3(0,0,0)
    spawn_player.transform.orientation = quat(vec3(1,0,0),math.pi*0.5)

    ######################################
    # catch the player entity's transform
    #  invoked on update thread 
    #    when entity is actually spawned.
    ######################################

    def onSpawn(entity):
      self.player_transform = entity.transform

    spawn_player.onSpawn(onSpawn)

  ##############################################

  def createBallData(self,ctx):

    self.arch_ball = self.ecsscene.createArchetype("BoxArchetype")
    self.spawn_ball = self.ecsscene.createSpawnData("spawn_ball")
    self.spawn_ball.archetype = self.arch_ball
    self.spawn_ball.autospawn = False
    c_scenegraph = self.arch_ball.createComponent("SceneGraphComponent")

    sphere = ecs.BulletShapeSphereData()
    sphere.radius = 1.0

    c_physics = self.arch_ball.createComponent("BulletObjectComponent")

    c_physics.mass = 1.0
    c_physics.friction = 0.3
    c_physics.restitution = 0.45
    c_physics.angularDamping = 0.01
    c_physics.linearDamping = 0.01
    c_physics.allowSleeping = False
    c_physics.isKinematic = False
    c_physics.disablePhysics = False
    c_physics.shape = sphere

    self.ball_drawable = ModelDrawableData("data://tests/pbr_calib.glb")
    c_scenegraph.declareNodeOnLayer( name="cube1",drawable=self.ball_drawable,layer="layer1")

  ##############################################
  # generate the environment
  ##############################################

  def createEnvironmentData(self,ctx):

    arch_room = self.ecsscene.createArchetype("RoomArchetype")
    spawn_room = self.ecsscene.createSpawnData("spawn_room")
    spawn_room.archetype = arch_room
    spawn_room.autospawn = True
    spawn_room.transform.translation = vec3(0,-10,0)
    spawn_room.transform.scale = 1.0

    #########################
    # physics for room
    #########################

    c_scenegraph = arch_room.createComponent("SceneGraphComponent")
    c_physics = arch_room.createComponent("BulletObjectComponent")

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

    self.room_drawable = ModelDrawableData("data://tests/environ/roomtest.glb")
    
    room_mesh_transform = Transform()
    room_mesh_transform.nonUniformScale = vec3(5,8,5)
    room_mesh_transform.translation = vec3(0,-0.05,0)

    room_node = c_scenegraph.declareNodeOnLayer( name = "roomvis",
                                                      drawable = self.room_drawable,
                                                      layer = "layer1",
                                                      transform = room_mesh_transform)
    
  ##############################################

  def onGpuInit(self,ctx):
    
    ####################
    # create ECS scene
    ####################

    self.ecsscene = ecs.SceneData()

    ####################
    # create (graphics) scenegraph system
    ####################

    self.systemdata_scenegraph = self.ecsscene.createSystem("SceneGraphSystem")
    self.systemdata_scenegraph.declareLayer("layer1")
    self.systemdata_scenegraph.declareParams({
      "SkyboxIntensity": float(2.0),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.1),
      "DepthFogDistance": float(2000),
      "DepthFogPower": float(1.25),
    })

    ####################
    # create physics system
    ####################

    systemdata_phys = self.ecsscene.createSystem("BulletSystem")
    systemdata_phys.timeScale = 1.0
    systemdata_phys.simulationRate = 240.0
    systemdata_phys.debug = False
    systemdata_phys.linGravity = vec3(0,-9.8*3,0)

    self.systemdata_phys = systemdata_phys

    ####################
    # create archetype/entity data
    ####################

    self.createBallData(ctx)
    self.createEnvironmentData(ctx)
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

    # clean up

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
    if prob < 5 and self.spawncounter < 500:
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
    PXF = self.player_transform
    if PXF is not None:
      EYE = PXF.translation
      ROT = self.playerforce_rot
      DIR = (UIC.target-UIC.eye).normalized()
      # rot around Y by ROT
      MOTION_DIR = vec3(DIR.x,DIR.y,DIR.z)
      MOTION_DIR.roty(ROT)
      
      TGT = EYE + DIR
      UP = vec3(0,1,0)
      
      self.playerforce.direction = MOTION_DIR

      self.controller.systemNotify( self.sys_sg,
                                    tokens.UpdateCamera,{
                                      tokens.eye: EYE,
                                      tokens.tgt: TGT,
                                      tokens.up: UP,
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

  def playerImpulse(self,impulse):
    self.controller.systemNotify( self.sys_phys,
                                  tokens.IMPULSE,
                                  {
                                    tokens.component: self.comp_phys_player,
                                    tokens.impulse: impulse
                                  })

  ##############################################

  def onUiEvent(self,uievent):

    walk_force = 5e3

    ##############################################
    # camera controls
    ##############################################

    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

    ##############################################
    # motion controls
    ##############################################

    elif uievent.code == tokens.KEY_DOWN.hashed:
      #### JUMP #####
      if uievent.keycode == ord(" "): 
        self.playerImpulse(vec3(0,80,0)) # JUMP
      #### FORWARD #####
      elif uievent.keycode == ord("W"):
        self.playerforce.magnitude = walk_force
        self.playerforce_rot = 0.0
      #### LEFT #####
      elif uievent.keycode == ord("A"):
        self.playerforce.magnitude = walk_force
        self.playerforce_rot = math.pi*1.5
      #### BACK #####
      elif uievent.keycode == ord("S"):
        self.playerforce.magnitude = walk_force
        self.playerforce_rot = math.pi
      #### RIGHT #####
      elif uievent.keycode == ord("D"):
        self.playerforce.magnitude = walk_force
        self.playerforce_rot = math.pi*0.5
    ##############################################
    elif uievent.code == tokens.KEY_UP.hashed:
      if uievent.keycode in [ord("W"),ord("A"),ord("S"),ord("D")]:
        self.playerforce.magnitude = 0.0

    return ui.HandlerResult()

###############################################################################

ECS_FIRST_PERSON_SHOOTER().ezapp.mainThreadLoop()
