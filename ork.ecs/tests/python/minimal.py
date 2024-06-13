#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os
from pathlib import Path
from orkengine import core, lev2, ecs
from ork import path as ork_path

print(ork_path.lev2_pylib)
sys.path.append(str(ork_path.lev2_pylib)) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createFrustumPrim, createGridData
from lev2utils.scenegraph import createSceneGraph

tokens = core.CrcStringProxy()
################################################################################

class MinimalSceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = ecs.createApp(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera( app=self, eye = vec3(10,10,10), constrainZ=True, up=vec3(0,1,0))

    self.ecsscene = ecs.SceneData()
    self.a1 = self.ecsscene.createArchetype("a1")
    self.comp_sg = self.a1.createComponent("SceneGraphComponent")
    self.comp_phys = self.a1.createComponent("BulletObjectComponent")


    self.sysd_phys = self.ecsscene.createSystem("BulletSystem")
    self.sysd_sg = self.ecsscene.createSystem("SceneGraphSystem")

    self.controller = ecs.Controller()
    self.controller.bindScene(self.ecsscene)

    print("comp_sg",self.comp_sg)
    print("comp_phys",self.comp_phys)
    print("sysd_phys",self.sysd_phys)
    print("sysd_sg",self.sysd_sg)

    print("controller",self.controller)

    self.controller.createSimulation()
    self.controller.startSimulation()


  def onGpuInit(self,ctx):

    self.sys_phys = self.controller.findSystem("BulletSystem")
    print(self.sys_phys)

    self.controller.systemNotify(self.sys_phys,tokens.YO,True)

    #createSceneGraph(app=self,rendermodel="DeferredPBR")
    #pmatrix = fmtx4_to_dmtx4(ctx.perspective(45,1,0.25,3))
    #vmatrix = dmtx4.lookAt(dvec3(0, 0, 0),  # eye
    #                      dvec3(0, 0, -1), # tgt
    #                      dvec3(0, 1, 0))  # up
    #frustum_prim = createFrustumPrim(ctx=ctx,vmatrix=vmatrix,pmatrix=pmatrix)
    #pipeline = createPipeline( app = self,
    #                           ctx = ctx,
    #                           techname = "std_mono",
    #                           rendermodel = "DeferredPBR" )
    #self.primnode = frustum_prim.createNode("node1",self.layer1,pipeline)
    #self.grid_data = createGridData()
    #self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    #self.grid_node.sortkey = 1

  def onGpuExit(self,ctx):
    self.controller.stopSimulation()

  def onUpdate(self,updinfo):
    self.controller.updateSimulation()
    θ    = updinfo.absolutetime * math.pi * 2.0 * 0.1
    x = math.sin(θ)*10
    z = -math.cos(θ)*10
    y = 5+math.sin(θ*1.7)*2

    m_world_to_view = mtx4.lookAt(vec3(x, y, z), # eye
                                  vec3(0, 0, 0),  # tgt
                                  vec3(0, 1, 0))  # up

    m_view_to_world =  m_world_to_view.inverse

    #self.primnode.worldTransform.directMatrix = m_view_to_world

    ###################################
    #self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

###############################################################################

MinimalSceneGraphApp().ezapp.mainThreadLoop()
