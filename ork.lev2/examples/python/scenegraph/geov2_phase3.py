#!/usr/bin/env ork.python
################################################################################
# GEOV2 Phase 3 — asset integration + bindable uniform params.
#
# The surface is authored in the DSL with a ctx.param("base_color", ...): a
# BINDABLE runtime uniform (a member of the generated ublk_ptex_params block,
# bound via material.bindParam). The ptex3d asset wrapper materializes the DSL
# to a cached .fxv2 and packs a PbrMaterialGenData (shaderpath + the param
# defaults in shader_params). This demo proves the full Phase-3 loop:
#
#   1. author CastMetal (DSL) with a bindable base_color param
#   2. wrap via the asset layer  -> PbrMaterialGenData(shaderpath, shader_params)
#   3. ROUND-TRIP that gendata through JSON (serialize -> deserialize)
#   4. build a live PBRMaterial from the DESERIALIZED gendata (the .fxv2 +
#      the param pre-bound to its default)
#   5. drive base_color LIVE every frame via material.bindParam(callable)
#
# The reflected class lookup in step 3 only works once lev2 is initialized
# (RegisterClassX runs in lev2appinit), so the round-trip lives in onGpuInit —
# which is exactly the path a real scene takes through ork.scene.viewer.
#
#   ./geov2_phase3.py [--cell_scale N] [--still]
################################################################################

import math, sys, argparse
import numpy as np
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((lev2exdir() / "python").normalized.as_string)
from lev2utils.cameras import setupUiCamera
from ork.hypergraph.ptex3d import Ptex3d, P, rgb
import ork.hypergraph.ecs.scene.assets as hgassets

tokens = CrcStringProxy()

_ap = argparse.ArgumentParser(description="GEOV2 Phase 3 asset/param render test")
_ap.add_argument("--cell_scale", type=float, default=4.0)
_ap.add_argument("--still", action="store_true", help="hold base_color static (no live animation)")
_args = _ap.parse_args()

################################################################################
# The surface — base tone is a BINDABLE uniform (ctx.param), so it can be driven
# from Python/C++ at runtime rather than baked into the generated GLSL.
################################################################################

class CastMetal(Ptex3d):
  def __init__(self, ctx, *, cell_scale=4.0, rough=(0.0, 0.9)):
    base  = ctx.param("base_color", vec3(0.55, 0.57, 0.60)) # <- bindable tone
    swim  = ctx.param("swim", vec3(0.0, 0.0, 0.0))          # <- bindable cell drift
    wall  = ctx.param("wall", 0.05)                         # <- bindable wall width (cell units)
    cell  = P.voronoi(ctx.P_object * cell_scale + swim)     # .edge .fwedge .cell .cell2
    # .fwedge = the border width-corrected for surface grazing (constant apparent
    # width, world units, no crease dots). `wall` is the band width in cell units.
    seam  = P.smoothstep(0.0, wall, cell.fwedge)
    steel = base * P.mix(0.55, 1.0, cell.cell)              # per-cell tone off base
    steel = P.mix(steel, steel * rgb(1.06, 0.98, 0.90), ctx.Cd.w)   # Cd.w warm tint
    self.surface(
      albedo    = steel * P.mix(0.45, 1.0, seam),           # dark recessed seams
      metallic  = 1.0,
      roughness = P.mix(rough[0], rough[1], cell.cell2),    # flat per-cell gloss
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

class Phase3App:
  def __init__(self):
    self.ezapp = OrkEzApp.create(self, left=100, top=100, width=1280, height=720, ssaa=0)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 0, 8), constrainZ=True, up=vec3(0, 1, 0), fov_deg=60)
    self.time = 0.0

  def _swim_offset(self):
    # slowly drift the voronoi coordinate so the cell pattern "swims" across the
    # surface — proof of LIVE per-draw uniform control (a generator binding, not
    # a baked constant). Returns a vec4 (the vec4-padded swim slot; .xyz used).
    if _args.still:
      return vec4(0.0, 0.0, 0.0, 0.0)
    t = self.time
    return vec4(t * 0.17, t * 0.15, t * 0.19, 0.0)

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

    # ── 1+2. DSL -> asset wrapper -> PbrMaterialGenData(shaderpath, shader_params)
    wrap = hgassets.Ptex3d(dsl_class=CastMetal, cell_scale=_args.cell_scale)
    gd = wrap.gendata
    print("shaderpath        :", gd.shaderpath)
    print("bindable params   :", list(gd.shader_params.keys()))

    # ── 3. round-trip the reflected gendata through JSON (classes are
    #       registered now that lev2 is initialized).
    js  = gd.serializeJson()
    gd2 = Object.deserializeJson(js)
    print("round-trip class  :", type(gd2).__name__)
    print("round-trip path   :", gd2.shaderpath)
    print("round-trip params :", list(gd2.shader_params.keys()))
    assert gd2.shaderpath == gd.shaderpath
    assert "base_color" in list(gd2.shader_params.keys())

    # ── 4. build a live PBRMaterial from the DESERIALIZED gendata (the same
    #       path materialize_from_scenedata takes: PbrMaterialGenData -> PbrMaterial).
    mat = hgassets.PbrMaterial.from_gendata(gd2, ctx=ctx).build()
    self.material = mat
    print("base_color param  :", "resolved" if mat.param("base_color") else "MISSING")
    print("swim param        :", "resolved" if mat.param("swim") else "MISSING")

    # ── 5a. base_color: a static bind (constant uniform path).
    mat.bindParam("base_color", vec4(0.55, 0.57, 0.60, 1.0))
    # ── 5b. swim: a LIVE bind — a 0-arg callable re-evaluated every draw, so the
    #        voronoi cells slowly swim across the surface.
    mat.bindParam("swim", self._swim_offset)

    # geometry
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

    self.node = self.prim.createNode("ptex3node", self.layer, mat)
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

Phase3App().ezapp.mainThreadLoop()
