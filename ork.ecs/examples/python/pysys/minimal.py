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
LAYERNAME = "std_deferred"
################################################################################

class PYSYS_MINIMAL(object):

  ##############################################

  def __init__(self):
    super().__init__()
    self.ezapp = ecs.createApp(self,ssaa=2,fullscreen=False)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
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

    systemdata_phys = self.ecsscene.declareSystem("PythonSystem")
    #systemdata_phys.timeScale = 1.0
    #systemdata_phys.simulationRate = 240.0
    #systemdata_phys.debug = False
    #systemdata_phys.linGravity = vec3(0,-9.8*3,0)

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

    ####################
    # create archetype/entity data
    ####################

    self.createBallData()

    
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

    self.sys_phys = self.controller.findSystem("PythonSystem")
    self.sys_sg = self.controller.findSystem("SceneGraphSystem")###

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
    c_python = arch_ball.declareComponent("PythonComponent")

    sphere = ecs.BulletShapeSphereData()
    sphere.radius = 1.0

    #c_python.mass = 1.0
    #c_python.friction = 0.3
    #c_python.restitution = 0.45
    #c_python.angularDamping = 0.01
    #c_python.linearDamping = 0.01
    #c_python.allowSleeping = False
    #c_python.isKinematic = False
    #c_python.disablePhysics = False
    #c_python.shape = sphere

    drawable = ModelDrawableData("data://tests/pbr_calib.glb")
    c_scenegraph.declareNodeOnLayer( name="ballnode",drawable=drawable,layer=LAYERNAME)

    ball_spawner = self.ecsscene.declareSpawner("ball_spawner")
    ball_spawner.archetype = arch_ball
    ball_spawner.autospawn = False

  ##############################################

  def onGpuInit(self,ctx):
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
      i = random.randint(-5,5)
      j = random.randint(-5,5)
      prob = random.randint(0,100)
      if prob < 5 and self.spawncounter < 2:
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

PYSYS_MINIMAL().ezapp.mainThreadLoop()
