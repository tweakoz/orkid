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
from orkengine.lev2 import RigidPrimitive, MicroMesh

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

NUM_SPHERES = 300
NUM_CUBES = 200
NUM_TRANSPARENT = 200

################################################################################

def make_sphere_arrays(radius=1.0, n=8):
  # Faces are CCW-from-outside; MicroMesh handles winding internally.
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

def make_cube_arrays(size=1.0):
  h = size / 2.0
  face_data = [
    ((0,0,1),  [(-h,-h,h),(h,-h,h),(h,h,h),(-h,h,h)]),
    ((0,0,-1), [(h,-h,-h),(-h,-h,-h),(-h,h,-h),(h,h,-h)]),
    ((1,0,0),  [(h,-h,h),(h,-h,-h),(h,h,-h),(h,h,h)]),
    ((-1,0,0), [(-h,-h,-h),(-h,-h,h),(-h,h,h),(-h,h,-h)]),
    ((0,1,0),  [(-h,h,h),(h,h,h),(h,h,-h),(-h,h,-h)]),
    ((0,-1,0), [(-h,-h,-h),(h,-h,-h),(h,-h,h),(-h,-h,h)]),
  ]
  verts, norms, faces = [], [], []
  for (nx, ny, nz), corners in face_data:
    base = len(verts)
    for c in corners:
      verts.append(c)
      norms.append((nx, ny, nz))
    faces.extend([3, base, base+1, base+2, 3, base, base+2, base+3])
  nv = len(verts)
  verts_np = np.array(verts, dtype=np.float32)
  norms_np = np.array(norms, dtype=np.float32)
  binormals_np = np.zeros((nv, 3), dtype=np.float32)
  binormals_np[:, 0] = 1.0
  return verts_np, norms_np, binormals_np, faces

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
    createSceneGraph(app=self, params_dict={
      "SkyboxTexPathStr": "ork_envmaps|cold4k",
      "SkyboxIntensity": float(1),
    })

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1

    white_img = lev2.Image.createFromFile("src://effect_textures/white.dds")
    normal_img = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    self.materials = []
    self.prims = []

    def make_instanced_set(mesh_arrays, count, name, roughness, metallic, alpha_blend=False):
      v, n, b, faces = mesh_arrays
      nv = len(v)
      # White vertex colors — per-instance colors come from the SSBO modcolors.
      vc = np.ones((nv, 4), dtype=np.float32)
      mesh = MicroMesh.fromVertAndFaceLists(v, faces)
      mesh.updateNormals(n)
      mesh.updateBinormals(b)
      mesh.updateColors(vc)
      prim = RigidPrimitive()
      prim.updateWithMicroMesh(mesh, ctx, tokens.TRIANGLES)
      mtl = lev2.PBRMaterial()
      mtl.assignImages(ctx, color=white_img, normal=normal_img, mtlruf=white_img, doConform=True)
      mtl.baseColor = vec4(1, 1, 1, 1)
      mtl.roughnessFactor = roughness
      mtl.metallicFactor = metallic
      if alpha_blend:
        mtl.alphaBlend = True
      mtl.gpuInit(ctx)
      self.materials.append(mtl)
      self.prims.append(prim)
      node = prim.createInstancedNode(count, name, self.layer1, mtl)
      node.sortkey = 20 if alpha_blend else 10
      return node

    sphere_arrays = make_sphere_arrays(1.0, 8)
    cube_arrays = make_cube_arrays(1.0)

    ##################################
    # Opaque instanced spheres
    ##################################
    self.sphere_node = make_instanced_set(sphere_arrays, NUM_SPHERES, "spheres", 0.4, 0.8)
    self.sphere_instdata = self.sphere_node.instanceData

    self.sphere_data = []
    matrices = np.array(self.sphere_instdata.matrices, copy=False)
    colors = np.array(self.sphere_instdata.colors, copy=False)
    for i in range(NUM_SPHERES):
      x = random.uniform(-10, 10)
      z = random.uniform(-10, 10)
      y = random.uniform(0.5, 5)
      s = random.uniform(0.2, 0.5)
      phase = random.uniform(0, math.tau)
      speed = random.uniform(0.3, 1.0)
      cr = random.uniform(0.5, 1.0)
      cg = random.uniform(0.3, 0.8)
      cb = random.uniform(0.2, 0.6)
      self.sphere_data.append((x, y, z, s, phase, speed, cr, cg, cb))
      m = mtx4.composed(vec3(x, y, z), quat(), s)
      matrices[i] = np.array(m, copy=False)
      colors[i] = (cr, cg, cb, 1.0)

    ##################################
    # Opaque instanced cubes
    ##################################
    self.cube_node = make_instanced_set(cube_arrays, NUM_CUBES, "cubes", 0.6, 0.5)
    self.cube_instdata = self.cube_node.instanceData

    self.cube_data = []
    matrices = np.array(self.cube_instdata.matrices, copy=False)
    colors = np.array(self.cube_instdata.colors, copy=False)
    for i in range(NUM_CUBES):
      x = random.uniform(-10, 10)
      z = random.uniform(-10, 10)
      y = random.uniform(0.5, 4)
      s = random.uniform(0.15, 0.4)
      phase = random.uniform(0, math.tau)
      speed = random.uniform(0.2, 0.7)
      rx_speed = random.uniform(0.3, 1.2)
      rz_speed = random.uniform(0.2, 0.8)
      cr = random.uniform(0.3, 0.7)
      cg = random.uniform(0.3, 0.7)
      cb = random.uniform(0.5, 1.0)
      self.cube_data.append((x, y, z, s, phase, speed, rx_speed, rz_speed, cr, cg, cb))
      m = mtx4.composed(vec3(x, y, z), quat(), s)
      matrices[i] = np.array(m, copy=False)
      colors[i] = (cr, cg, cb, 1.0)

    ##################################
    # Transparent instanced spheres
    ##################################
    self.trans_node = make_instanced_set(sphere_arrays, NUM_TRANSPARENT, "trans_spheres", 0.0, 1.0, alpha_blend=True)
    self.trans_instdata = self.trans_node.instanceData

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

    # Lighting — two point lights
    self.dyn_lights = []
    for lname, lpos, lcolor, lintens in [
      ("key",  vec3(5, 12, 5),   vec3(1, 0.95, 0.85), 20.0),
      ("fill", vec3(-5, 10, -3), vec3(0.5, 0.6, 0.9), 12.0),
    ]:
      light = lev2.DynamicPointLight()
      light.data.color = lcolor
      light.data.intensity = lintens
      light.data.radius = 30.0
      light_node = self.layer1.createLightNode(lname, light)
      light_node.setMatrix(mtx4.transMatrix(lpos))
      self.dyn_lights.append(light)

    self.scene.lightingmanager.gpuInit(ctx)

    print(f"Instancing Test: {NUM_SPHERES} spheres + {NUM_CUBES} cubes + {NUM_TRANSPARENT} transparent bubbles")

  ##############################################

  def onGpuUpdate(self, ctx):
    t = self.time

    # Animate spheres: bobbing, occasional visibility toggle
    matrices = np.array(self.sphere_instdata.matrices, copy=False)
    colors = np.array(self.sphere_instdata.colors, copy=False)
    for i in range(NUM_SPHERES):
      x, y_base, z, s, phase, speed, cr, cg, cb = self.sphere_data[i]
      # Rare visibility toggle (~5% hidden at any time)
      visible = math.sin(t * 0.2 + phase * 3.7) > -0.9
      if not visible:
        matrices[i] = np.zeros((4, 4), dtype=np.float32)
        colors[i] = (0, 0, 0, 0)
        continue
      y = y_base + math.sin(t * speed + phase) * 0.3
      m = mtx4.composed(vec3(x, y, z), quat(vec3(0, 1, 0), t * speed + phase), s)
      matrices[i] = np.array(m, copy=False)
      colors[i] = (cr, cg, cb, 1.0)

    # Animate cubes: tumbling rotation, scale pulsing
    matrices = np.array(self.cube_instdata.matrices, copy=False)
    colors = np.array(self.cube_instdata.colors, copy=False)
    for i in range(NUM_CUBES):
      x, y_base, z, s, phase, speed, rx_spd, rz_spd, cr, cg, cb = self.cube_data[i]
      y = y_base + math.sin(t * speed + phase) * 0.2
      pulse = s * (0.8 + 0.2 * math.sin(t * 1.5 + phase))
      rot = quat(vec3(1, 0, 0), t * rx_spd + phase) * quat(vec3(0, 1, 0), t * speed) * quat(vec3(0, 0, 1), t * rz_spd)
      m = mtx4.composed(vec3(x, y, z), rot, pulse)
      matrices[i] = np.array(m, copy=False)
      colors[i] = (cr, cg, cb, 1.0)

    # Animate transparent spheres: fade in/out lifecycle
    matrices = np.array(self.trans_instdata.matrices, copy=False)
    colors = np.array(self.trans_instdata.colors, copy=False)
    for i in range(NUM_TRANSPARENT):
      x, y_base, z, s, phase, speed, birth, lifetime = self.trans_data[i]
      age = t - birth
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
