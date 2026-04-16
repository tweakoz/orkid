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

CAMERA_HEIGHT_ABOVE_TERRAIN = 1.0
TERRAIN_FADE_NEAR = 450.0
TERRAIN_FADE_FAR  = 500.0

def terrain_height(x, z):
  mtn_wave   = (math.sin(x * 0.005 + 0.3) * math.cos(z * 0.005 * 0.8 + 0.7)
              + math.sin(x * 0.005 * 1.3 - z * 0.005 * 0.4) * 0.5)
  large_wave = (math.sin(x * 0.015) * math.cos(z * 0.015 * 0.7)
              + math.cos(x * 0.015 * 0.6 + z * 0.015) * 0.6)
  med_wave   = (math.sin(x * 0.04 + z * 0.04 * 0.5)
              * math.cos(z * 0.04 * 1.3))
  small_wave = (math.sin(x * 0.12 * 1.1)
              * math.sin(z * 0.12 * 0.9))
  fine_wave  = (math.sin(x * 0.3 * 1.2 + z * 0.3 * 0.7)
              * math.cos(z * 0.3 * 1.1))
  return (mtn_wave * 80.0
        + large_wave * 30.0
        + med_wave * 12.0
        + small_wave * 4.0
        + fine_wave * 1.5)

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
      tgt=vec3(0, 15, 0),
      up=vec3(0, 1, 0),
      far = 10000.0,
      grid_variant=None
    )

    # WASD movement state
    self.move_vel = vec2(0, 0)
    self.pos_offset = vec3(0, 0, 0)
    self.move_speed = 40.0
    self.smoothed_height = terrain_height(0, 0) + CAMERA_HEIGHT_ABOVE_TERRAIN

    self.createEzApp(ssaa=2)

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
    gmtl.rasterstate.setBlendingMacro(tokens.ALPHA)

    fs = gmtl.freestyle
    param_fade_near = fs.param("terrainFadeNear")
    param_fade_far  = fs.param("terrainFadeFar")
    if param_fade_near:
      gmtl.bindParam(param_fade_near, TERRAIN_FADE_NEAR)
    if param_fade_far:
      gmtl.bindParam(param_fade_far, TERRAIN_FADE_FAR)
    param_base_quad_size = fs.param("BaseQuadSize")
    if param_base_quad_size:
      gmtl.bindParam(param_base_quad_size, 1.0)

    #######################################
    # ground drawable
    #######################################

    gdata = GeoClipMapDrawable()
    gdata.pbrmaterial = gmtl
    gdata.numLevels = 16
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

    # Apply WASD movement in camera-relative direction (XZ only)
    move_dir = zdir * self.move_vel.y + xdir * self.move_vel.x
    self.pos_offset += move_dir * self.move_speed * DT

    # Sample terrain height at current XZ and smooth toward it
    cam_x = self.pos_offset.x
    cam_z = self.pos_offset.z
    target_y = terrain_height(cam_x, cam_z) + CAMERA_HEIGHT_ABOVE_TERRAIN
    min_y = terrain_height(cam_x, cam_z) + 1.0
    alpha = 1.0 - math.exp(-DT / 1.0)
    self.smoothed_height += (target_y - self.smoothed_height) * alpha
    if self.smoothed_height < min_y:
      # Fast catch-up to avoid going underground
      rescue_alpha = 1.0 - math.exp(-DT / 0.1)
      self.smoothed_height += (min_y - self.smoothed_height) * rescue_alpha
    self.pos_offset = vec3(cam_x, self.smoothed_height, cam_z)

    # Update camera position offset (direction still from uicam)
    uicam.positionOffset = self.pos_offset
    uicam.updateMatrices()
    self.SGC.camera.copyFrom(uicam.cameradata)

  ################################################
  # UI event handler for WASD
  ################################################

  def _onUiEvent(self, uievent):
    code = uievent.code

    if code == 2634741946:  # key down
      keycode = uievent.keycode
      if keycode == ord('W'):
        self.move_vel = vec2(self.move_vel.x, 1)
        return ui.HandlerResult()
      elif keycode == ord('S'):
        self.move_vel = vec2(self.move_vel.x, -1)
        return ui.HandlerResult()
      elif keycode == ord('A'):
        self.move_vel = vec2(-1, self.move_vel.y)
        return ui.HandlerResult()
      elif keycode == ord('D'):
        self.move_vel = vec2(1, self.move_vel.y)
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
