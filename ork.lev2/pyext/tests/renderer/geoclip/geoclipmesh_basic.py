#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, argparse
from orkengine.core import vec2, vec3, quat, thisdir, CrcStringProxy
from orkengine.lev2 import PBRMaterial, Image, GeoClipMapDrawable, ui
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

CAMERA_HEIGHT_ABOVE_TERRAIN = 8.0
TERRAIN_FADE_NEAR = 2400.0
TERRAIN_FADE_FAR  = 3500.0
CAMERA_NEAR_PLANE = 0.3         # must match StandardSceneGraphComponent near=
ALPHA_FADE_NEAR_DISTANCE = 2.0  # alpha ramps 0 at near plane → 1 at this distance

# Uniform XYZ scale on the terrain function — features s× wider AND s× taller,
# so slopes (→ normals → lighting) are preserved. Pushes visible detail out to
# farther rings without coarsening close-up sampling.
TERRAIN_FEATURE_SCALE = 1.5

# Slope-following pitch: tilt camera up/down proportional to the terrain
# slope along the current XZ forward direction, smoothed over SLOPE_TAU.
SLOPE_PITCH_GAIN = -0.35   # radians of pitch per unit slope (dh/dxz)
SLOPE_TAU        = 1.0   # smoothing time constant (seconds)
SLOPE_PROBE      = 2.0   # meters ahead to sample for slope estimate

def terrain_height(x, z):
  # Mirror the shader's uniform XYZ scale: evaluate at (x/s, z/s) and scale
  # amplitude by s. Keeps camera-follow height tracking consistent with the
  # rendered terrain.
  s = TERRAIN_FEATURE_SCALE
  x = x / s
  z = z / s
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
  fine2_wave = (math.sin(x * 0.6 * 1.2 + z * 0.6 * 0.7)
              * math.cos(z * 0.6 * 1.1))
  fine3_wave = (math.sin(x * 1.2 * 1.2 + z * 1.2 * 0.7)
              * math.cos(z * 1.2 * 1.1))
  return s * (mtn_wave * 80.0
            + large_wave * 30.0
            + med_wave * 12.0
            + small_wave * 4.0
            + fine_wave * 1.5
            + fine2_wave * 0.75
            + fine3_wave * 0.375)

################################################################################

class GeoClipMapApp(ComponentizedApplication):

  def __init__(self, fullscreen=False):
    super().__init__()
    self._fullscreen = fullscreen

    # Configure scenegraph parameters
    sg_params = {
      "preset": "ForwardPBR",
      "SkyboxIntensity": 1.0,
      "SpecularIntensity": 1.0,
      "DiffuseIntensity": 1.0,
      "AmbientLight": vec3(1),
      "DepthFogDistance": 10000.0,
      "DepthFogPower": 2.0,
      "SkyboxTexPathStr": "ocean"
    }

    # Add standard scenegraph component with camera
    self.SGC = self.addComponent(
      "std_scenegraph",
      StandardSceneGraphComponent,
      sg_params=sg_params,
      eye=vec3(0, 1, 0),
      tgt=vec3(0, 1, 0.1),
      up=vec3(0, 1, 0),
      near = CAMERA_NEAR_PLANE,
      far = 10000.0,
      grid_variant=None
    )

    # WASD movement state
    self.move_vel = vec2(0, 0)
    self.pos_offset = vec3(0, 0, 0)
    self.move_speed = 5.0
    self.smoothed_height = terrain_height(0, 0) + CAMERA_HEIGHT_ABOVE_TERRAIN

    # Arrow-key rotation state (constant angular velocity while held)
    self.yaw_vel   = 0.0  # -1 / 0 / +1
    self.pitch_vel = 0.0  # -1 / 0 / +1
    self.yaw_rate   = 1.5  # rad/sec
    self.pitch_rate = 1.0  # rad/sec

    # Smoothed pitch contribution from terrain slope ahead of the camera.
    self.slope_pitch = 0.0

    # CapsLock toggles autowalk-forward.
    self.autowalk = False

    self.createEzApp(ssaa=4, fullscreen=self._fullscreen)

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

    # bindParam now takes the param NAME (str) directly (was: an FxShaderParam handle).
    gmtl.bindParam("terrainFadeNear",     TERRAIN_FADE_NEAR)
    gmtl.bindParam("terrainFadeFar",      TERRAIN_FADE_FAR)
    gmtl.bindParam("terrainNearFadeDist", ALPHA_FADE_NEAR_DISTANCE)
    gmtl.bindParam("terrainNearPlane",    CAMERA_NEAR_PLANE)
    gmtl.bindParam("terrainFeatureScale", TERRAIN_FEATURE_SCALE)
    gmtl.bindParam("BaseQuadSize",        1.0)

    #######################################
    # ground drawable
    #######################################

    gdata = GeoClipMapDrawable()
    gdata.pbrmaterial = gmtl
    gdata.numLevels = 16
    gdata.ringSize = 256
    gdata.baseQuadSize = 0.5
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

    uicam = self.SGC.uicam
    uicam.explicit_near_far = True

    # Arrow-key rotation: accumulate into heading/elevation. Orientation is
    # composed once below (with slope pitch folded in) so we avoid a second
    # updateMatrices this frame.
    if self.yaw_vel != 0.0:
      dq = quat.createFromAxisAngle(vec3(0, 1, 0), self.yaw_vel * self.yaw_rate * DT)
      uicam.heading = uicam.heading * dq
    if self.pitch_vel != 0.0:
      dq = quat.createFromAxisAngle(vec3(1, 0, 0), self.pitch_vel * self.pitch_rate * DT)
      uicam.elevation = uicam.elevation * dq

    # Use last-frame's zDir for movement/slope sampling — one-frame lag on
    # rotation input is invisible at 60 Hz, and it lets us avoid a second
    # updateMatrices this frame.
    zdir = uicam.zDir
    zdir = vec3(zdir.x, 0, zdir.z)
    zdir.normalize()

    # Compute xdir (right vector)
    UP = vec3(0, 1, 0)
    xdir = zdir.cross(UP)

    # Apply WASD movement in camera-relative direction (XZ only). Autowalk
    # acts like a held-W when no forward/back input is active; pressing W or
    # S still overrides it so the user keeps manual control.
    effective_fwd = self.move_vel.y
    if self.autowalk and effective_fwd == 0.0:
      effective_fwd = 1.0
    move_dir = zdir * effective_fwd + xdir * self.move_vel.x
    self.pos_offset += move_dir * self.move_speed * DT

    # Sample terrain height at current XZ and smooth toward it
    cam_x = self.pos_offset.x
    cam_z = self.pos_offset.z
    h_here = terrain_height(cam_x, cam_z)
    target_y = h_here + CAMERA_HEIGHT_ABOVE_TERRAIN
    min_y = h_here + 1.0
    alpha = 1.0 - math.exp(-DT / 1.0)
    self.smoothed_height += (target_y - self.smoothed_height) * alpha
    if self.smoothed_height < min_y:
      # Fast catch-up to avoid going underground
      rescue_alpha = 1.0 - math.exp(-DT * 4.0)
      self.smoothed_height += (min_y - self.smoothed_height) * rescue_alpha
    self.pos_offset = vec3(cam_x, self.smoothed_height, cam_z)

    # Slope-following pitch: sample terrain SLOPE_PROBE meters ahead along
    # the forward XZ direction, convert to a pitch target, and exp-smooth
    # toward it over SLOPE_TAU seconds. Pitch is applied between heading
    # and the manual arrow-key elevation so it doesn't accumulate into
    # manual pitch state.
    h_ahead = terrain_height(cam_x + zdir.x * SLOPE_PROBE,
                             cam_z + zdir.z * SLOPE_PROBE)
    slope = (h_ahead - h_here) / SLOPE_PROBE
    # Sign: uphill (slope>0) should tilt view upward. In ezuicam's
    # convention pitch-up is NEGATIVE X rotation (mouse-look analog).
    slope_target = -slope * SLOPE_PITCH_GAIN
    slope_alpha  = 1.0 - math.exp(-DT / SLOPE_TAU)
    self.slope_pitch += (slope_target - self.slope_pitch) * slope_alpha

    # Final orientation = manual elevation * slope pitch * heading. Both
    # elevation and slope rotate around local X so their order doesn't
    # affect the result.
    q_slope = quat.createFromAxisAngle(vec3(1, 0, 0), self.slope_pitch)
    uicam.orientation = (uicam.elevation * q_slope) * uicam.heading

    uicam.positionOffset = self.pos_offset
    uicam.updateMatrices()
    self.SGC.camera.copyFrom(uicam.cameradata)

  ################################################
  # UI event handler for WASD
  ################################################

  def _onUiEvent(self, uievent):
    code = uievent.code

    # GLFW arrow-key codes: 262=RIGHT, 263=LEFT, 264=DOWN, 265=UP
    if code == 2634741946:  # key down
      keycode = uievent.keycode
      print("keydown keycode<%d>" % keycode)
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
      elif keycode == 263:  # LEFT
        self.yaw_vel = -0.2
        return ui.HandlerResult()
      elif keycode == 262:  # RIGHT
        self.yaw_vel = 0.2
        return ui.HandlerResult()
      elif keycode == 265:  # UP
        self.pitch_vel = 0.2
        return ui.HandlerResult()
      elif keycode == 264:  # DOWN
        self.pitch_vel = -0.2
        return ui.HandlerResult()
      elif keycode == 280:  # CAPS_LOCK — toggle autowalk
        self.autowalk = not self.autowalk
        print("autowalk<%s>" % self.autowalk)
        return ui.HandlerResult()

    elif code == 957111669:  # key up
      keycode = uievent.keycode
      if keycode == ord('W') or keycode == ord('S'):
        self.move_vel = vec2(self.move_vel.x, 0)
        return ui.HandlerResult()
      elif keycode == ord('A') or keycode == ord('D'):
        self.move_vel = vec2(0, self.move_vel.y)
        return ui.HandlerResult()
      elif keycode == 262 or keycode == 263:  # LEFT/RIGHT
        self.yaw_vel = 0.0
        return ui.HandlerResult()
      elif keycode == 264 or keycode == 265:  # UP/DOWN
        self.pitch_vel = 0.0
        return ui.HandlerResult()

    return None  # Not handled, let parent process

###############################################################################

parser = argparse.ArgumentParser(description='GeoClipMap terrain demo')
parser.add_argument('-f', '--fullscreen', action='store_true', help='Run in fullscreen mode')
args = parser.parse_args()

app = GeoClipMapApp(fullscreen=args.fullscreen)
app.ezapp.mainThreadLoop()
