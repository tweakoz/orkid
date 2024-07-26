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
from orkengine.core import vec3, vec4, quat
from orkengine.core import CrcStringProxy, VarMap, Transform
from orkengine import lev2, ecs
this_dir = Path(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(str(obt_path.orkid()/"ork.lev2"/"examples"/"python")) # add parent dir to path
from ork import path as ork_path
from lev2utils.cameras import *

################################################################################
tokens = CrcStringProxy()
LAYERNAME = "std_forward"
GROUP_PLAYER = 1
GROUP_BALL = 2
GROUP_ENV = 4
GROUP_ALL = GROUP_PLAYER | GROUP_BALL | GROUP_ENV
NUM_BALLS = 1500
OFFSET = vec3(0,0.5,0)
SIMRATE = 120
BALLS_NODE_NAME = "balls-instancing-node"
SSAO_NUM_SAMPLES = 32
################################################################################

class ECS_FIRST_PERSON_SHOOTER(object):

  ##############################################

  def __init__(self):
    super().__init__()

    self.ezapp = ecs.createApp( self,
                                ssaa=0,
                                fullscreen=False,
                                left = 20,
                                top = 42,
                                width = 1680,
                                height = 900,
                                
                                disableMouseCursor=True)

    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera( app=self, 
                   eye = vec3(0,0,0), 
                   tgt=vec3(0,0,1), 
                   constrainZ=True, 
                   up=vec3(0,1,0),
                   near=0.1,
                   far = 1000.0 )
    self.uicam.rotOnMove = True 

    ##############################################

    self.player_transform = None
    self.spawncounter = 0
    self.gpuframecounter = 0
    self.gpuframecounterUP = 0
    
    self.ecsInit()

  ##############################################

  def ecsInit(self):
    ####################
    # create ECS scene
    ####################

    self.ecsscene = ecs.SceneData()

    ####################
    # create (graphics) scenegraph system
    ####################

    systemdata_SG = self.ecsscene.declareSystem("SceneGraphSystem")
    systemdata_SG.declareLayer(LAYERNAME)
    systemdata_SG.declareParams({
      "SkyboxIntensity": float(2.5),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.0),
      "DepthFogDistance": float(2000),
      "DepthFogPower": float(1.25),
      "SSAONumSamples": SSAO_NUM_SAMPLES,
      "SSAONumSteps": 8,
      "SSAOBias": -3e-4,
      "SSAORadius": 1.0*25.4/1000, # 2 inches
      "SSAOWeight": 1.0,
      "SSAOPower": 0.75,
      "preset": "DeferredPBR"
    })
    
    self.systemdata_scenegraph = systemdata_SG

    ####################
    # create physics system
    ####################

    systemdata_phys = self.ecsscene.declareSystem("BulletSystem")
    systemdata_phys.timeScale = 1.0
    systemdata_phys.simulationRate = SIMRATE
    systemdata_phys.debug = False
    systemdata_phys.linGravity = vec3(0,-9.8*3,0)

    self.systemdata_phys = systemdata_phys

    drawable = lev2.InstancedModelDrawableData("data://tests/pbr_calib.glb")
    drawable.resize(NUM_BALLS)
    systemdata_SG.declareNodeOnLayer( name=BALLS_NODE_NAME,
                                      drawable=drawable,
                                      layer=LAYERNAME)

    ####################
    # create archetype/entity data
    ####################

    self.createBallData()
    self.createEnvironmentData()
    self.createPlayerData()
    self.createProjectileData()

  ##############################################

  def ecsLaunch(self):

    ##################
    # create / bind controller
    ##################

    self.controller = ecs.Controller()
    self.controller.bindScene(self.ecsscene)

    ##################
    # install rendercallback on ezapp
    #  This is so the ezapp will render the ecs scene from C++,
    #   without the need for the c++ to call into python on the render thread
    ##################

    self.controller.installRenderCallbackOnEzApp(self.ezapp)

    ##################
    # create / launch simulation
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

  ##############################################

  def createPlayerData(self):

    arch_player = self.ecsscene.declareArchetype("PlayerArchetype")
    c_scenegraph = arch_player.declareComponent("SceneGraphComponent")
    c_physics = arch_player.declareComponent("BulletObjectComponent")

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

    self.ball_state = dict()
    
   
    def incrBallState(id):
      state = 0
      if id in self.ball_state.keys():
        state = self.ball_state[id]+1
      self.ball_state[id] = state
      return state
    
    
    if True:
      def onCollision(table):
        ea = table[tokens.entityA]
        eb = table[tokens.entityB]
        if ea.spawner.archetype == arch_player: # A is player
          ga = table[tokens.groupA]
          gb = table[tokens.groupB]
          if (gb == GROUP_BALL):
            erB = table[tokens.entrefB]
            sgcomp = self.controller.findComponent(erB,"SceneGraphComponent")
            state = incrBallState(erB.id)
            hsv = vec3()
            hsv.x = float(state)*0.1
            hsv.y = 1.0
            hsv.z = 0.5
            rgb = hsv.hsv2rgb()
            self.controller.componentNotify(sgcomp,tokens.ChangeModColor,vec4(rgb,1))
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

  def createBallData(self):

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

    ############################
    # connect to instancing tech
    ############################

    nid = lev2.scenegraph.NodeInstanceData(BALLS_NODE_NAME)
    c_physics.declareNodeInstance(nid)
    c_scenegraph.declareNodeInstance(nid)

    #ball_drawable = ModelDrawableData("data://tests/pbr_calib.glb")
    #c_scenegraph.declareNodeOnLayer( name="ballnode",
    #                                 drawable=ball_drawable,
    #                                 layer=LAYERNAME)

    ball_spawner = self.ecsscene.declareSpawner("ball_spawner")
    ball_spawner.archetype = arch_ball
    ball_spawner.autospawn = False

  ##############################################

  def createProjectileData(self):

    arch_ball = self.ecsscene.declareArchetype("ProjectileArchetype")
    c_scenegraph = arch_ball.declareComponent("SceneGraphComponent")
    c_physics = arch_ball.declareComponent("BulletObjectComponent")

    sphere = ecs.BulletShapeSphereData()
    sphere.radius = 0.5


    c_physics.mass = 5.0
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

    ball_drawable = lev2.ModelDrawableData("data://tests/pbr_calib.glb")
    c_scenegraph.declareNodeOnLayer( name="projnode",
                                     drawable=ball_drawable,
                                     layer=LAYERNAME,
                                     modcolor = vec4(0,0,1,0))

    proj_spawner = self.ecsscene.declareSpawner("proj_spawner")
    proj_spawner.archetype = arch_ball
    proj_spawner.transform.scale = 0.5
    proj_spawner.autospawn = False
    
  ##############################################
  # generate the environment
  ##############################################

  def createEnvironmentData(self):

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

    room_drawable = lev2.ModelDrawableData("data://tests/environ/roomtest.glb")
    
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
    self.ecsLaunch()

  ##############################################

  def onGpuExit(self,ctx): # clean up
    self.controller.stopSimulation()
    self.controller.beginWriteTrace

  ##############################################

  def onUpdate(self,updinfo):

    ##############################
    # spawn balls
    ##############################

    prob = random.randint(0,100)
    if prob < 50 and self.spawncounter < NUM_BALLS:
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
      DIR = (UIC.target-UIC.eye).normalized
      # rot around Y by ROT
      MOTION_DIR = vec3(DIR.x,DIR.y,DIR.z)
      MOTION_DIR.roty(ROT)
            
      self.playerforce.direction = MOTION_DIR

      # throttle camera updates
      #  to reduce ecs controller traffic
      if self.ezapp.shouldUpdateThrottleOnGPU:

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
                                  tokens.IMPULSE_ON_COMPONENT_DATA,
                                  {
                                    tokens.component: self.player_physics_componentdata,
                                    tokens.impulse: impulse
                                  })

  ##############################################

  def fireProjectile(self):
    PXF = self.player_transform
    if PXF is not None:
      EYE = PXF.translation+OFFSET
      UIC = self.uicam.cameradata
      ROT = self.playerforce_rot
      DIR = (UIC.target-UIC.eye).normalized

      SAD = ecs.SpawnAnonDynamic("proj_spawner")
      SAD.overridexf.orientation = quat(vec3(0,1,0),0)
      SAD.overridexf.scale = 1.0
      SAD.overridexf.translation = EYE+DIR*5.0
      ent = self.controller.spawnEntity(SAD)
      c_physics = self.controller.findComponent(ent,"EcsBulletObjectComponent")
      
      def LAUNCH_OP():
        self.controller.systemNotify( self.sys_phys,
                                      tokens.IMPULSE_ON_COMPONENT,
                                      {
                                        tokens.component: c_physics,
                                        tokens.impulse: DIR*350.0
                                      })
    
      self.controller.realtimeDelayedOperation(0.1,LAUNCH_OP)
      
  ##############################################

  def onUiEvent(self,uievent):

    ui = lev2.ui

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
      #### FIRE #####
      elif uievent.keycode == ord("F"):
        self.fireProjectile()
    ##############################################
    elif uievent.code == tokens.KEY_UP.hashed:
      if uievent.keycode in [ord("W"),ord("A"),ord("S"),ord("D")]:
        self.playerforce.magnitude = 0.0

    return ui.HandlerResult()

###############################################################################

ECS_FIRST_PERSON_SHOOTER().ezapp.mainThreadLoop()
