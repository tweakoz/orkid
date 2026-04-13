#!/usr/bin/env ork.python

################################################################################
# GPU Instanced Procedural Geometry Test
# Demonstrates InstancedRigidPrimitiveDrawable:
# - Instanced rendering of procedural meshes via SSBO
# - Per-instance transforms (position, rotation, scale) via world matrices
# - Per-instance colors multiplied with per-vertex colors
# - Opaque and transparent instanced objects
# - Time-varying per-instance animation
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, signal, random
import numpy as np
from orkengine.core import vec3, vec4, quat, mtx4, CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine.lev2 import RigidPrimitive

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

NUM_OPAQUE = 500
NUM_TRANSPARENT = 200

################################################################################

def make_sphere_arrays(radius=1.0, n=8):
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

class InstancingApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self, width=1280, height=720)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 8, 20), tgt=vec3(0, 2, 0))
    signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())
    self.time = 0.0

  ##############################################

  def onGpuInit(self, ctx):
    createSceneGraph(app=self)

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1

    white_img = lev2.Image.createFromBuffer(4, 4, "RGBA8",
      np.full((4, 4, 4), 255, dtype=np.uint8))
    normal_img = lev2.Image.createFromBuffer(4, 4, "RGBA8",
      np.tile(np.array([128, 128, 255, 255], dtype=np.uint8), (4, 4, 1)))

    sv, sn, sb, su, si = make_sphere_arrays(1.0, 8)
    nv = len(sv)

    ##################################
    # Opaque instanced spheres
    ##################################
    colors_opaque = np.zeros((nv, 4), dtype=np.uint8)
    colors_opaque[:, 0] = 200
    colors_opaque[:, 1] = 200
    colors_opaque[:, 2] = 200
    colors_opaque[:, 3] = 255

    prim_opaque = RigidPrimitive()
    prim_opaque.fromArrays(sv, sn, sb, su, colors_opaque, si, ctx)

    mtl_opaque = lev2.PBRMaterial()
    mtl_opaque.assignImages(ctx, color=white_img, normal=normal_img, mtlruf=white_img, doConform=True)
    mtl_opaque.baseColor = vec4(1, 1, 1, 1)
    mtl_opaque.roughnessFactor = 0.5
    mtl_opaque.metallicFactor = 0.0
    mtl_opaque.gpuInit(ctx)

    self.opaque_node = prim_opaque.createInstancedNode(NUM_OPAQUE, "opaque_spheres", self.layer1, mtl_opaque)
    self.opaque_node.sortkey = 10
    self.opaque_instdata = self.opaque_node.instanceData

    # Initialize opaque instance data
    self.opaque_data = []
    matrices = np.array(self.opaque_instdata.matrices, copy=False)
    colors = np.array(self.opaque_instdata.colors, copy=False)
    for i in range(NUM_OPAQUE):
      x = random.uniform(-10, 10)
      z = random.uniform(-10, 10)
      y = random.uniform(0.5, 5)
      s = random.uniform(0.1, 0.4)
      phase = random.uniform(0, math.tau)
      speed = random.uniform(0.3, 1.0)
      # Random warm color
      cr = random.uniform(0.1, 0.25)
      cg = random.uniform(0.05, 0.15)
      cb = random.uniform(0.03, 0.1)
      self.opaque_data.append((x, y, z, s, phase, speed, cr, cg, cb))
      m = mtx4.composed(vec3(x, y, z), quat(), s)
      matrices[i] = np.array(m, copy=False)
      colors[i] = (cr, cg, cb, 1.0)

    ##################################
    # Transparent instanced spheres
    ##################################
    colors_trans = np.zeros((nv, 4), dtype=np.uint8)
    colors_trans[:, 0] = 230
    colors_trans[:, 1] = 240
    colors_trans[:, 2] = 255
    colors_trans[:, 3] = 255  # vertex alpha = 1, instance alpha controls opacity

    prim_trans = RigidPrimitive()
    prim_trans.fromArrays(sv, sn, sb, su, colors_trans, si, ctx)

    mtl_trans = lev2.PBRMaterial()
    mtl_trans.assignImages(ctx, color=white_img, normal=normal_img, mtlruf=white_img, doConform=True)
    mtl_trans.baseColor = vec4(1, 1, 1, 1)
    mtl_trans.roughnessFactor = 0.0
    mtl_trans.metallicFactor = 1.0
    mtl_trans.alphaBlend = True
    mtl_trans.gpuInit(ctx)

    self.trans_node = prim_trans.createInstancedNode(NUM_TRANSPARENT, "trans_spheres", self.layer1, mtl_trans)
    self.trans_node.sortkey = 20
    self.trans_instdata = self.trans_node.instanceData

    # Initialize transparent instance data
    self.trans_data = []
    matrices = np.array(self.trans_instdata.matrices, copy=False)
    colors = np.array(self.trans_instdata.colors, copy=False)
    for i in range(NUM_TRANSPARENT):
      x = random.uniform(-8, 8)
      z = random.uniform(-8, 8)
      y = random.uniform(1, 6)
      s = random.uniform(0.2, 0.6)
      phase = random.uniform(0, math.tau)
      speed = random.uniform(0.5, 1.5)
      birth = random.uniform(-5, 0)
      lifetime = random.uniform(3, 8)
      self.trans_data.append((x, y, z, s, phase, speed, birth, lifetime))
      m = mtx4.composed(vec3(x, y, z), quat(), s)
      matrices[i] = np.array(m, copy=False)
      colors[i] = (0.9, 0.95, 1.0, 0.4)

    # Keep materials alive
    self.materials = [mtl_opaque, mtl_trans]
    self.prims = [prim_opaque, prim_trans]

    # Lighting
    light = lev2.DynamicPointLight()
    light.data.color = vec3(1, 0.95, 0.85)
    light.data.intensity = 8.0
    light.data.radius = 30.0
    self.light_node = light.data.createNode("sun", self.layer1)
    self.light_node.worldTransform.translation = vec3(5, 10, 5)
    self.light = light

    self.scene.lightingmanager.gpuInit(ctx)

    print(f"Instancing Test: {NUM_OPAQUE} opaque + {NUM_TRANSPARENT} transparent spheres")

  ##############################################

  def onGpuUpdate(self, ctx):
    t = self.time

    # Animate opaque instances: gentle bobbing
    matrices = np.array(self.opaque_instdata.matrices, copy=False)
    for i in range(NUM_OPAQUE):
      x, y_base, z, s, phase, speed, cr, cg, cb = self.opaque_data[i]
      y = y_base + math.sin(t * speed + phase) * 0.3
      m = mtx4.composed(vec3(x, y, z), quat(vec3(0, 1, 0), t * speed + phase), s)
      matrices[i] = np.array(m, copy=False)

    # Animate transparent instances: fade in/out lifecycle
    matrices = np.array(self.trans_instdata.matrices, copy=False)
    colors = np.array(self.trans_instdata.colors, copy=False)
    for i in range(NUM_TRANSPARENT):
      x, y_base, z, s, phase, speed, birth, lifetime = self.trans_data[i]
      age = t - birth
      # Wrap around
      cycle_age = age % lifetime
      fade = 1.0 - (cycle_age / lifetime) ** 1.5
      alpha = max(0.0, 0.5 * fade)
      y = y_base + math.sin(t * speed + phase) * 0.5
      m = mtx4.composed(vec3(x, y, z), quat(), s * (0.5 + 0.5 * fade))
      matrices[i] = np.array(m, copy=False)
      colors[i] = (0.9, 0.95, 1.0, alpha)

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
  app = InstancingApp()
  app.ezapp.mainThreadLoop()
