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
from orkengine.lev2 import RigidPrimitive, MicroMesh

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

################################################################################

def make_sphere_arrays(radius=1.0, n=12):
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

################################################################################

class PointLightsApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self, width=1280, height=720,
                                      use_subsystems=["opq", "core", "gpu", "lev2"])
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

    def make_solid_color_prim(verts, norms, binormals, faces, color, alpha):
      n = len(verts)
      # BGRA pre-swap — see vtxcolor_materials.py for the rationale.
      colors = np.tile(np.array([color[2], color[1], color[0], alpha],
                                dtype=np.float32), (n, 1))
      mesh = MicroMesh.fromVertAndFaceLists(verts, faces)
      mesh.updateNormals(norms)
      mesh.updateBinormals(binormals)
      mesh.updateColors(colors)
      prim = RigidPrimitive()
      prim.updateWithMicroMesh(mesh, ctx, tokens.TRIANGLES)
      return prim

    def make_pbr_material(roughness, metallic, alpha_blend):
      mtl = lev2.PBRMaterial()
      mtl.assignImages(ctx, color=white_img, normal=normal_img,
                       mtlruf=white_img, doConform=True)
      mtl.baseColor = vec4(1, 1, 1, 1)
      mtl.roughnessFactor = roughness
      mtl.metallicFactor = metallic
      if alpha_blend:
        mtl.alphaBlend = True
      mtl.gpuInit(ctx)
      return mtl

    sv, sn, sb, sf = make_sphere_arrays(1.0, 12)
    self.materials = []
    self.prims = []

    # Three spheres: matte, metallic, mirror
    configs = [
      ("matte",    (-3.75, 1.2, 0), (0.6, 0.55, 0.5),  0.9, 0.0, False),
      ("metallic", (-1.25, 1.2, 0), (0.7, 0.5, 0.3),  0.3, 1.0, False),
      ("mirror",   (1.25,  1.2, 0), (0.9, 0.9, 0.9),  0.0, 1.0, False),
      ("glass",    (3.75,  1.2, 0), (0.9, 0.95, 1.0),  0.0, 1.0, True),
    ]

    for name, pos, color, roughness, metallic, alpha_blend in configs:
      alpha = 0.3 if alpha_blend else 1.0
      prim = make_solid_color_prim(sv, sn, sb, sf, color, alpha)
      mtl = make_pbr_material(roughness, metallic, alpha_blend)
      self.materials.append(mtl)
      self.prims.append(prim)
      node = prim.createNode(name, self.layer1, mtl)
      node.worldTransform.translation = vec3(*pos)
      node.sortkey = 20 if alpha_blend else 10

    # Three orbiting point lights with indicator spheres
    orb_sv, orb_sn, orb_sb, orb_sf = make_sphere_arrays(0.12, 6)
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
      light.data.radius = 1.5
      light_node = self.layer1.createLightNode(f"light_{name}", light)

      orb_color = (min(color.x, 1.0), min(color.y, 1.0), min(color.z, 1.0))
      orb_prim = make_solid_color_prim(orb_sv, orb_sn, orb_sb, orb_sf, orb_color, 1.0)
      orb_mtl = make_pbr_material(0.0, 0.0, False)
      self.materials.append(orb_mtl)
      self.prims.append(orb_prim)
      orb_node = orb_prim.createNode(f"orb_{name}", self.layer1, orb_mtl)

      self.lights.append((light, light_node, orb_node))

    self.scene.lightingmanager.gpuInit(ctx)

    print("Point Lights Test:")
    print("  matte sphere (rough=0.9, metal=0)")
    print("  metallic sphere (rough=0.3, metal=1)")
    print("  mirror sphere (rough=0, metal=1)")
    print("  glass sphere (rough=0, metal=1, transparent)")
    print("  Three colored point lights orbit in an ellipse")

  ##############################################

  def onGpuUpdate(self, ctx):
    t = self.time
    rx, rz = 6.0, 3.5  # ellipse radii
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
