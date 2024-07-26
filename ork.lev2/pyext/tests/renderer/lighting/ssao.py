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
from orkengine.core import vec3, vec4, CrcStringProxy, VarMap, lev2_pyexdir
from orkengine import lev2
lev2_pyexdir.addToSysPath()
from lev2utils.cameras import *
from lev2utils.scenegraph import createSceneGraph
from signal import signal, SIGINT

tokens = CrcStringProxy()

SSAO_NUM_SAMPLES = 32
################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

################################################################################

class SSAOAPP(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self,width=1280,height=720,ssaa=0)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)

    #self.materials = set()

    setupUiCamera( app=self, #
                   eye = vec3(0,100,150), #
                   constrainZ=True, #
                   up=vec3(0,1,0))
    
    self.ssaamode = True

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
    sceneparams.preset = "DeferredPBR"
    sceneparams.SkyboxIntensity = float(1)
    sceneparams.SpecularIntensity = float(1)
    sceneparams.DiffuseIntensity = float(1)
    sceneparams.AmbientLight = vec3(0)
    sceneparams.DepthFogDistance = float(1e6)
    sceneparams.SkyboxTexPathStr = "src://effect_textures/white.dds"
    sceneparams.SkyboxTexPathStr = "src://envmaps/tozenv_nebula"
    sceneparams.SSAONumSamples = 32
    sceneparams.SSAONumSteps = 5
    sceneparams.SSAOBias = -1e-5
    sceneparams.SSAORadius = 2.0*25.4/1000.0
    sceneparams.SSAOWeight = 0.75
    sceneparams.SSAOPower = 0.75
    ###################################
    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_std = self.scene.createLayer("std_deferred")
    self.std_layers = [self.layer_std,self.layer_donly]
    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useFloatColorBuffer = True
    self.pbr_common.useDepthPrepass = True
    #######################################
    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    normal = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
    gmtl = lev2.PBRMaterial() 
    gmtl.assignImages(
      ctx,
      color = white,
      normal = normal,
      mtlruf = white,
      doConform=True
    )
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.baseColor = vec4(.9,.9,1,1)
    gmtl.doubleSided = True
    gmtl.gpuInit(ctx)
    gdata = lev2.GroundPlaneDrawableData()
    gdata.pbrmaterial = gmtl
    gdata.extent = 1000.0
    self.gdata = gdata
    self.drawable_ground = gdata.createSGDrawable(self.scene)
    self.groundnode = self.scene.createDrawableNodeOnLayers(self.std_layers,"partgroundicle-node",self.drawable_ground)
    self.groundnode.worldTransform.translation = vec3(0,0,0)
    #######################################
    self.model = lev2.XgmModel("data://tests/misc_gltf_samples/DamagedHelmet.glb")
    self.drawable_model = self.model.createDrawable()
    self.modelnode = self.scene.createDrawableNodeOnLayers(self.std_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 50
    self.modelnode.worldTransform.translation = vec3(0,15,0)

  ################################################

  def onUpdate(self,updinfo):
    abstim = updinfo.absolutetime
    pos = vec3(0,20+math.sin(abstim*1)*10,0)
    self.modelnode.worldTransform.translation = pos
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    if self.ssaamode == True:
      self.pbr_common.ssaoNumSamples = SSAO_NUM_SAMPLES
    else:
      self.pbr_common.ssaoNumSamples = 0
    
  ##############################################

  def onUiEvent(self,uievent):
    ui = lev2.ui
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

def sig_handler(signal_received, frame):
  print('SIGINT or CTRL-C detected. Exiting gracefully')
  sys.exit(0)

###############################################################################

signal(SIGINT, sig_handler)

SSAOAPP().ezapp.mainThreadLoop()
