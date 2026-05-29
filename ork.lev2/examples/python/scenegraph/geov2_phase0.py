#!/usr/bin/env ork.python
################################################################################
# GEOV2 Phase 0 — prove the procedural-PBR substitution hook (NO codegen).
#
# Builds a UV-sphere as a lev2.Geometry with a synthesized 4D `Cd` SELECTOR
# channel (not a color) + a uv channel, adapts it to a RigidPrimitive, and
# renders it with a PBRMaterial whose `shaderpath` is overridden to the
# hand-authored geov2_phase0_proto.fxv2. This validates, with zero codegen:
#   * PBRMaterial.shaderpath substitution into the stock forward-PBR machine
#   * CV technique selection (Primitive callback sets _has_vtxcolors=true)
#   * the depth-prepass technique (forward node drives it)
#   * the four GEOV2 surface inputs reaching the fragment:
#       Cd (frg_clr) as a 4D selector, frg_opos (object space), uv, world pos
#
#   ./geov2_phase0.py
################################################################################

import math, sys, argparse
import numpy as np
from obt import path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((lev2exdir() / "python").normalized.as_string)
from lev2utils.cameras import setupUiCamera
this_dir = path.directoryOfInvokingModule()

tokens = CrcStringProxy()

_SHADERS = {"marble": "geov2_phase0_marble.fxv2",   # clean polished-marble surface
            "debug":  "geov2_phase0_proto.fxv2"}    # garish input-proving pattern
_ap = argparse.ArgumentParser(description="GEOV2 Phase 0 render test")
_ap.add_argument("--shader", choices=list(_SHADERS), default="marble",
                 help="surface shader (default: marble)")
SHADER_FILE = _SHADERS[_ap.parse_args().shader]

################################################################################

def make_uvsphere(radius, nu, nv):
  """Lat/long UV-sphere → (verts, normals, binormals, uvs, tris-flat)."""
  verts, norms, bins, uvs = [], [], [], []
  up = np.array([0.0, 1.0, 0.0], dtype=np.float32)
  for iv in range(nv + 1):
    v   = iv / nv
    phi = v * math.pi
    for iu in range(nu + 1):
      u     = iu / nu
      theta = u * 2.0 * math.pi
      n = np.array([math.sin(phi) * math.cos(theta),
                    math.cos(phi),
                    math.sin(phi) * math.sin(theta)], dtype=np.float32)
      verts.append(n * radius)
      norms.append(n)
      b = np.cross(n, up)
      if np.linalg.norm(b) < 1e-5:
        b = np.cross(n, np.array([1.0, 0.0, 0.0], dtype=np.float32))
      bins.append(b / (np.linalg.norm(b) + 1e-9))
      uvs.append([u, v])
  row  = nu + 1
  tris = []
  for iv in range(nv):
    for iu in range(nu):
      a = iv * row + iu; b = a + 1; c = a + row; d = c + 1
      tris += [a, b, c,  b, d, c]
  return (np.array(verts, dtype=np.float32),
          np.array(norms, dtype=np.float32),
          np.array(bins,  dtype=np.float32),
          np.array(uvs,   dtype=np.float32),
          np.array(tris,  dtype=np.int32))

################################################################################

class Phase0App:

  def __init__(self):
    self.ezapp = OrkEzApp.create(self, left=100, top=100, width=1280, height=720, ssaa=0)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 0, 8), constrainZ=True, up=vec3(0, 1, 0), fov_deg=60)
    self.time = 0.0

  def onGpuInit(self, ctx):
    sg_params = VarMap()
    sg_params.SkyboxIntensity  = 1.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity= 1.0
    sg_params.AmbientLevel     = vec3(0.06)
    sg_params.SkyboxTexPathStr = "<ork_envmaps2>/blender_forest.xir"
    sg_params.preset           = "ForwardPBR"
    self.scenegraph = self.ezapp.createScene(sg_params)
    self.layer      = self.scenegraph.createLayer("std_forward")

    ############################################
    # Geometry: UV-sphere + synthesized 4D Cd selector + uv
    ############################################
    verts, norms, bins, uvs, tris = make_uvsphere(2.5, 64, 48)

    # Cd is a 4D PER-VERTEX SELECTOR (not a color):
    #   .x = longitude  -> object-space band phase
    #   .y = latitude   -> roughness ramp
    #   .z = checker    -> metallic patches
    #   .w = hemisphere -> base-color blend (top vs bottom)
    u = uvs[:, 0]; v = uvs[:, 1]
    checker = ((np.floor(u * 6.0).astype(int) + np.floor(v * 6.0).astype(int)) % 2).astype(np.float32)
    cd = np.stack([u, v, checker, (norms[:, 1] * 0.5 + 0.5)], axis=1).astype(np.float32)

    geo = Geometry()
    geo.point["P"]        = verts
    geo.point["N"]        = norms
    geo.point["binormal"] = bins
    geo.point["uv"]       = uvs
    geo.point["Cd"]       = cd
    geo.addPolys(tris, sides=3)
    print("geo: num_points=%d num_polys=%d" % (geo.num_points, geo.num_polys))

    self.prim = RigidPrimitive()
    self.prim.updateWithMicroMesh(geo.toMicroMesh(), ctx, tokens.TRIANGLES)

    ############################################
    # PBRMaterial with the custom shader substituted in
    ############################################
    mat = PBRMaterial()
    mat.shaderpath = str(this_dir / SHADER_FILE)
    print("using surface shader: %s" % SHADER_FILE)
    mat.assignImages(
      ctx,
      color  = Image.createRGB8FromColor(8, 8, vec3(1.0)),
      normal = Image.createRGB8FromColor(8, 8, vec3(0.5, 1.0, 0.5)),
      mtlruf = Image.createRGB8FromColor(8, 8, vec3(1.0)),
      doConform=True)
    mat.baseColor       = vec4(1, 1, 1, 1)
    mat.roughnessFactor = 1.0
    mat.metallicFactor  = 1.0
    mat.gpuInit(ctx)
    self.material = mat

    self.node = self.prim.createNode("ptex0node", self.layer, mat)
    self.scenegraph.lightingmanager.gpuInit(ctx)

  def onUpdate(self, updinfo):
    self.time += updinfo.deltatime
    self.scenegraph.updateScene(self.cameralut)

  def onUiEvent(self, uievent):
    res = ui.HandlerResult()
    if self.uicam.uiEventHandler(uievent):
      self.camera.copyFrom(self.uicam.cameradata)
    return res

################################################################################

Phase0App().ezapp.mainThreadLoop()
