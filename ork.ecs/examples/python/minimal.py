#!/usr/bin/env python3
################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math, random, argparse
from orkengine.core import *
from orkengine import lev2
from orkengine import ecs
################################################################################
parser = argparse.ArgumentParser(description='ecs example')
################################################################################
args = vars(parser.parse_args())
################################################################################
class EcsApp(object):
    ################################################
    def __init__(self):
        super().__init__()
        self.sceneparams = VarMap()
        self.sceneparams.preset = "PBRDeferred"
        self.ezapp = ecs.createApp(self)
        self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
        self.updinit = True
        self.coreapp = self.ezapp.coreapp()
    ################################################
    # onUpdateInit (always called before onGpuInit() is complete...)
    #  technically called at the beginning of ezapp->runloop()
    ################################################
    def onUpdateInit(self):
        ###########################
        # create drawables
        ###########################
        self.terdd = lev2.TerrainDrawableData()
        self.terdd.rock1 = vec3(1, 1, 1)
        self.terdd.writeHmapPath("src://terrain/testhmap2_2048.png")
        self.terdi = lev2.TerrainDrawableInst(self.terdd)
        self.terdi.worldHeight = 5000.0
        self.terdi.worldSizeXZ = 8192.0
        self.terrainDrawable = self.terdi.createCallbackDrawable()
        ###########################
        # init ECS
        ###########################
        self.scene = ecs.SceneData()
        print(self.scene)
        self.sgsystemdata = self.scene.addSceneGraphSystem()
        self.arch1 = ecs.CompositeArchetype()
        print(self.arch1)
        self.sgcd = self.arch1.addSceneGraphComponent()
        self.scene.addSceneObject(self.arch1)
        self.sim = ecs.Simulation(self.scene,self.coreapp)
        print(self.sim)
        self.sim.onLink( lambda : self.onSimulationLink() )
        self.sim.start()
        ##############################################
    def onSimulationLink(self):
        ###########################
        # fetch SceneGraphSystem / default camera / layer
        ###########################
        self.sgsysteminst = self.sim.sceneGraphSystem()
        self.layer = self.sgsysteminst.defaultLayer
        self.camera = self.sgsysteminst.defaultCamera
        print(self.layer)
        print(self.camera)
        ###########################
        #  create scenegraph nodes
        ###########################
        self.sg_node = self.layer.createDrawableNode("terrain-node", self.terrainDrawable);
        print(self.sg_node)
        self.r_lightdata = lev2.PointLightData()
        self.r_lightdata.color = vec3(1000,0,0)
        self.r_lightnode = self.r_lightdata.createNode("red-light-node",self.layer)
    ##############################################
    def onGpuInit(self,ctx):
        #self.model = lev2.Model("src://environ/objects/misc/ref/torus.glb")
        pass
    ################################################
    def onUpdate(self,updinfo):
        ###################################
        # set camera
        ###################################
        θ    = updinfo.absolutetime * math.pi * 2.0 * 0.1
        distance = 10.0
        height = 300
        eye = vec3(0,height,-10)+vec3(math.sin(θ), 0, -math.cos(θ)) * distance
        self.camera.lookAt(eye, # eye
                           vec3(0, height, 0), # tgt
                           vec3(0, 1, 0)) # up
        ###################################
        # set light
        ###################################
        mtx = mtx4()
        mtx.compose(vec3(0,10,0),quat(),1)
        self.r_lightnode.setMatrix(mtx)
        ###################################
        # update ECS simulation
        ###################################
        self.sim.update()
    ################################################
    def onDraw(self,drawevent):
        ###################################
        # render ECS simulation
        ###################################
        self.sim.render(drawevent)
################################################
app = EcsApp()
app.ezapp.exec()

