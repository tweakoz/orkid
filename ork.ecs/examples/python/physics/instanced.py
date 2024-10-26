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
from orkengine.core import vec3, vec4, quat, CrcStringProxy, VarMap, Transform
from orkengine import lev2, ecs
this_dir = Path(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(str(obt_path.orkid()/"ork.lev2"/"examples"/"python")) # add parent dir to path
from ork import path as ork_path
sys.path.append(str(ork_path.lev2_pylib)) # add parent dir to path
from lev2utils.cameras import setupUiCamera

################################################################################
tokens = CrcStringProxy()
LAYERNAME = "std_deferred"
NUM_BALLS = 3500
BALLS_NODE_NAME = "balls-instancing-node"
################################################################################

class ECS_INSTANCED(object):

  ##############################################

  def __init__(self):
    super().__init__()
    self.ezapp = ecs.createApp(self,ssaa=2,fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera( app=self, eye = vec3(50), tgt=vec3(0,0,1), constrainZ=True, up=vec3(0,1,0))
    self.ecsInit()

  ##############################################

  def ecsInit(self):

    ####################
    # create ECS scene
    ####################

    self.ecsscene = ecs.SceneData()

    ##############################################
    # setup global physics 
    ##############################################

    systemdata_phys = self.ecsscene.declareSystem("BulletSystem")
    systemdata_phys.timeScale = 1.0
    systemdata_phys.simulationRate = 60.0
    systemdata_phys.debug = False
    systemdata_phys.linGravity = vec3(0,-9.8*3,0)

    ####################
    # create scenegraph
    ####################


    systemdata_SG = self.ecsscene.declareSystem("SceneGraphSystem")
    systemdata_SG.declareLayer(LAYERNAME)
    systemdata_SG.declareParams({
      "SkyboxIntensity": float(2.0),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.1),
      "DepthFogDistance": float(2000),
      "DepthFogPower": float(1.25),
    })

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
    c_physics.allowSleeping = True
    c_physics.isKinematic = False
    c_physics.disablePhysics = False
    c_physics.shape = sphere

    ############################
    # connect to instancing tech
    ############################

    nid = lev2.scenegraph.NodeInstanceData(BALLS_NODE_NAME)
    c_physics.declareNodeInstance(nid)
    c_scenegraph.declareNodeInstance(nid)

    ############################

    ball_spawner = self.ecsscene.declareSpawner("ball_spawner")
    ball_spawner.archetype = arch_ball
    ball_spawner.autospawn = False

  ##############################################

  def createEnvironmentData(self):

    arch_env = self.ecsscene.declareArchetype("RoomArchetype")
    c_scenegraph = arch_env.declareComponent("SceneGraphComponent")
    c_physics = arch_env.declareComponent("BulletObjectComponent")

    #########################
    # physics for room
    #########################

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

    drawable = lev2.ModelDrawableData("data://tests/environ/roomtest.glb")
    
    mesh_transform = Transform()
    mesh_transform.nonUniformScale = vec3(5,8,5)
    mesh_transform.translation = vec3(0,-0.05,0)

    room_node = c_scenegraph.declareNodeOnLayer( name = "envnode",
                                                 drawable = drawable,
                                                 layer = LAYERNAME,
                                                 transform = mesh_transform)
    
    env_spawner = self.ecsscene.declareSpawner("env_spawner")
    env_spawner.archetype = arch_env
    env_spawner.autospawn = True
    env_spawner.transform.translation = vec3(0,-10,0)
    env_spawner.transform.scale = 1.0
    
  ##############################################

  def onUpdateInit(self):
    pass

  ##############################################

  def onGpuInit(self,ctx):
    self.ecsLaunch()

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
    if prob < 40 and self.spawncounter < NUM_BALLS:
      self.spawncounter += 1
      SAD = ecs.SpawnAnonDynamic("ball_spawner")
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

    return lev2.ui.HandlerResult()

###############################################################################

ECS_INSTANCED().ezapp.mainThreadLoop()
