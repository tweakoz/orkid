#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os
from orkengine.core import vec2, vec3, thisdir, CrcStringProxy
from orkengine.lev2 import PBRMaterial, Image, GeoClipMapDrawable, ui
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
      eye=vec3(0, 150, -15),
      tgt=vec3(0, 150, 0),
      up=vec3(0, 1, 0),
      grid_variant=None
    )

    # WASD movement state
    self.move_vel = vec2(0, 0)
    self.pos_offset = vec3(0, 0, 0)
    self.move_speed = 150.0

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
    gdata.numLevels = 8
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

  ################################################
  # update callback
  ################################################

  def _onUpdate(self, updinfo):
    DT = updinfo.deltatime

    # Get camera direction (flattened to XZ plane)
    uicam = self.SGC.uicam
    zdir = uicam.zDir
    zdir = vec3(zdir.x, 0, zdir.z)
    zdir.normalize()

    # Compute xdir (right vector)
    UP = vec3(0, 1, 0)
    xdir = zdir.cross(UP)

    # Apply WASD movement in camera-relative direction
    move_dir = zdir * self.move_vel.y + xdir * self.move_vel.x
    self.pos_offset += move_dir * self.move_speed * DT

    # Update camera position offset
    uicam.positionOffset = self.pos_offset
    uicam.updateMatrices()
    self.SGC.camera.copyFrom(uicam.cameradata)

  ################################################
  # UI event handler for WASD
  ################################################

  def _onUiEvent(self, uievent):
    code = uievent.code
    move_speed = 5.0

    if code == 2634741946:  # key down
      keycode = uievent.keycode
      if keycode == ord('W'):
        self.move_vel = vec2(self.move_vel.x, move_speed)
        return ui.HandlerResult()
      elif keycode == ord('S'):
        self.move_vel = vec2(self.move_vel.x, -move_speed)
        return ui.HandlerResult()
      elif keycode == ord('A'):
        self.move_vel = vec2(-move_speed, self.move_vel.y)
        return ui.HandlerResult()
      elif keycode == ord('D'):
        self.move_vel = vec2(move_speed, self.move_vel.y)
        return ui.HandlerResult()

    elif code == 957111669:  # key up
      keycode = uievent.keycode
      if keycode == ord('W') or keycode == ord('S'):
        self.move_vel = vec2(self.move_vel.x, 0)
        return ui.HandlerResult()
      elif keycode == ord('A') or keycode == ord('D'):
        self.move_vel = vec2(0, self.move_vel.y)
        return ui.HandlerResult()

    return None  # Not handled, let parent process

###############################################################################

app = GeoClipMapApp()
app.ezapp.mainThreadLoop()
