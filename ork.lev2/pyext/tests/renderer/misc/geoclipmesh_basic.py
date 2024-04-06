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
from common.cameras import *
from common.scenegraph import createSceneGraph
from signal import signal, SIGINT

tokens = CrcStringProxy()

sys.path.append(str(thisdir()/".."/"particles"))
from _ptc_harness import *

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

################################################################################

class GeoClipMapApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=4)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.curtime = 0.0

    setupUiCamera( app=self, #
                   near = 0.1, #
                   far = 10000, #
                   eye = vec3(0,1,-15), #
                   tgt = vec3(0,1,--14), #
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
    sceneparams.SkyboxIntensity = float(1.0)
    sceneparams.SpecularIntensity = float(1.0)
    sceneparams.DiffuseIntensity = float(1.0)
    sceneparams.AmbientLight = vec3(1)
    sceneparams.DepthFogDistance = float(10000)
    sceneparams.DepthFogPower = float(2)
    sceneparams.SkyboxTexPathStr = "src://envmaps/tozenv_nebula.png"

    ###################################
    # create scene
    ###################################

    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd]

    #######################################
    # ground material (water)
    #######################################

    gmtl = PBRMaterial() 
    gmtl.texColor = Texture.load("src://effect_textures/white.dds")
    gmtl.texNormal = Texture.load("src://effect_textures/default_normal.dds")
    gmtl.texMtlRuf = Texture.load("src://effect_textures/white.dds")
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.doubleSided = True
    gmtl.shaderpath = str(thisdir()/"geoclipmesh_basic.glfx")
    gmtl.addLightingLambda()
    gmtl.gpuInit(ctx)
    gmtl.blending = tokens.OFF

    #######################################
    # ground drawable
    #######################################

    gdata = GeoClipMapDrawable()
    gdata.pbrmaterial = gmtl
    gdata.numLevels = 4
    gdata.ringSize = 256
    gdata.baseQuadSize = 1
    gdata.circle = False

    # level0: ringSize * baseQuadSize / 2 = 128 meters radius
    # level1: level0*2 = 256 meters radius
    # level2: level1*2 = 512 meters radius
    # level3 : level2*2 = 1024 meters radius
    
    # total : 1920 meters radius

    self.gdata = gdata
    self.drawable_ground = gdata.createSGDrawable(self.scene)
    self.groundnode = self.scene.createDrawableNodeOnLayers([self.layer_fwd],"geoclip-node",self.drawable_ground)
    self.groundnode.worldTransform.translation = vec3(0,0,0)
    self.groundnode.worldTransform.scale = 1

  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.curtime = updinfo.absolutetime

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

GeoClipMapApp().ezapp.mainThreadLoop()
