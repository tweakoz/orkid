#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os
from pathlib import Path
from obt import path as obt_path
from orkengine import core, lev2, ecs

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
    self.ezapp = ecs.createApp(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    #self.materials = set()
    setupUiCamera( app=self, eye = vec3(10,10,10), constrainZ=True, up=vec3(0,1,0))

    self.ecsscene = ecs.SceneData()
    self.a1 = self.ecsscene.createArchetype("BoxArchetype")
    self.a2 = self.ecsscene.createArchetype("GroundArchetype")

    self.spawn_box = self.ecsscene.createSpawnData("spawn_box")
    self.spawn_box.archetype = self.a1
    self.spawn_box.autospawn = False

    self.spawn_gnd = self.ecsscene.createSpawnData("spawn_gnd")
    self.spawn_gnd.archetype = self.a2
    self.spawn_gnd.autospawn = True

    ##############################################
    # setup scene graph
    ##############################################

    self.comp_sgbox = self.a1.createComponent("SceneGraphComponent")
    self.comp_sg = self.a1.createComponent("SceneGraphComponent")
    self.sysd_sg = self.ecsscene.createSystem("SceneGraphSystem")

    self.comp_sggnd = self.a2.createComponent("SceneGraphComponent")

    ##############################################
    # setup physics For Box
    ##############################################

    sphere = ecs.BulletShapeSphereData()
    sphere.radius = 1.0

    c_phys = self.a1.createComponent("BulletObjectComponent")

    c_phys.mass = 1.0
    c_phys.friction = 0.1
    c_phys.restitution = 0.45
    c_phys.angularDamping = 0.1
    c_phys.linearDamping = 0.1
    c_phys.allowSleeping = False
    c_phys.isKinematic = False
    c_phys.disablePhysics = False
    c_phys.shape = sphere

    ##############################################
    # setup physics For global
    ##############################################

    ground = ecs.BulletShapePlaneData()
    ground.normal = vec3(0,1,0)
    ground.position = vec3(0,-6,0)

    ground_phys = self.a2.createComponent("BulletObjectComponent")

    ground_phys.mass = 0.0
    ground_phys.allowSleeping = True
    ground_phys.isKinematic = False
    ground_phys.disablePhysics = True
    ground_phys.shape = ground
    
    ##############################################
    # setup physics For global
    ##############################################

    systemdata_phys = self.ecsscene.createSystem("BulletSystem")
    systemdata_phys.expGravity = vec3(0,-9.8*2,0)
    systemdata_phys.timeScale = 1.0
    systemdata_phys.simulationRate = 240.0
    systemdata_phys.debug = True

    #down_force = ecs.DirectionalForceData()
    #down_force.direction = vec3(0,-1,0)
    #down_force.force = 1.0
    #systemdata_phys.addGlobalForce("downforce",down_force)
    
    
    self.systemdata_phys = systemdata_phys
    self.comp_phys = c_phys

    ##############################################

    self.controller = ecs.Controller()
    self.controller.bindScene(self.ecsscene)

    self.sysd_sg.declareLayer("layer1")

  ##############################################

    print("comp_sg",self.comp_sg)
    print("comp_phys",self.comp_phys)
    print("systemdata_phys",self.systemdata_phys)
    print("sysd_sg",self.sysd_sg)
    print("controller",self.controller)

  ##############################################

  def onGpuInit(self,ctx):
    
    self.cube = prims.createCubePrim(ctx=ctx, size=1.0)
    pipeline = createPipeline( app = self, ctx = ctx, rendermodel="DeferredPBR", techname="std_mono_deferred" )
    self.cube_drawable = self.cube.createDrawableData(pipeline)
    self.comp_sg.declareNodeOnLayer("cube1",self.cube_drawable,"layer1")

    self.controller.createSimulation()
    self.controller.startSimulation()

    self.sys_phys = self.controller.findSystem("BulletSystem")
    self.sys_sg = self.controller.findSystem("SceneGraphSystem")
    print(self.sys_phys)
    print(self.sys_sg)

    self.controller.systemNotify(self.sys_phys,tokens.YO,vec3(0,0,0))
    self.controller.systemNotify( self.sys_sg,tokens.ResizeFromMainSurface,True)



    for i in range(-5,5):
      for j in range(-5,5):
        SAD = ecs.SpawnAnonDynamic("spawn_box")
        SAD.overridexf.orientation = quat(vec3(0,1,0),0)
        SAD.overridexf.scale = 1.0
        SAD.overridexf.translation = vec3(i,15,j)
        self.e1 = self.controller.spawnEntity(SAD)
    
  ##############################################

  def onDraw(self,drawevent):
    self.controller.renderSimulation(drawevent)

  ##############################################

  def onGpuExit(self,ctx):
    self.controller.stopSimulation()

  ##############################################

  def onUpdate(self,updinfo):
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
    self.controller.updateSimulation()
    #self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

###############################################################################

MinimalSceneGraphApp().ezapp.mainThreadLoop()
