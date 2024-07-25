#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random, numpy, argparse
from obt import path
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
lev2_pyexdir.addToSysPath()
from lev2utils.cameras import *
from lev2utils.scenegraph import createSceneGraph
from signal import signal, SIGINT

tokens = CrcStringProxy()
this_dir = Path(os.path.dirname(os.path.abspath(__file__)))

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

################################################################################

class HSVGAPP(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=1)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)

    #self.materials = set()

    setupUiCamera( app=self, #
                   eye = vec3(0,100,150), #
                   constrainZ=True, #
                   up=vec3(0,1,0))

  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################
    sceneparams = VarMap() 
    sceneparams.preset = "ForwardPBR"
    sceneparams.SkyboxIntensity = float(1)
    sceneparams.SpecularIntensity = float(1.2)
    sceneparams.DiffuseIntensity = float(1.0)
    sceneparams.AmbientLight = vec3(0.0)
    sceneparams.DepthFogDistance = float(1e6)
    sceneparams.SkyboxTexPathStr = "src://envmaps/tozenv_caustic1.png"
    ###################################
    # post fx node
    ###################################
    pfx_radial = PostFxNodeUser()
    pfx_radial.shader_path = str(this_dir / "usertest.glfx")
    pfx_radial.technique = "tek_radial_distort"
    pfx_radial.params.mvp = mtx4()
    pfx_radial.params.modcolor = vec4(1,0,0,1)
    pfx_radial.params.time = 0.0
    pfx_radial.gpuInit(ctx,8,8);
    pfx_radial.addToSceneVars(sceneparams,"PostFxChain")
    self.pfx_radial = pfx_radial
    ###################################
    # post fx node
    ###################################
    pfx_feedback = PostFxNodeUser()
    pfx_feedback.shader_path = str(this_dir / "usertest.glfx")
    pfx_feedback.technique = "tek_feedback"
    pfx_feedback.params.mvp = mtx4()
    pfx_feedback.gpuInit(ctx,8,8);
    pfx_feedback.addToSceneVars(sceneparams,"PostFxChain")
    self.pfx_feedback = pfx_feedback
    ###################################
    # post fx node
    ###################################
    pfx_swapchannels = PostFxNodeUser()
    pfx_swapchannels.shader_path = str(this_dir / "usertest.glfx")
    pfx_swapchannels.technique = "tek_swapchannels"
    pfx_swapchannels.params.mvp = mtx4()
    pfx_swapchannels.gpuInit(ctx,8,8);
    pfx_swapchannels.addToSceneVars(sceneparams,"PostFxChain")
    self.pfx_swapchannels = pfx_swapchannels
    ###################################
    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]
    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useFloatColorBuffer = True
    #######################################
    self.model = XgmModel("data://tests/misc_gltf_samples/DamagedHelmet.glb")
    self.drawable_model = self.model.createDrawable()
    self.modelnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 35
    self.modelnode.worldTransform.translation = vec3(0,28,0)

  ################################################

  def onGpuPreFrame(self,ctx):
    self.pfx_feedback.params.FeedbackMap = self.scene.compositorpostnode(2).outputBuffer.texture
    self.pfx_radial.params.FeedbackMap = self.scene.compositorpostnode(2).outputBuffer.texture
    rendernode = self.scene.compositorrendernode
    #deferred_context = rendernode.context
    #gbuffer_group = deferred_context.gbuffer
    #if gbuffer_group is not None:
      #gbuffer = gbuffer_group.mrt_buffer(0)
      #if gbuffer.texture is not None:
        #self.pfx_radial.params.GBufferMap = gbuffer.texture

  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    time = updinfo.absolutetime
    self.pfx_radial.params.time = time*1
    
  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

###############################################################################

def sig_handler(signal_received, frame):
  print('SIGINT or CTRL-C detected. Exiting gracefully')
  sys.exit(0)

###############################################################################

signal(SIGINT, sig_handler)

HSVGAPP().ezapp.mainThreadLoop()
