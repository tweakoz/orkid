#!/usr/bin/env ork.python

################################################################################
# Procedural Transparency Test
# Demonstrates alpha blending with PBR vertex-color technique:
# - Opaque vs transparent objects (depth writes, sort keys)
# - Per-object opacity via ModColor.a (node.modcolor)
# - Time-varying opacity animation
# - Transparent objects around opaque objects
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
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

################################################################################

def make_sphere_arrays(radius=1.0, n=12):
  # Faces are CCW-from-outside; MicroMesh handles winding internally.
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
  verts = []
  norms = []
  faces = []
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

def create_prim(ctx, verts, norms, binormals, faces, color, alpha, roughness, metallic, alpha_blend, white_img, normal_img):
  nv = len(verts)
  r, g, b = color
  # BGRA pre-swap — see vtxcolor_materials.py for the rationale.
  colors_np = np.tile(np.array([b, g, r, alpha], dtype=np.float32), (nv, 1))

  mesh = MicroMesh.fromVertAndFaceLists(verts, faces)
  mesh.updateNormals(norms)
  mesh.updateBinormals(binormals)
  mesh.updateColors(colors_np)
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
  return prim, mtl

################################################################################

class TransparencyApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self, width=1280, height=720,
                                      use_subsystems=["opq", "core", "gpu", "lev2"])
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 3, 8), tgt=vec3(0, 1.5, 0))
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

    self.white_img = lev2.Image.createFromFile("src://effect_textures/white.dds")
    self.normal_img = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    sv, sn, sb, sf = make_sphere_arrays(radius=1.0, n=12)
    cv, cn, cb, cf = make_cube_arrays(size=0.8)

    self.materials = []
    self.prims = []

    # Center: opaque red cube
    prim, mtl = create_prim(ctx, cv, cn, cb, cf,
      (0.7, 0.1, 0.05), 1.0, 0.5, 0.0, False, self.white_img, self.normal_img)
    self.materials.append(mtl)
    self.prims.append(prim)
    self.cube_node = prim.createNode("cube", self.layer1, mtl)
    self.cube_node.worldTransform.translation = vec3(0, 1.2, 0)
    self.cube_node.sortkey = 10

    # Left: transparent blue sphere (fixed alpha 0.35)
    prim, mtl = create_prim(ctx, sv, sn, sb, sf,
      (0.6, 0.7, 1.0), 0.35, 0.0, 1.0, True, self.white_img, self.normal_img)
    self.materials.append(mtl)
    self.prims.append(prim)
    self.sphere_fixed = prim.createNode("sphere_fixed", self.layer1, mtl)
    self.sphere_fixed.worldTransform.translation = vec3(-3, 1.5, 0)
    self.sphere_fixed.sortkey = 20

    # Center: transparent sphere around the cube (mirror + transparent)
    prim, mtl = create_prim(ctx, sv, sn, sb, sf,
      (0.85, 0.9, 1.0), 0.3, 0.0, 1.0, True, self.white_img, self.normal_img)
    self.materials.append(mtl)
    self.prims.append(prim)
    self.sphere_around = prim.createNode("sphere_around", self.layer1, mtl)
    self.sphere_around.worldTransform.translation = vec3(0, 1.5, 0)
    self.sphere_around.worldTransform.scale = 1.8
    self.sphere_around.sortkey = 20

    # Right: sphere with time-varying opacity via ModColor.a
    prim, mtl = create_prim(ctx, sv, sn, sb, sf,
      (0.9, 0.3, 0.1), 1.0, 0.3, 0.0, True, self.white_img, self.normal_img)
    self.materials.append(mtl)
    self.prims.append(prim)
    self.sphere_fading = prim.createNode("sphere_fading", self.layer1, mtl)
    self.sphere_fading.worldTransform.translation = vec3(3, 1.5, 0)
    self.sphere_fading.sortkey = 20

    # Tint test: opaque sphere with animated tint via ModColor.rgb
    prim, mtl = create_prim(ctx, sv, sn, sb, sf,
      (0.8, 0.8, 0.8), 1.0, 0.5, 0.0, False, self.white_img, self.normal_img)
    self.materials.append(mtl)
    self.prims.append(prim)
    self.sphere_tint = prim.createNode("sphere_tint", self.layer1, mtl)
    self.sphere_tint.worldTransform.translation = vec3(0, 1.5, -3)
    self.sphere_tint.sortkey = 10

    # Lighting
    self.point_light = lev2.DynamicPointLight()
    self.point_light.data.color = vec3(1, 0.95, 0.85)
    self.point_light.data.intensity = 5.0
    self.point_light.data.radius = 25.0
    self.light_node = self.layer1.createLightNode("key", self.point_light)
    self.light_node.setMatrix(mtx4.transMatrix(vec3(3, 5, 5)))

    self.scene.lightingmanager.gpuInit(ctx)

    print("Transparency Test:")
    print("  Left: fixed alpha (0.35) transparent mirror sphere")
    print("  Center: opaque cube inside transparent sphere")
    print("  Right: time-varying opacity (sin(t))")
    print("  Back: animated tint via ModColor.rgb")

  ##############################################

  def onGpuUpdate(self, ctx):
    t = self.time

    # Animate opacity on right sphere via ModColor.a
    opacity = 0.5 + 0.5 * math.sin(t * 1.5)
    self.sphere_fading.modcolor = vec4(1, 1, 1, opacity)

    # Animate tint on back sphere via ModColor.rgb
    r = 0.5 + 0.5 * math.sin(t * 0.7)
    g = 0.5 + 0.5 * math.sin(t * 0.7 + math.tau / 3)
    b = 0.5 + 0.5 * math.sin(t * 0.7 + 2 * math.tau / 3)
    self.sphere_tint.modcolor = vec4(r, g, b, 1.0)

    # Rotate the cube
    self.cube_node.worldTransform.orientation = quat(vec3(0, 1, 0), t * 0.5)

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
  app = TransparencyApp()
  app.ezapp.mainThreadLoop()
