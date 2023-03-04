#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, sys, os, argparse, asyncio
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/".."/".."/"examples"/"python").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

################################################################################

midictrl = -1 

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-m", "--movie", action="store_true", help='do movie record' )
parser.add_argument("-c", "--midictrl", type=int, default=-1, help='midi controller index')
parser.add_argument("-l", "--listmidictrl", action="store_true", help='list midi controllers' )

args = vars(parser.parse_args())
if args["listmidictrl"]:
  for item in midi.InputContext().inputs:
    print(item)
  sys.exit(0)

midictrl = args["midictrl"]

class CompositorSetupApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    if args["movie"]:
      self.ezapp.enableMovieRecording("test.mp4")

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

    self.deferred_context = comp_tek.renderNode.context
    self.deferred_context.gpuInit(ctx) # early init
    lighting_mtl = self.deferred_context.lightingMaterial
    self.envlighting_pipeline = self.deferred_context.pipeline_envlighting_model0_mono
    print(lighting_mtl)
    print(self.envlighting_pipeline)
    
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
    comp_tek.renderNode.overrideShader("ork_lev2://examples/python/common/compositorsetup.glfx")

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

    self.scene.enableSynchro()

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    if midictrl>=0:
      def _ON_MIDI(t,m):
        if m[0]==176:
          c = m[1]
          v = m[2]
          if c==0: # controller 0
            pbr_common.ambientLevel = vec3(float(v)/127.0)
          if c==1: # controller 1
            pbr_common.diffuseLevel = float(v)/127.0
          if c==2: # controller 2
            pbr_common.specularLevel = float(v)/127.0

        print(m)
      self.midi = midi.InputContext()
      self.midi.startInputByIndex(2,_ON_MIDI)

  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

###############################################################################

CompositorSetupApp().ezapp.mainThreadLoop()
