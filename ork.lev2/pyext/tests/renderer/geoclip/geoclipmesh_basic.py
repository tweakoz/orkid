#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os
from orkengine.core import vec3, thisdir, CrcStringProxy
from orkengine.lev2 import PBRMaterial, Image, GeoClipMapDrawable
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

################################################################################

class GeoClipMapApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()

    # Configure scenegraph parameters
    sg_params = {
      "preset": "ForwardPBR",
      "SkyboxIntensity": 1.0,
      "SpecularIntensity": 1.0,
      "DiffuseIntensity": 1.0,
      "AmbientLight": vec3(1),
      "DepthFogDistance": 10000.0,
      "DepthFogPower": 2.0,
      "SkyboxTexPathStr": "nebula"
    }

    # Add standard scenegraph component with camera
    self.SGC = self.addComponent(
      "std_scenegraph",
      StandardSceneGraphComponent,
      sg_params=sg_params,
      eye=vec3(0, 15, -15),
      tgt=vec3(0, 0, 0),
      up=vec3(0, 1, 0),
      grid_variant=None
    )

    self.createEzApp(ssaa=0)

  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def _onGpuInit(self, ctx):

    # Get scenegraph and layer from StandardSceneGraphComponent
    self.scene = self.SGC.scenegraph
    self.layer_fwd = self.SGC.layer_fwd

    #######################################
    # ground material
    #######################################

    gmtl = PBRMaterial()
    color = Image.createFromFile("src://effect_textures/white.dds")
    normal = Image.createFromFile("src://effect_textures/default_normal.dds")
    mtlruf = Image.createFromFile("src://effect_textures/white.dds")
    gmtl.assignImages(
      ctx,
      color=color,
      normal=normal,
      mtlruf=mtlruf,
      doConform=True
    )
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.doubleSided = True
    gmtl.shaderpath = str(thisdir()/"geoclipmesh_basic.fxv2")
    gmtl.addBasicStateLambda()
    gmtl.addLightingLambda()
    gmtl.gpuInit(ctx)
    gmtl.rasterstate.setBlendingMacro(tokens.OFF)

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
    self.groundnode = self.scene.createDrawableNodeOnLayers([self.layer_fwd], "geoclip-node", self.drawable_ground)
    self.groundnode.worldTransform.translation = vec3(0, 0, 0)
    self.groundnode.worldTransform.scale = 1

###############################################################################

app = GeoClipMapApp()
app.ezapp.mainThreadLoop()
