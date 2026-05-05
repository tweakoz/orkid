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
from orkengine.lev2 import RigidPrimitive, MicroMesh

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
  # Returns (verts_np, norms_np, binormals_np, faces).
  # `faces` is a flat [count, i0, i1, i2, ...] list with CCW-from-outside
  # winding (the natural authored direction). MicroMesh.fromVertAndFaceLists
  # reverses winding internally to match the vertex-color forward technique.
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

def make_sphere_arrays(radius=1.0, n=8):
  # Same convention as make_cube_arrays: faces are CCW-from-outside.
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

    def make_solid_color_prim(verts, norms, binormals, faces, color):
      n = len(verts)
      # BGRA pre-swap: MicroMesh::updateRigidPrim packs colors via
      # fvec4::ARGBU32 which lands as BGRA in memory; the GPU reads RGBA.
      colors = np.tile(np.array([color[2], color[1], color[0], 1.0],
                                dtype=np.float32), (n, 1))
      mesh = MicroMesh.fromVertAndFaceLists(verts, faces)
      mesh.updateNormals(norms)
      mesh.updateBinormals(binormals)
      mesh.updateColors(colors)
      prim = RigidPrimitive()
      prim.updateWithMicroMesh(mesh, ctx, tokens.TRIANGLES)
      return prim

    def make_pbr_material(roughness, metallic):
      mtl = lev2.PBRMaterial()
      mtl.assignImages(ctx, color=white_img, normal=normal_img,
                       mtlruf=white_img, doConform=True)
      mtl.baseColor = vec4(1, 1, 1, 1)
      mtl.roughnessFactor = roughness
      mtl.metallicFactor = metallic
      mtl.doubleSided = True  # lit on both sides; exercises the engine's dynamic cull path
      mtl.gpuInit(ctx)
      return mtl

    # Cube
    cv, cn, cb, cf = make_cube_arrays(size=1.0)
    self.cube_prim = make_solid_color_prim(cv, cn, cb, cf, obj_color)
    self.cube_mtl = make_pbr_material(roughness, metallic)
    self.cube_node = self.cube_prim.createNode("cube", self.layer1, self.cube_mtl)
    self.cube_node.worldTransform.translation = vec3(-0.8, 0, 0)
    self.cube_node.sortkey = 10

    # Sphere next to the cube (same material)
    sv_obj, sn_obj, sb_obj, sf_obj = make_sphere_arrays(0.6, 12)
    self.sphere_prim = make_solid_color_prim(sv_obj, sn_obj, sb_obj, sf_obj, obj_color)
    self.sphere_mtl = make_pbr_material(roughness, metallic)
    self.sphere_node = self.sphere_prim.createNode("sphere", self.layer1, self.sphere_mtl)
    self.sphere_node.worldTransform.translation = vec3(0.8, 0, 0)
    self.sphere_node.sortkey = 10

    # Light indicator sphere
    sv, sn, sb, sf = make_sphere_arrays(0.08, 6)
    self.orb_prim = make_solid_color_prim(sv, sn, sb, sf, (1.0, 1.0, 1.0))
    self.orb_mtl = make_pbr_material(0.0, 0.0)
    self.orb_node = self.orb_prim.createNode("orb", self.layer1, self.orb_mtl)
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
