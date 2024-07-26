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
import trimesh

this_dir = Path(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(str(obt_path.orkid()/"ork.lev2"/"examples"/"python")) # add parent dir to path
from ork import path as ork_path
sys.path.append(str(ork_path.lev2_pylib)) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.submeshes import *
from lev2utils.shaders import createPipeline

################################################################################
tokens = CrcStringProxy()
LAYERNAME = "std_forward"
NUM_BALLS = 8000
RATE = 0.7
SIMRATE = 60
GROUP_STATIC = 1
GROUP_BALL = 2
GROUP_ENV = 4
GROUP_ALL = GROUP_STATIC | GROUP_BALL | GROUP_ENV
BALLS_NODE_NAME = "balls-instancing-node"
SSAO_NUM_SAMPLES = 16
SPAWN_HEIGHT = 25
################################################################################

class ECS_MINIMAL(object):

  ##############################################

  def __init__(self):
    super().__init__()
    self.ezapp = ecs.createApp(self,ssaa=0,fullscreen=False)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    setupUiCamera( app=self, eye = vec3(50), tgt=vec3(0,0,1), constrainZ=True, up=vec3(0,1,0))
    self.ecsInit()
    self.ents = list()
    self.ssaamode = False
    
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
    systemdata_phys.simulationRate = SIMRATE
    systemdata_phys.debug = False
    systemdata_phys.linGravity = vec3(0,-9.8*3,0)
    systemdata_phys.test_deactivation = True

    ####################
    # create scenegraph
    ####################

    systemdata_SG = self.ecsscene.declareSystem("SceneGraphSystem")
    systemdata_SG.declareLayer(LAYERNAME)
    systemdata_SG.declareLayer("depth_prepass")
    #self.layer_donly = self.scene.createLayer("depth_prepass")
    #self.layer_fwd = self.layer1
    #self.fwd_layers = [self.layer_fwd,self.layer_donly]
    systemdata_SG.declareParams({
      "preset": "ForwardPBR",
      "SkyboxIntensity": float(4),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.0),
      "DepthFogDistance": float(2000),
      "DepthFogPower": float(1.25),
      "SSAONumSamples": SSAO_NUM_SAMPLES,
      "SSAONumSteps": 2,
      "SSAOBias": -1.0e-5,
      "SSAORadius": 2.0*25.4/1000.0,
      "SSAOWeight": 0.75,
      "SSAOPower": 0.75,
    })

    drawable = InstancedModelDrawableData("data://tests/pbr_calib_lopoly.glb")
    drawable.resize(NUM_BALLS)
    systemdata_SG.declareNodeOnLayer( name=BALLS_NODE_NAME,
                                      drawable=drawable,
                                      layers=[LAYERNAME,"depth_prepass"])

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

    print(self.sys_sg)

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

    c_physics.mass = 10.0
    c_physics.friction = 0.5
    c_physics.restitution = 0.1
    c_physics.angularDamping = 0.1
    c_physics.linearDamping = 0.1
    c_physics.sleepThresholdLinear = 10
    c_physics.sleepThresholdAngular = 10
    c_physics.allowSleeping = True
    c_physics.isKinematic = False
    c_physics.disablePhysics = False
    c_physics.shape = sphere
    #c_physics.groupAssign = GROUP_BALL
    #c_physics.groupCollidesWith = GROUP_ALL

    ############################
    # connect to instancing tech
    ############################

    nid = lev2.scenegraph.NodeInstanceData(BALLS_NODE_NAME)
    c_physics.declareNodeInstance(nid)
    c_scenegraph.declareNodeInstance(nid)

    ############################

    def onSpawn(table):
      entref = table[tokens.entref]
      #self.controller.entBarrier(entref)
      sgcomp = self.controller.findComponent(entref,"SceneGraphComponent")

    ############################

    ball_spawner = self.ecsscene.declareSpawner("ball_spawner")
    ball_spawner.archetype = arch_ball
    ball_spawner.autospawn = False
    ball_spawner.onSpawn(onSpawn)

  ##############################################

  def createEnvironmentData(self):

    arch_env = self.ecsscene.declareArchetype("RoomArchetype")
    c_scenegraph = arch_env.declareComponent("SceneGraphComponent")
    c_physics = arch_env.declareComponent("BulletObjectComponent")

    #########################
    # physics for room
    #########################

    submesh2=fullBoxQuads(20,20)
    q1 = quat(vec3(1,0,0),math.pi*0.25)
    q2 = quat(vec3(0,1,0),math.pi*0.5)
    tmeshx = submeshToTrimesh(submesh2,vec3(0,25,0),q1,vec3(1))
    tmeshz = submeshToTrimesh(submesh2,vec3(0,20,0),q1*q2,vec3(1))



    submesh=fullBoxQuads(40,20)
    tmesh = submeshToTrimesh(submesh,vec3(0),quat(),vec3(1))

    boolean_out = tmesh.difference(tmeshx)
    boolean_out = boolean_out.difference(tmeshz)
    submesh = trimeshToSubmesh(boolean_out)

    for i in range(0,4):
      tmesh = submeshToTrimesh(submesh,vec3(0),quat(),vec3(1))
      evw = 10+i*10
      evh = 12
      y = 5+i*6
      submesh2=fullBoxQuads(evw,evh)
      tmesh2 = submeshToTrimesh(submesh2,vec3(0,y,0),quat(),vec3(1))

      boolean_out = tmesh.difference(tmesh2)

      
      submesh = trimeshToSubmesh(boolean_out)
      submesh = submesh.withFaceNormals()
      submesh = submesh.withVertexColorsFromNormals()
      submesh = submesh.withBarycentricUVs()

    shape = ecs.BulletShapeMeshData()
    shape.submesh = submesh
    shape.scale = vec3(1)
    #shape.translation = vec3(0,SPAWN_HEIGHT,0)

    c_physics.mass = 0.0
    c_physics.allowSleeping = True
    c_physics.isKinematic = False
    c_physics.disablePhysics = True
    c_physics.shape = shape
    #c_physics.groupAssign = GROUP_ENV
    #c_physics.groupCollidesWith = GROUP_BALL|GROUP_STATIC

    #########################
    # visible mesh for room
    #########################
    
    self.room_SGCOMP = c_scenegraph
    self.room_submesh = submesh
    
    #########################
    # spawner
    #########################
    
    env_spawner = self.ecsscene.declareSpawner("env_spawner")
    env_spawner.archetype = arch_env
    env_spawner.autospawn = True
    env_spawner.transform.translation = vec3(0,-SPAWN_HEIGHT,0)
    env_spawner.transform.scale = 1.0
    
  ##############################################

  def onGpuInit(self,ctx):
    
    #########################
    # need a graphics context
    #  to create room visuals
    #########################

    rprimdata = RigidPrimitiveDrawableData()
    rprimdata.primitive = RigidPrimitive(self.room_submesh,ctx)
    
    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    normal = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
    material = PBRMaterial()
    material.assignImages(
      ctx,
      color = white,
      normal = normal,
      mtlruf = white,
      doConform=True
    )
    material.gpuInit(ctx)
    self.material = material

    fstyle = material.freestyle
    fxcache = material.fxcache

    permu = FxPipelinePermutation()
    permu.rendering_model = "FORWARD_PBR"
    permu.instanced = False
    permu.skinned = False
    permu.is_picking = False
    permu.stereo = False
    permu.has_vtxcolors = False
    self.permu = permu
    #
    #pipeline = material.fxcache.findPipeline(permu) 
    
    #rprimdata.pipeline = pipeline
    rprimdata.material = material
    
    mesh_transform = Transform()
    mesh_transform.scale = 1.0
    mesh_transform.translation = vec3(0,0,0)

    room_node = self.room_SGCOMP.declareNodeOnLayer( name = "envnode",
                                                     drawable = rprimdata,
                                                     layers = [LAYERNAME,"depth_prepass"],
                                                     transform = mesh_transform)

    #########################
    # boot up the ECS
    #########################

    self.ecsLaunch()

  ##############################################

  def onGpuExit(self,ctx):
    self.controller.stopSimulation()

  ##############################################

  def onUpdate(self,updinfo):

    ##############################
    # spawn balls
    ##############################

    if True:
      extent = 10
      i = random.randint(-extent,extent)
      j = random.randint(-extent,extent)
      prob = random.uniform(0,1)
      if prob<RATE and self.spawncounter < NUM_BALLS:
        self.spawncounter += 1
        fi = float(self.spawncounter)/float(NUM_BALLS)
        ifi = 1.0-fi
        SAD = ecs.SpawnAnonDynamic("ball_spawner")
        #SAD.overridexf.orientation = quat(vec3(0,1,0),0)
        #SAD.overridexf.scale = 1.0
        SAD.overridexf.translation = vec3(i,20,j)
        h = random.uniform(0,1)
        v = random.uniform(.25,2)
        rgb = vec3(h,1,v).hsv2rgb()
        SAD.table = ecs.DataTable()
        SAD.table[tokens.modcolor] = vec4(rgb,1)
        mass = 10.0 * pow(ifi,3)
        SAD.table[tokens.mass] = mass
        self.ents += [self.controller.spawnEntity(SAD)]
      num_ents = len(self.ents)

      
    ##############################
    # camera update
    ##############################

    UIC = self.uicam.cameradata
   
    if True:
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

    if self.ssaamode == True:
      self.controller.systemNotify( self.sys_sg,
                                    tokens.UpdatePbrCommon,{
                                      tokens.SSAONumSamples: SSAO_NUM_SAMPLES
                                    }
                                  )
    else:
      self.controller.systemNotify( self.sys_sg,
                                    tokens.UpdatePbrCommon,{
                                      tokens.SSAONumSamples: 0
                                    }
                                  )

    ##############################
    # tick the simulation
    ##############################

    self.controller.updateSimulation()

  ##############################################

  def onUiEvent(self,uievent):
    res = ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      if uievent.keycode == ord("A"):
        if self.ssaamode == True:
          self.ssaamode = False
        else:
          self.ssaamode = True
        print("SSAO MODE",self.ssaamode)
        return res
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    else:
      handled = ui.HandlerResult()
    return res

###############################################################################

ECS_MINIMAL().ezapp.mainThreadLoop()
