#!/usr/bin/env ork.python

################################################################################
# Procedural Geometry + Point Lights Test
# Demonstrates DynamicPointLight with procedural PBR vertex-color geometry.
# Multiple colored point lights orbit around a set of procedural objects
# with varying PBR materials (matte, metallic, mirror).
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

def make_sphere_arrays(radius=1.0, n=12):
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
  indices = []
  for i in range(n):
    for j in range(w):
      a = i * w + j
      b = a + 1 if j < w - 1 else i * w
      c = a + w
      d = c + 1 if j < w - 1 else (i + 1) * w
      indices.extend([a, c, b, b, c, d])
  nv = len(verts)
  verts_np = np.array(verts, dtype=np.float32)
  norms_np = np.array(norms, dtype=np.float32)
  up = np.array([0, 1, 0], dtype=np.float32)
  binormals_np = np.cross(norms_np, up)
  degen = np.linalg.norm(binormals_np, axis=1) < 1e-6
  binormals_np[degen] = np.cross(norms_np[degen], [1, 0, 0])
  lens = np.linalg.norm(binormals_np, axis=1, keepdims=True)
  binormals_np = binormals_np / np.where(lens < 1e-10, 1.0, lens)
  uvs_np = np.zeros((nv, 2), dtype=np.float32)
  return verts_np, norms_np, binormals_np, uvs_np, np.array(indices, dtype=np.uint32)

################################################################################

class PointLightsApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self, width=1280, height=720)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 4, 10), tgt=vec3(0, 1, 0))
    signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())
    self.time = 0.0

  ##############################################

  def onGpuInit(self, ctx):
    createSceneGraph(app=self, params_dict={
      "SkyboxTexPathStr": "ork_envmaps|cold4k",
      "SkyboxIntensity": float(1),
    })

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1

    white_img = lev2.Image.createFromFile("src://effect_textures/white.dds")
    normal_img = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    sv, sn, sb, su, si = make_sphere_arrays(1.0, 12)
    self.materials = []
    self.prims = []

    # Three spheres: matte, metallic, mirror
    configs = [
      ("matte",    (-2.5, 1.2, 0), (0.2, 0.15, 0.12), 0.9, 0.0),
      ("metallic", (0,    1.2, 0), (0.7, 0.5, 0.3),   0.3, 1.0),
      ("mirror",   (2.5,  1.2, 0), (0.9, 0.9, 0.9),   0.0, 1.0),
    ]

    for name, pos, color, roughness, metallic in configs:
      nv = len(sv)
      r, g, b = color
      colors_np = np.zeros((nv, 4), dtype=np.uint8)
      colors_np[:, 0] = int(r * 255)
      colors_np[:, 1] = int(g * 255)
      colors_np[:, 2] = int(b * 255)
      colors_np[:, 3] = 255

      prim = RigidPrimitive()
      prim.fromArrays(sv, sn, sb, su, colors_np, si, ctx)

      mtl = lev2.PBRMaterial()
      mtl.assignImages(ctx, color=white_img, normal=normal_img, mtlruf=white_img, doConform=True)
      mtl.baseColor = vec4(1, 1, 1, 1)
      mtl.roughnessFactor = roughness
      mtl.metallicFactor = metallic
      mtl.gpuInit(ctx)
      self.materials.append(mtl)
      self.prims.append(prim)

      node = prim.createNode(name, self.layer1, mtl)
      node.worldTransform.translation = vec3(*pos)
      node.sortkey = 10

    # Three orbiting point lights with indicator spheres
    orb_sv, orb_sn, orb_sb, orb_su, orb_si = make_sphere_arrays(0.12, 6)
    light_configs = [
      ("red",   vec3(1.0, 0.2, 0.1)),
      ("green", vec3(0.1, 1.0, 0.2)),
      ("blue",  vec3(0.2, 0.3, 1.0)),
    ]
    self.lights = []
    for name, color in light_configs:
      light = lev2.DynamicPointLight()
      light.data.color = color
      light.data.intensity = 8.0
      light.data.radius = 15.0
      light_node = self.layer1.createLightNode(f"light_{name}", light)

      # Indicator sphere matching light color
      nv = len(orb_sv)
      orb_colors = np.zeros((nv, 4), dtype=np.uint8)
      orb_colors[:, 0] = int(min(color.x, 1.0) * 255)
      orb_colors[:, 1] = int(min(color.y, 1.0) * 255)
      orb_colors[:, 2] = int(min(color.z, 1.0) * 255)
      orb_colors[:, 3] = 255
      orb_prim = RigidPrimitive()
      orb_prim.fromArrays(orb_sv, orb_sn, orb_sb, orb_su, orb_colors, orb_si, ctx)
      orb_mtl = lev2.PBRMaterial()
      orb_mtl.assignImages(ctx, color=white_img, normal=normal_img, mtlruf=white_img, doConform=True)
      orb_mtl.baseColor = vec4(1, 1, 1, 1)
      orb_mtl.roughnessFactor = 0.0
      orb_mtl.metallicFactor = 0.0
      orb_mtl.gpuInit(ctx)
      self.materials.append(orb_mtl)
      self.prims.append(orb_prim)
      orb_node = orb_prim.createNode(f"orb_{name}", self.layer1, orb_mtl)

      self.lights.append((light, light_node, orb_node))

    self.scene.lightingmanager.gpuInit(ctx)

    print("Point Lights Test:")
    print("  Left: matte sphere (rough=0.9, metal=0)")
    print("  Center: metallic sphere (rough=0.3, metal=1)")
    print("  Right: mirror sphere (rough=0, metal=1)")
    print("  Three colored point lights orbit in an ellipse")

  ##############################################

  def onGpuUpdate(self, ctx):
    t = self.time
    rx, rz = 5.0, 3.0  # ellipse radii
    for i, (light, light_node, orb_node) in enumerate(self.lights):
      angle = t * 0.8 + i * math.tau / 3
      x = rx * math.cos(angle)
      z = rz * math.sin(angle)
      y = 2.0 + math.sin(t * 1.5 + i) * 0.8
      light_node.setMatrix(mtx4.transMatrix(vec3(x, y, z)))
      orb_node.worldTransform.translation = vec3(x, y, z)

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
  app = PointLightsApp()
  app.ezapp.mainThreadLoop()
