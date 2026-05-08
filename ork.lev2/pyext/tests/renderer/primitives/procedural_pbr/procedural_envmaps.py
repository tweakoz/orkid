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
# A row of 9 spheres sweeps roughness 0→1 at metallic=1, plus an extra row at
# metallic=0, so reflections and diffuse irradiance are both visible against
# whichever envmap is active.
#
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, signal
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
    setupUiCamera(app=self, eye=vec3(0, 4, 14), tgt=vec3(0, 1, 0))
    signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())
    self.time = 0.0
    self.mode = 2                # default: static gradient
    self.solid_idx = 0
    self.last_radiance_key = None
    self.pending_radiance = None  # 1-frame defer to avoid pink-flash from
                                  # not-yet-uploaded textures (Vulkan one-shot
                                  # transfers drain at the next frame begin)

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

    # Two rows: top row metallic=1 (mirror→matte sweep), bottom row metallic=0
    # (dielectric, lit primarily by the diffuse irradiance map).
    cols     = 9
    spacing  = 1.6
    start_x  = -(cols - 1) * spacing / 2.0
    rows = [
      ("metal",    1.0, 1.4),  # y = 1.4
      ("dielect",  0.0, 0.0),  # y = 0.0
    ]
    for row_name, metallic, y_off in rows:
      for i in range(cols):
        roughness = i / (cols - 1)
        # Bright neutral-tan vertex color so both reflection and diffuse read
        # well against any envmap; (B, G, R, A) ordering for the MicroMesh
        # ARGBU32 packing convention used elsewhere in these tests.
        rgb = (0.85, 0.78, 0.7)
        colors_np = np.tile(np.array([rgb[2], rgb[1], rgb[0], 1.0],
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
        mtl.baseColor      = vec4(1, 1, 1, 1)
        mtl.roughnessFactor = roughness
        mtl.metallicFactor  = metallic
        mtl.gpuInit(ctx)
        self.material_keep_alive.append(mtl)

        node = prim.createNode(f"sphere_{row_name}_{i}", self.layer1, mtl)
        node.worldTransform.translation = vec3(start_x + i * spacing, 1.0 + y_off, 0)
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

    print("Procedural envmaps test:")
    print("  1: solid color (cycles through palette on each press)")
    print("  2: static gradient (sky / horizon / ground)")
    print("  3: time-varying gradient (day-to-night cycle)")

  ##############################################

  def _build_radiance_for_mode(self, ctx):
    """Return (radiance_maps, key) for the current mode and time."""
    if self.mode == 1:
      color = SOLID_PALETTE[self.solid_idx % len(SOLID_PALETTE)]
      key   = ("solid", self.solid_idx)
      return lev2.PbrCommon.makeRadianceMapsSolidColor(color, ctx), key

    if self.mode == 2:
      key = ("gradient_static",)
      return lev2.PbrCommon.makeRadianceMapsGradient(STATIC_GRADIENT, ctx), key

    # mode 3: continuous day-to-night cycle. Quantize the key to the current
    # time bucket so we rebuild every frame.
    t = self.time
    cycle = (math.sin(t * 0.4) + 1.0) * 0.5  # 0 = night, 1 = day
    sky_top    = vec3(0.02 + 0.20 * cycle, 0.03 + 0.28 * cycle, 0.06 + 0.45 * cycle)
    sky_horiz  = vec3(0.05 + 0.40 * cycle, 0.08 + 0.45 * cycle, 0.10 + 0.55 * cycle)
    horizon_lo = vec3(0.05 + 0.30 * cycle, 0.05 + 0.30 * cycle, 0.05 + 0.30 * cycle)
    nadir      = vec3(0.02 + 0.05 * cycle, 0.02 + 0.05 * cycle, 0.02 + 0.05 * cycle)
    stops = [
      (0.00, sky_top),
      (0.48, sky_horiz),
      (0.52, horizon_lo),
      (1.00, nadir),
    ]
    return lev2.PbrCommon.makeRadianceMapsGradient(stops, ctx), ("gradient_time", round(t * 60))

  ##############################################

  def onGpuUpdate(self, ctx):
    # Swap in last frame's pending maps now that their upload one-shot has
    # been drained at the start of this frame.
    if self.pending_radiance is not None:
      self.pbr_common.RadianceMaps = self.pending_radiance
      self.pending_radiance = None

    rmaps, key = self._build_radiance_for_mode(ctx)
    if key != self.last_radiance_key:
      self.last_radiance_key = key
      self.pending_radiance  = rmaps

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
