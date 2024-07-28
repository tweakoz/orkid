#!/usr/bin/env python3

################################################################################
# lev2 sample which overrides the compositor
#  specifically it overrides the gbuffer write shader
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, argparse, asyncio
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')

class CompositorSetupApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)

    self.materials = set()
    setupUiCamera( app=self, eye = vec3(10,10,10), constrainZ=True, up=vec3(0,1,0))

  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create compositor
    ###################################

    comp_tek = NodeCompositingTechnique()
    comp_tek.renderNode = DeferredPbrRenderNode()
    comp_tek.outputNode = ScreenOutputNode()

    comp_data = CompositingData()
    comp_scene = comp_data.createScene("scene1")
    comp_sceneitem = comp_scene.createSceneItem("item1")
    comp_sceneitem.technique = comp_tek

    # OVERRIDES

    pbr_common = comp_tek.renderNode.pbr_common
    pbr_common.requestSkyboxTexture("src://envmaps/tozenv_hellscape")
    pbr_common.environmentIntensity = 1
    pbr_common.environmentMipBias = 10
    pbr_common.environmentMipScale = 1
    pbr_common.diffuseLevel = 1
    pbr_common.specularLevel = 1
    pbr_common.specularMipBias = 1
    pbr_common.skyboxLevel = .5
    pbr_common.depthFogDistance = 100
    pbr_common.depthFogPower = 1
    comp_tek.renderNode.overrideShader("ork_lev2://examples/python/lev2utils/compositorsetup.glfx")

    print(comp_sceneitem)
    print(comp_tek)
    print(pbr_common)
    #assert(False)

    ###################################################
    # create scenegraph with custom compositor setup
    ###################################################

    createSceneGraph(app=self,
                     rendermodel="DeferredPBR",
                     params_dict={
                      "preset": "USER",
                      "compositordata": comp_data
                    })

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

###############################################################################

CompositorSetupApp().ezapp.mainThreadLoop()
