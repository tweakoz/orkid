#!/usr/bin/env ork.python

################################################################################
# Procedural PBR Materials Test
# Demonstrates RigidPrimitive.fromArrays with PBR vertex-color technique,
# per-object roughness/metallic via PBRMaterial, and ModColor tint/opacity.
# Renders a row of spheres with different PBR material properties.
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, signal
import numpy as np
from orkengine.core import vec3, vec4, quat, mtx4, CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine.lev2 import RigidPrimitive

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

################################################################################
# Generate sphere mesh data as numpy arrays
################################################################################

def make_sphere_arrays(radius=1.0, n=16):
  """Generate sphere vertex data as numpy arrays for fromArrays."""
  verts = []
  norms = []
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
  indices = []
  for i in range(n):
    for j in range(w):
      a = i * w + j
      b = a + 1 if j < w - 1 else i * w
      c = a + w
      d = c + 1 if j < w - 1 else (i + 1) * w
      # CW winding for orkid
      indices.extend([a, c, b, b, c, d])

  nv = len(verts)
  verts_np = np.array(verts, dtype=np.float32)
  norms_np = np.array(norms, dtype=np.float32)
  # Compute binormals from normals
  up = np.array([0, 1, 0], dtype=np.float32)
  binormals_np = np.cross(norms_np, up)
  degen = np.linalg.norm(binormals_np, axis=1) < 1e-6
  binormals_np[degen] = np.cross(norms_np[degen], [1, 0, 0])
  lens = np.linalg.norm(binormals_np, axis=1, keepdims=True)
  binormals_np = binormals_np / np.where(lens < 1e-10, 1.0, lens)
  uvs_np = np.zeros((nv, 2), dtype=np.float32)
  indices_np = np.array(indices, dtype=np.uint32)
  return verts_np, norms_np, binormals_np, uvs_np, indices_np

################################################################################
# Material definitions: (name, color_rgb, roughness, metallic, alpha_blend)
################################################################################

MATERIALS = [
  ("gold",     (0.8, 0.7, 0.2),  0.3, 1.0, False),
  ("chrome",   (0.9, 0.9, 0.9),  0.0, 1.0, False),
  ("chalk",    (0.8, 0.75, 0.7), 1.0, 0.0, False),
  ("obsidian", (0.05, 0.05, 0.06), 0.1, 0.0, False),
  ("copper",   (0.7, 0.4, 0.2),  0.2, 1.0, False),
  ("plastic",  (0.6, 0.1, 0.1),  0.15, 0.8, False),
  ("glass",    (0.9, 0.95, 1.0), 0.0, 0.0, True),
  ("bubble",   (0.9, 0.95, 1.0), 0.0, 1.0, True),
]

################################################################################

class ProceduralMaterialsApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self, width=1280, height=720)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 3, 12), tgt=vec3(0, 1, 0))
    signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())
    self.time = 0.0

  ##############################################

  def onGpuInit(self, ctx):
    createSceneGraph(app=self, params_dict={
      "SkyboxTexPathStr": "ork_envmaps|cold4k",
      "SkyboxIntensity": float(1),
    })

    # Grid
    self.grid_data = createGridData()
    self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1

    # White textures for PBR (vertex colors provide albedo)
    self.white_img = lev2.Image.createFromFile("src://effect_textures/white.dds")
    self.normal_img = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    # Sphere mesh data (shared across all materials)
    verts_np, norms_np, binormals_np, uvs_np, indices_np = make_sphere_arrays(radius=1.0, n=16)

    self.material_keep_alive = []
    self.nodes = []

    spacing = 2.5
    start_x = -(len(MATERIALS) - 1) * spacing / 2.0

    for i, (name, color, roughness, metallic, alpha_blend) in enumerate(MATERIALS):
      # Create vertex colors
      nv = len(verts_np)
      r, g, b = color
      a = 0.3 if alpha_blend else 1.0
      colors_np = np.zeros((nv, 4), dtype=np.uint8)
      colors_np[:, 0] = int(r * 255)
      colors_np[:, 1] = int(g * 255)
      colors_np[:, 2] = int(b * 255)
      colors_np[:, 3] = int(a * 255)

      # Create RigidPrimitive via fromArrays
      prim = RigidPrimitive()
      prim.fromArrays(verts_np, norms_np, binormals_np, uvs_np, colors_np, indices_np, ctx)

      # Create PBR material
      mtl = lev2.PBRMaterial()
      mtl.assignImages(ctx, color=self.white_img, normal=self.normal_img,
                       mtlruf=self.white_img, doConform=True)
      mtl.baseColor = vec4(1, 1, 1, 1)
      mtl.roughnessFactor = roughness
      mtl.metallicFactor = metallic
      if alpha_blend:
        mtl.alphaBlend = True
      mtl.gpuInit(ctx)
      self.material_keep_alive.append(mtl)

      # Create node
      node = prim.createNode(f"sphere_{name}", self.layer1, mtl)
      node.worldTransform.translation = vec3(start_x + i * spacing, 1.2, 0)
      node.sortkey = 20 if alpha_blend else 10
      self.nodes.append((node, prim, name))

    # Point light
    self.point_light = lev2.DynamicPointLight()
    self.point_light.data.color = vec3(1, 0.95, 0.85)
    self.point_light.data.intensity = 5.0
    self.point_light.data.radius = 30.0
    self.light_node = self.layer1.createLightNode("key_light", self.point_light)
    self.light_node.setMatrix(mtx4.transMatrix(vec3(3, 5, 5)))

    self.scene.lightingmanager.gpuInit(ctx)

    print("Procedural PBR Materials Test:")
    for i, (name, color, roughness, metallic, alpha_blend) in enumerate(MATERIALS):
      print(f"  {name}: rough={roughness} metal={metallic} alpha={alpha_blend}")

  ##############################################

  def onGpuUpdate(self, ctx):
    pass

  def onUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()

  def onUpdate(self, updinfo):
    self.time = updinfo.absolutetime
    self.scene.updateScene(self.cameralut)

################################################################################

if __name__ == "__main__":
  app = ProceduralMaterialsApp()
  app.ezapp.mainThreadLoop()
