#!/usr/bin/env python3

################################################################################
# ECS (Entity/Component/System) sample for a simple FPS like experience
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

################################################################################
tokens = core.CrcStringProxy()
LAYERNAME = "std_deferred"
GROUP_PLAYER = 1
GROUP_BALL = 2
GROUP_ENV = 4
GROUP_ALL = GROUP_PLAYER | GROUP_BALL | GROUP_ENV
NUM_BALLS = 400
################################################################################

class ECS_FIRST_PERSON_SHOOTER(object):

  ##############################################

  def __init__(self):
    super().__init__()

    self.ezapp = ecs.createApp( self,
                                ssaa=0,
                                fullscreen=True,
                                disableMouseCursor=True)

    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    setupUiCamera( app=self, eye = vec3(0,0,0), tgt=vec3(0,0,1), constrainZ=True, up=vec3(0,1,0))
    self.uicam.rotOnMove = True 

    ##############################################

    self.player_transform = None
    self.spawncounter = 0
    self.framecounter = 0

  ##############################################

  def createPlayerData(self,ctx):

    arch_player = self.ecsscene.declareArchetype("PlayerArchetype")
    c_scenegraph = arch_player.declareComponent("SceneGraphComponent")
    c_physics = arch_player.declareComponent("BulletObjectComponent")

    ######################################
    # scenegraph setup for player
    ######################################

    if False: # dont really need in first person mode
      drawable = ModelDrawableData("data://tests/pbr_calib.glb")
      viz_xf = Transform() # non uniform scale for sphere (to make it a capsule)
      viz_xf.nonUniformScale = vec3(1,1,3)

      c_scenegraph.declareNodeOnLayer( name="playernode",
                                       drawable=drawable,
                                       layer=LAYERNAME,
                                       transform=viz_xf)

    ######################################
    # physics setup for player
    ######################################
    
    capsule = ecs.BulletShapeCapsuleData()
    capsule.radius = 1.0
    capsule.extent = 3.0

    c_physics.mass = 10.0
    c_physics.friction = 0.1
    c_physics.restitution = 0.01
    c_physics.angularDamping = 0.5
    c_physics.linearDamping = 0.5
    c_physics.allowSleeping = False
    c_physics.isKinematic = False
    c_physics.disablePhysics = False
    c_physics.angularFactor = vec3(0,1,0)
    c_physics.shape = capsule
    c_physics.groupAssign = GROUP_PLAYER
    c_physics.groupCollidesWith = GROUP_BALL|GROUP_ENV


    if False:
      def onCollision(table):
        pa = table[tokens.pointA]
        pb = table[tokens.pointB]
        nB = table[tokens.normalOnB]
        ga = table[tokens.groupA]
        gb = table[tokens.groupB]
        ball_and_player =  (ga == GROUP_PLAYER and gb == GROUP_BALL)
        ball_and_player |= (ga == GROUP_BALL and gb == GROUP_PLAYER)
        if ball_and_player:
          print("COLLISION: pa<%s> pb<%s> nb<%s> " % (pa,pb,nB) )
      c_physics.onCollision( onCollision )

    self.playerforce = ecs.DirectionalForceData()
    c_physics.declareForce("playerforce",self.playerforce)
    self.playerforce_rot = 0.0

    ######################################
    # statically spawned player entity
    ######################################

    player_spawner = self.ecsscene.declareSpawner("player_spawner")
    player_spawner.archetype = arch_player
    player_spawner.autospawn = True
    player_spawner.transform.translation = vec3(0,5,-35)
    player_spawner.transform.orientation = quat(vec3(1,0,0),math.pi*0.5)

    ######################################
    # catch the player entity's transform
    #  invoked on update thread 
    #    when entity is actually spawned.
    ######################################

    def onSpawn(table):
      entity = table[tokens.entity]
      self.player_transform = entity.transform

    player_spawner.onSpawn(onSpawn)
    
    self.player_physics_componentdata = c_physics

  ##############################################

  def createBallData(self,ctx):

    arch_ball = self.ecsscene.declareArchetype("BallArchetype")
    c_scenegraph = arch_ball.declareComponent("SceneGraphComponent")
    c_physics = arch_ball.declareComponent("BulletObjectComponent")

    sphere = ecs.BulletShapeSphereData()
    sphere.radius = 1.0


    c_physics.mass = 1.0
    c_physics.friction = 0.3
    c_physics.restitution = 0.45
    c_physics.angularDamping = 0.01
    c_physics.linearDamping = 0.01
    c_physics.allowSleeping = False
    c_physics.isKinematic = False
    c_physics.disablePhysics = False
    c_physics.shape = sphere
    c_physics.groupAssign = GROUP_BALL
    c_physics.groupCollidesWith = GROUP_ALL

    ball_drawable = ModelDrawableData("data://tests/pbr_calib.glb")
    c_scenegraph.declareNodeOnLayer( name="ballnode",
                                     drawable=ball_drawable,
                                     layer=LAYERNAME)

    ball_spawner = self.ecsscene.declareSpawner("ball_spawner")
    ball_spawner.archetype = arch_ball
    ball_spawner.autospawn = False

  ##############################################
  # generate the environment
  ##############################################

  def createEnvironmentData(self,ctx):

    arch_room = self.ecsscene.declareArchetype("RoomArchetype")
    c_scenegraph = arch_room.declareComponent("SceneGraphComponent")
    c_physics = arch_room.declareComponent("BulletObjectComponent")

    #########################
    # physics for room
    #########################

    shape = ecs.BulletShapeMeshData()
    shape.meshpath = "data://tests/environ/envtest2.obj"
    shape.scale = vec3(5,8,5)
    shape.translation = vec3(0,0.01,0) # offset to avoid z-fighting of physics debugger

    c_physics.mass = 0.0
    c_physics.allowSleeping = True
    c_physics.isKinematic = False
    c_physics.disablePhysics = False
    c_physics.shape = shape
    c_physics.groupAssign = GROUP_ENV
    c_physics.groupCollidesWith = GROUP_ALL

    #########################
    # visible mesh for room
    #########################

    room_drawable = ModelDrawableData("data://tests/environ/roomtest.glb")
    
    room_mesh_transform = Transform()
    room_mesh_transform.nonUniformScale = vec3(5,8,5)
    room_mesh_transform.translation = vec3(0,-0.05,0)

    room_node = c_scenegraph.declareNodeOnLayer( name = "envnode",
                                                 drawable = room_drawable,
                                                 layer = LAYERNAME,
                                                 transform = room_mesh_transform)
    
    env_spawner = self.ecsscene.declareSpawner("env_spawner")
    env_spawner.archetype = arch_room
    env_spawner.autospawn = True
    env_spawner.transform.translation = vec3(0,-10,0) # pull ground down
    env_spawner.transform.scale = 1.0

  ##############################################

  def onGpuInit(self,ctx):
    
    ####################
    # create ECS scene
    ####################

    self.ecsscene = ecs.SceneData()

    ####################
    # create (graphics) scenegraph system
    ####################

    self.systemdata_scenegraph = self.ecsscene.declareSystem("SceneGraphSystem")
    self.systemdata_scenegraph.declareLayer(LAYERNAME)
    self.systemdata_scenegraph.declareParams({
      "SkyboxIntensity": float(1.5),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.1),
      "DepthFogDistance": float(2000),
      "DepthFogPower": float(1.25),
    })

    ####################
    # create physics system
    ####################

    systemdata_phys = self.ecsscene.declareSystem("BulletSystem")
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

    self.controller.systemNotify( self.sys_sg,tokens.ResizeFromMainSurface,True)

    #SAD = ecs.SpawnAnonDynamic("player_spawner")
    #SAD.overridexf.orientation = quat(vec3(1,0,0),math.pi*0.5)
    #SAD.overridexf.scale = 1.0
    #SAD.overridexf.translation = vec3(0,5,-25)
    #self.controller.spawnEntity(SAD)

    ##################
    # install rendercallback on ezapp
    #  (so the ezapp will render the ecs scene from C++)
    ##################

    self.controller.installRenderCallbackOnEzApp(self.ezapp)

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

    prob = random.randint(0,100)
    if prob < 5 and self.spawncounter < NUM_BALLS:
      i = random.randint(-5,5)
      j = random.randint(-5,5)
      self.spawncounter += 1
      SAD = ecs.SpawnAnonDynamic("ball_spawner")
      SAD.overridexf.orientation = quat(vec3(0,1,0),0)
      SAD.overridexf.scale = 1.0
      SAD.overridexf.translation = vec3(i,15,j)
      self.controller.spawnEntity(SAD)
      
    ##############################
    # camera update
    ##############################

    PXF = self.player_transform
    if PXF is not None:
      UIC = self.uicam.cameradata
      ROT = self.playerforce_rot
      DIR = (UIC.target-UIC.eye).normalized()
      # rot around Y by ROT
      MOTION_DIR = vec3(DIR.x,DIR.y,DIR.z)
      MOTION_DIR.roty(ROT)
            
      self.playerforce.direction = MOTION_DIR

      # throttle camera updates
      #  to reduce ecs controller traffic
      if (updinfo.counter%3)==0:

        OFFSET = vec3(0,0.5,0)
        EYE = PXF.translation+OFFSET
        TGT = EYE + DIR
        UP = vec3(0,1,0)
        
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
                                    tokens.component: self.player_physics_componentdata,
                                    tokens.impulse: impulse
                                  })

  ##############################################

  def onUiEvent(self,uievent):

    walk_force = 3e2

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
        self.playerImpulse(vec3(0,500,0)) # JUMP
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
