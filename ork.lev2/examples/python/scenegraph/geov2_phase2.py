#!/usr/bin/env ork.python
################################################################################
# GEOV2 Phase 2 — author the surface in the DSL (no hand-written GLSL).
#
# CastMetal is written as Python expressions over `ctx` atoms and `P.<op>`s;
# materialize_ptex3d runs the SSA/CSE emitter -> the Phase-1 generator -> a
# cached .fxv2, bound to a PBRMaterial and rendered. Same cast-metal look as
# Phase 1, but the surface is now DSL, not a GLSL string. Try --cell_scale.
#
#   ./geov2_phase2.py [--cell_scale N]
################################################################################

import math, sys, argparse
import numpy as np
from obt import path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((lev2exdir() / "python").normalized.as_string)
from lev2utils.cameras import setupUiCamera
from ork.hypergraph.ptex3d import Ptex3d, P, rgb, materialize_ptex3d

tokens = CrcStringProxy()

_ap = argparse.ArgumentParser(description="GEOV2 Phase 2 DSL render test")
_ap.add_argument("--cell_scale", type=float, default=4.0)
_args = _ap.parse_args()

################################################################################
# The surface — authored in the DSL (cf. the hand-written GLSL in Phase 1).
################################################################################

class CastMetal(Ptex3d):
  # NOTE: the generator applies a temp roughness linearizer (eff = r*0.7+0.3),
  # so authored 0.0 -> eff 0.3 (shiniest reachable), 1.0 -> eff 1.0.
  # Author the low end near 0 so low-hash cells read glossy ("some shiny bits").
  def __init__(self, ctx, *, cell_scale=4.0, rough=(0.0, 0.9)):
    cell  = P.voronoi(ctx.P_object * cell_scale)            # .edge .fwedge .cell .cell2
    # .fwedge = border width-corrected for surface grazing (constant apparent
    # width, world units, dot-free). Threshold is in coordinate units.
    seam  = P.smoothstep(0.0, 0.04, cell.fwedge)
    steel = P.mix(rgb(0.35), rgb(0.63), cell.cell)          # flat per-cell tone
    steel = P.mix(steel, steel * rgb(1.06, 0.98, 0.90), ctx.Cd.w)   # Cd.w warm tint
    self.surface(
      albedo    = steel * P.mix(0.45, 1.0, seam),           # dark recessed seams
      metallic  = 1.0,
      roughness = P.mix(rough[0], rough[1], cell.cell2),    # flat per-cell gloss (shiny→matte)
    )

################################################################################

def make_uvsphere(radius, nu, nv):
  verts, norms, bins, uvs = [], [], [], []
  up = np.array([0.0, 1.0, 0.0], dtype=np.float32)
  for iv in range(nv + 1):
    v = iv / nv; phi = v * math.pi
    for iu in range(nu + 1):
      u = iu / nu; theta = u * 2.0 * math.pi
      n = np.array([math.sin(phi) * math.cos(theta), math.cos(phi),
                    math.sin(phi) * math.sin(theta)], dtype=np.float32)
      verts.append(n * radius); norms.append(n)
      b = np.cross(n, up)
      if np.linalg.norm(b) < 1e-5:
        b = np.cross(n, np.array([1.0, 0.0, 0.0], dtype=np.float32))
      bins.append(b / (np.linalg.norm(b) + 1e-9)); uvs.append([u, v])
  row = nu + 1; tris = []
  for iv in range(nv):
    for iu in range(nu):
      a = iv * row + iu; b = a + 1; c = a + row; d = c + 1
      tris += [a, b, c,  b, d, c]
  return (np.array(verts, dtype=np.float32), np.array(norms, dtype=np.float32),
          np.array(bins, dtype=np.float32), np.array(uvs, dtype=np.float32),
          np.array(tris, dtype=np.int32))

################################################################################

class Phase2App:
  def __init__(self):
    self.ezapp = OrkEzApp.create(self, left=100, top=100, width=1280, height=720, ssaa=0)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 0, 8), constrainZ=True, up=vec3(0, 1, 0), fov_deg=60)
    self.time = 0.0

  def onGpuInit(self, ctx):
    sg_params = VarMap()
    sg_params.SkyboxIntensity = 1.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(0.06)
    sg_params.SkyboxTexPathStr = "<ork_envmaps2>/blender_forest.xir"
    sg_params.preset = "ForwardPBR"
    self.scenegraph = self.ezapp.createScene(sg_params)
    self.layer = self.scenegraph.createLayer("std_forward")

    # ── DSL → emitter → generated .fxv2 ──
    fxv2_path = materialize_ptex3d(CastMetal, cell_scale=_args.cell_scale)
    print("DSL-generated shader: %s" % fxv2_path)

    verts, norms, bins, uvs, tris = make_uvsphere(2.5, 64, 48)
    u = uvs[:, 0]; v = uvs[:, 1]
    cd = np.stack([u, v, np.zeros_like(u), (norms[:, 1] * 0.5 + 0.5)], axis=1).astype(np.float32)
    geo = Geometry()
    geo.point["P"] = verts; geo.point["N"] = norms; geo.point["binormal"] = bins
    geo.point["uv"] = uvs;   geo.point["Cd"] = cd
    geo.addPolys(tris, sides=3)
    print("geo: num_points=%d num_polys=%d" % (geo.num_points, geo.num_polys))

    self.prim = RigidPrimitive()
    self.prim.updateWithMicroMesh(geo.toMicroMesh(), ctx, tokens.TRIANGLES)

    # Our procedural shader feeds albedo/roughness/metallic straight into the
    # lighting (never samples CNMREA, never reads ModAlbedo) — so textures +
    # baseColor are unneeded. Factors are restored as IDENTITY (default
    # metallicFactor=0.0 would zero a procedural metal if anything multiplies it).
    mat = PBRMaterial()
    mat.shaderpath = fxv2_path
    mat.metallicFactor = 1.0
    mat.roughnessFactor = 1.0
    mat.gpuInit(ctx)
    self.material = mat

    self.node = self.prim.createNode("ptex2node", self.layer, mat)
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

Phase2App().ezapp.mainThreadLoop()
