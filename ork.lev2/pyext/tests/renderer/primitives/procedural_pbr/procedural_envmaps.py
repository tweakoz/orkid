#!/usr/bin/env ork.python

################################################################################
# Procedural Envmaps Test
# Demonstrates lev2.PbrCommon.makeRadianceMapsSolidColor and
# makeRadianceMapsGradient — in-memory radiance maps with no disk lag.
#
# Press 1: solid color background (cycles through several colors).
# Press 2: static vertical gradient (sky / horizon / ground).
# Press 3: time-varying gradient (day-to-night cycle, rebuilt every frame).
#
# Layout follows scenegraph/shaderballs.py: a 9×9 grid on the floor where the
# X axis sweeps metallic 0→1 and the Z axis sweeps roughness 0→1, with random
# colors per sphere. Lets you see the full PBR matrix against the procedural
# envmap.
#
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, signal, random, colorsys
import numpy as np
from orkengine.core import vec3, vec4, quat, mtx4, CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine.lev2 import RigidPrimitive, MicroMesh

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

################################################################################

def make_sphere_arrays(radius=1.0, n=16):
  verts, norms = [], []
  for i in range(n + 1):
    lat = math.pi * i / n
    for j in range(n * 2):
      lon = math.tau * j / (n * 2)
      x = math.sin(lat) * math.cos(lon)
      y = math.cos(lat)
      z = math.sin(lat) * math.sin(lon)
      verts.append((x * radius, y * radius, z * radius))
      norms.append((x, y, z))
  w = n * 2
  faces = []
  for i in range(n):
    for j in range(w):
      a = i * w + j
      b = a + 1 if j < w - 1 else i * w
      c = a + w
      d = c + 1 if j < w - 1 else (i + 1) * w
      faces.extend([3, a, b, c, 3, b, d, c])
  nv = len(verts)
  verts_np = np.array(verts, dtype=np.float32)
  norms_np = np.array(norms, dtype=np.float32)
  up = np.array([0, 1, 0], dtype=np.float32)
  binormals_np = np.cross(norms_np, up)
  degen = np.linalg.norm(binormals_np, axis=1) < 1e-6
  binormals_np[degen] = np.cross(norms_np[degen], [1, 0, 0])
  lens = np.linalg.norm(binormals_np, axis=1, keepdims=True)
  binormals_np = binormals_np / np.where(lens < 1e-10, 1.0, lens)
  return verts_np, norms_np, binormals_np, faces

################################################################################

# Solid colors to cycle through in mode 1 (linear PBR space).
SOLID_PALETTE = [
  vec3(0.15, 0.15, 0.15),  # neutral gray
  vec3(0.18, 0.06, 0.06),  # crimson
  vec3(0.06, 0.16, 0.08),  # forest
  vec3(0.06, 0.10, 0.20),  # navy
  vec3(0.20, 0.18, 0.06),  # warm yellow
]

# Static "sky to ground" stops for mode 2. Zenith is kept fairly bright so
# the silhouette-top of chrome spheres (which samples V_tex near 0) reads as
# sky, not as a dark cap.
STATIC_GRADIENT = [
  (0.00, vec3(0.22, 0.30, 0.52)),
  (0.48, vec3(0.32, 0.46, 0.72)),
  (0.52, vec3(0.40, 0.40, 0.42)),
  (1.00, vec3(0.06, 0.06, 0.07)),
]

################################################################################

class ProceduralEnvmapsApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self, width=1280, height=720)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 12, 18), tgt=vec3(0, 1, 0))
    signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())
    self.time = 0.0
    self.mode = 2  # default: static gradient
    self.solid_idx = 0
    self.last_radiance_key = None

  ##############################################

  def onGpuInit(self, ctx):
    createSceneGraph(app=self, params_dict={
      # Some skybox path is required by the scenegraph init; it loads, then we
      # immediately replace pbr_common.RadianceMaps with a procedural one.
      "SkyboxTexPathStr": "ork_envmaps|tozenv_basic",
      "SkyboxIntensity":  float(1),
    })

    white_img  = lev2.Image.createFromFile("src://effect_textures/white.dds")
    normal_img = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    verts_np, norms_np, binormals_np, faces = make_sphere_arrays(radius=0.7, n=16)
    nv = len(verts_np)

    self.material_keep_alive = []
    self.prim_keep_alive     = []  # RigidPrimitive holds the GPU cluster data;
                                   # if it's GC'd, render dereferences null
    self.nodes = []

    # 9×9 grid: X axis = metallic 0→1, Z axis = roughness 0→1, random HSV color.
    grid_n   = 9
    spacing  = 1.8
    origin   = -(grid_n - 1) * spacing / 2.0
    random.seed(12)
    for ix in range(grid_n):
      for iz in range(grid_n):
        metallic  = ix / float(grid_n - 1)
        roughness = iz / float(grid_n - 1)

        h = random.uniform(0, 1)
        s = random.uniform(0, 0.7)
        v = random.uniform(0.4, 1.0)
        r, g, b = colorsys.hsv_to_rgb(h, s, v)
        # BGRA pre-swap for MicroMesh's ARGBU32 packing convention.
        colors_np = np.tile(np.array([b, g, r, 1.0],
                                     dtype=np.float32), (nv, 1))

        mesh = MicroMesh.fromVertAndFaceLists(verts_np, faces)
        mesh.updateNormals(norms_np)
        mesh.updateBinormals(binormals_np)
        mesh.updateColors(colors_np)
        prim = RigidPrimitive()
        prim.updateWithMicroMesh(mesh, ctx, tokens.TRIANGLES)

        mtl = lev2.PBRMaterial()
        mtl.assignImages(ctx, color=white_img, normal=normal_img,
                         mtlruf=white_img, doConform=True)
        mtl.baseColor       = vec4(1, 1, 1, 1)
        mtl.roughnessFactor = roughness
        mtl.metallicFactor  = metallic
        mtl.gpuInit(ctx)
        self.material_keep_alive.append(mtl)

        node = prim.createNode(f"sphere_{ix}_{iz}", self.layer1, mtl)
        node.worldTransform.translation = vec3(origin + ix * spacing,
                                               1.0,
                                               origin + iz * spacing)
        node.sortkey = 10
        self.prim_keep_alive.append(prim)
        self.nodes.append(node)

    # Simple key light so dielectrics don't go fully black if envmap is dim.
    self.point_light = lev2.DynamicPointLight()
    self.point_light.data.color     = vec3(1, 0.95, 0.85)
    self.point_light.data.intensity = 4.0
    self.point_light.data.radius    = 30.0
    self.light_node = self.layer1.createLightNode("key", self.point_light)
    self.light_node.setMatrix(mtx4.transMatrix(vec3(3, 5, 5)))

    self.scene.lightingmanager.gpuInit(ctx)

    self.pbr_common = self.scene.pbr_common
    self.pbr_common.skyboxLevel = 1.0

    # One radiance maps object, mutated in place by all modes.
    self.proc_radiance = lev2.PbrCommon.makeProceduralRadianceMaps(ctx)
    lev2.PbrCommon.updateRadianceMapsGradient(self.proc_radiance, STATIC_GRADIENT, ctx)
    self.pbr_common.RadianceMaps = self.proc_radiance

    print("Procedural envmaps test (9×9 grid: X=metallic, Z=roughness):")
    print("  1: solid color (cycles through palette on each press)")
    print("  2: static gradient (sky / horizon / ground)")
    print("  3: time-varying gradient (day-to-night cycle)")

  ##############################################

  def _stops_for_time(self, t):
    """Day-to-night cycle gradient stops for mode 3."""
    cycle = (math.sin(t * 0.4) + 1.0) * 0.5  # 0 = night, 1 = day
    sky_top    = vec3(0.02 + 0.20 * cycle, 0.03 + 0.28 * cycle, 0.06 + 0.45 * cycle)
    sky_horiz  = vec3(0.05 + 0.40 * cycle, 0.08 + 0.45 * cycle, 0.10 + 0.55 * cycle)
    horizon_lo = vec3(0.05 + 0.30 * cycle, 0.05 + 0.30 * cycle, 0.05 + 0.30 * cycle)
    nadir      = vec3(0.02 + 0.05 * cycle, 0.02 + 0.05 * cycle, 0.02 + 0.05 * cycle)
    return [
      (0.00, sky_top),
      (0.48, sky_horiz),
      (0.52, horizon_lo),
      (1.00, nadir),
    ]

  ##############################################

  def onGpuUpdate(self, ctx):
    if self.mode == 3:
      stops = self._stops_for_time(self.time)
      lev2.PbrCommon.updateRadianceMapsGradient(self.proc_radiance, stops, ctx)
      self.last_radiance_key = ("gradient_time",)
      return
    key = ("solid", self.solid_idx) if self.mode == 1 else ("gradient_static",)
    if key == self.last_radiance_key:
      return
    self.last_radiance_key = key
    if self.mode == 1:
      color = SOLID_PALETTE[self.solid_idx % len(SOLID_PALETTE)]
      lev2.PbrCommon.updateRadianceMapsGradient(self.proc_radiance, [(0.0, color)], ctx)
    else:
      lev2.PbrCommon.updateRadianceMapsGradient(self.proc_radiance, STATIC_GRADIENT, ctx)

  ##############################################

  def onUiEvent(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed:
      kc = uievent.keycode
      if kc == ord("1"):
        if self.mode == 1:
          self.solid_idx = (self.solid_idx + 1) % len(SOLID_PALETTE)
        else:
          self.mode = 1
        return lev2.ui.HandlerResult()
      if kc == ord("2"):
        self.mode = 2
        return lev2.ui.HandlerResult()
      if kc == ord("3"):
        self.mode = 3
        return lev2.ui.HandlerResult()
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()

  def onUpdate(self, updinfo):
    self.time = updinfo.absolutetime
    self.scene.updateScene(self.cameralut)

################################################################################

if __name__ == "__main__":
  app = ProceduralEnvmapsApp()
  app.ezapp.mainThreadLoop()
