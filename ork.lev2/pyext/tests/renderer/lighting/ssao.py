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

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

################################################################################

class SSAOAPP(object):

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
    sceneparams.SpecularIntensity = float(1)
    sceneparams.DiffuseIntensity = float(1)
    sceneparams.AmbientLight = vec3(0.0)
    sceneparams.DepthFogDistance = float(1e6)
    sceneparams.SkyboxTexPathStr = "src://effect_textures/white.dds"
    sceneparams.SSAOMode = 1
    ###################################
    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]
    self.render_node = self.scene.compositorrendernode
    self.pbr_common = self.render_node.pbr_common
    self.pbr_common.useFloatColorBuffer = True
    #######################################
    gmtl = PBRMaterial() 
    gmtl.texColor = Texture.load("src://effect_textures/white.dds")
    gmtl.texNormal = Texture.load("src://effect_textures/default_normal.dds")
    gmtl.texMtlRuf = Texture.load("src://effect_textures/white.dds")
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.baseColor = vec4(.9,.9,1,1)
    gmtl.doubleSided = True
    gmtl.gpuInit(ctx)
    gdata = GroundPlaneDrawableData()
    gdata.pbrmaterial = gmtl
    gdata.extent = 1000.0
    self.gdata = gdata
    self.drawable_ground = gdata.createSGDrawable(self.scene)
    self.groundnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"partgroundicle-node",self.drawable_ground)
    self.groundnode.worldTransform.translation = vec3(0,0,0)
    #######################################
    self.model = XgmModel("data://tests/misc_gltf_samples/DamagedHelmet.glb")
    self.drawable_model = self.model.createDrawable()
    self.modelnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 50
    self.modelnode.worldTransform.translation = vec3(0,15,0)

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

def sig_handler(signal_received, frame):
  print('SIGINT or CTRL-C detected. Exiting gracefully')
  sys.exit(0)

###############################################################################

signal(SIGINT, sig_handler)

SSAOAPP().ezapp.mainThreadLoop()
