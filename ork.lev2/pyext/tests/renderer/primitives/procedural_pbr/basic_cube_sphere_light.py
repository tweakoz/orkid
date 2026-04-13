#!/usr/bin/env ork.python

################################################################################
# Basic Procedural Cube + Orbiting Point Light Test
# Minimal test: one cube, one orbiting point light with indicator sphere.
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, signal, argparse
import numpy as np
from orkengine.core import vec3, vec4, quat, mtx4, CrcStringProxy, lev2_pyexdir
from orkengine import lev2
from orkengine.lev2 import RigidPrimitive

lev2_pyexdir.addToSysPath()

parser = argparse.ArgumentParser(description='basic cube+sphere+light test')
parser.add_argument('--gold', action='store_true', help='use gold material instead of chalk')
args = parser.parse_args()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

################################################################################

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
  verts, norms, indices = [], [], []
  for (nx, ny, nz), corners in face_data:
    base = len(verts)
    for c in corners:
      verts.append(c)
      norms.append((nx, ny, nz))
    indices.extend([base, base+2, base+1, base, base+3, base+2])
  nv = len(verts)
  verts_np = np.array(verts, dtype=np.float32)
  norms_np = np.array(norms, dtype=np.float32)
  binormals_np = np.zeros((nv, 3), dtype=np.float32)
  binormals_np[:, 0] = 1.0
  uvs_np = np.zeros((nv, 2), dtype=np.float32)
  indices_np = np.array(indices, dtype=np.uint32)
  return verts_np, norms_np, binormals_np, uvs_np, indices_np

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

class BasicCubeLightApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self, width=1280, height=720)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(3, 2.5, 3), tgt=vec3(0, 0.5, 0))
    signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())
    self.time = 0.0

  def onGpuInit(self, ctx):
    createSceneGraph(app=self, params_dict={
      "SkyboxTexPathStr": "ork_envmaps|tozenv_basic",
      "SkyboxIntensity": float(1),
    })

    white_img = lev2.Image.createFromFile("src://effect_textures/white.dds")
    normal_img = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    # Material properties
    if args.gold:
      obj_color = (0.8, 0.7, 0.2)
      roughness, metallic = 0.3, 1.0
    else:
      obj_color = (0.8, 0.75, 0.7)
      roughness, metallic = 1.0, 0.0

    # Cube
    cv, cn, cb, cu, ci = make_cube_arrays(size=1.0)
    nv = len(cv)
    colors_np = np.zeros((nv, 4), dtype=np.uint8)
    colors_np[:, 0] = int(obj_color[0] * 255)
    colors_np[:, 1] = int(obj_color[1] * 255)
    colors_np[:, 2] = int(obj_color[2] * 255)
    colors_np[:, 3] = 255
    self.cube_prim = RigidPrimitive()
    self.cube_prim.fromArrays(cv, cn, cb, cu, colors_np, ci, ctx)
    cube_mtl = lev2.PBRMaterial()
    cube_mtl.assignImages(ctx, color=white_img, normal=normal_img, mtlruf=white_img, doConform=True)
    cube_mtl.baseColor = vec4(1, 1, 1, 1)
    cube_mtl.roughnessFactor = roughness
    cube_mtl.metallicFactor = metallic
    cube_mtl.gpuInit(ctx)
    self.cube_mtl = cube_mtl
    self.cube_node = self.cube_prim.createNode("cube", self.layer1, cube_mtl)
    self.cube_node.worldTransform.translation = vec3(-0.8, 0, 0)
    self.cube_node.sortkey = 10

    # Sphere next to the cube (same material)
    sv_obj, sn_obj, sb_obj, su_obj, si_obj = make_sphere_arrays(0.6, 12)
    nv_obj = len(sv_obj)
    sphere_colors = np.zeros((nv_obj, 4), dtype=np.uint8)
    sphere_colors[:, 0] = int(obj_color[0] * 255)
    sphere_colors[:, 1] = int(obj_color[1] * 255)
    sphere_colors[:, 2] = int(obj_color[2] * 255)
    sphere_colors[:, 3] = 255
    self.sphere_prim = RigidPrimitive()
    self.sphere_prim.fromArrays(sv_obj, sn_obj, sb_obj, su_obj, sphere_colors, si_obj, ctx)
    sphere_mtl = lev2.PBRMaterial()
    sphere_mtl.assignImages(ctx, color=white_img, normal=normal_img, mtlruf=white_img, doConform=True)
    sphere_mtl.baseColor = vec4(1, 1, 1, 1)
    sphere_mtl.roughnessFactor = roughness
    sphere_mtl.metallicFactor = metallic
    sphere_mtl.gpuInit(ctx)
    self.sphere_mtl = sphere_mtl
    self.sphere_node = self.sphere_prim.createNode("sphere", self.layer1, sphere_mtl)
    self.sphere_node.worldTransform.translation = vec3(0.8, 0, 0)
    self.sphere_node.sortkey = 10

    # Light indicator sphere
    sv, sn, sb, su, si = make_sphere_arrays(0.08, 6)
    nv_s = len(sv)
    orb_colors = np.full((nv_s, 4), 255, dtype=np.uint8)
    self.orb_prim = RigidPrimitive()
    self.orb_prim.fromArrays(sv, sn, sb, su, orb_colors, si, ctx)
    orb_mtl = lev2.PBRMaterial()
    orb_mtl.assignImages(ctx, color=white_img, normal=normal_img, mtlruf=white_img, doConform=True)
    orb_mtl.baseColor = vec4(1, 1, 1, 1)
    orb_mtl.roughnessFactor = 0.0
    orb_mtl.metallicFactor = 0.0
    orb_mtl.gpuInit(ctx)
    self.orb_mtl = orb_mtl
    self.orb_node = self.orb_prim.createNode("orb", self.layer1, orb_mtl)
    self.orb_node.sortkey = 10

    # Point light
    self.light = lev2.DynamicPointLight()
    self.light.data.color = vec3(1, 1, 1)
    self.light.data.intensity = 5.0
    self.light.data.radius = 5.0
    self.light_node = self.layer1.createLightNode("orbit_light", self.light)

    self.scene.lightingmanager.gpuInit(ctx)

  def onGpuUpdate(self, ctx):
    t = self.time
    angle = t * 1.0
    x = 2.0 * math.cos(angle)
    z = 2.0 * math.sin(angle)
    y = 1.0
    self.light_node.setMatrix(mtx4.transMatrix(vec3(x, y, z)))
    self.orb_node.worldTransform.translation = vec3(x, y, z)

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
  app = BasicCubeLightApp()
  app.ezapp.mainThreadLoop()
