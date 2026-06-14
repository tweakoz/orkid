#!/usr/bin/env ork.python
################################################################################
# GEOV2 Phase 1 — prove the .fxv2 template generator.
#
# A GLSL surface-function STRING (here: a worley-cell brushed metal, which also
# exercises extra_imports + lib_inherits via sdftools) is fed to
# ork.hypergraph.ptex3d.materialize_surface_fxv2 -> a complete, hash-named .fxv2
# under <staging>/dslshadercache/. That generated file is then bound to a
# PBRMaterial via shaderpath and rendered on a UV-sphere Geometry — proving the
# generator emits a valid forward-PBR shader (CV + depth-prepass) with the
# GEOV2 inputs (Cd/opos/uv) reaching the generated fragment. No DSL yet (Phase 2).
#
#   ./geov2_phase1.py
################################################################################

import math, sys
import numpy as np
from obt import path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((lev2exdir() / "python").normalized.as_string)
from lev2utils.cameras import setupUiCamera
from ork.hypergraph.ptex3d import materialize_surface_fxv2

tokens = CrcStringProxy()

################################################################################
# The surface body — a worley-cell brushed metal. Locals in scope:
#   wpos, opos, uv, cd (4D selector), tbn, wnrm, eye  →  writes o.<field>.
################################################################################

# Per-cell-hash voronoi (self-contained — only fract/sin/floor). Goes in the
# generator's `libblock=` param. Returns vec4(F1, F2, hashA, hashB): F1/F2 for
# the seam darkening, two independent flat-per-cell randoms (decorrelated tone
# vs gloss) so each cell is a distinct cast-metal plate — no radial rings.
LIBBLOCK = """
float shash3(vec3 p) {
  return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}
vec3 vhash3(vec3 p) {
  return fract(sin(vec3(dot(p, vec3(127.1, 311.7, 74.7)),
                        dot(p, vec3(269.5, 183.3, 246.1)),
                        dot(p, vec3(113.5, 271.9, 124.6)))) * 43758.5453);
}
vec4 voronoi_cell(vec3 x) {
  vec3 ip = floor(x), fp = fract(x);
  float f1 = 1e9, f2 = 1e9, idA = 0.0, idB = 0.0;
  for (int k = -1; k <= 1; k++)
  for (int j = -1; j <= 1; j++)
  for (int i = -1; i <= 1; i++) {
    vec3 g  = vec3(float(i), float(j), float(k));
    vec3 r  = g + vhash3(ip + g) - fp;
    float d = dot(r, r);
    if (d < f1)      { f2 = f1; f1 = d; idA = shash3(ip + g); idB = shash3(ip + g + vec3(31.7)); }
    else if (d < f2) { f2 = d; }
  }
  return vec4(sqrt(f1), sqrt(f2), idA, idB);
}
"""

# Cast-metal plates: flat per-cell tone + gloss, dark recessed seams.
SURFACE_BODY = """
vec4 c    = voronoi_cell(opos * 4.0);                 // F1,F2, hashA(tone), hashB(gloss)
float seam = smoothstep(0.0, 0.06, c.y - c.x);        // 1 in cell interior, 0 at seams
vec3 steel = mix(vec3(0.34, 0.35, 0.38), vec3(0.62, 0.63, 0.66), c.z);  // flat per-cell tone
steel      = mix(steel, steel * vec3(1.06, 0.98, 0.90), cd.w);          // Cd.w subtle warm tint
o.albedo   = steel * mix(0.45, 1.0, seam);            // dark recessed seams
o.metallic = 1.0;
// flat per-cell roughness spread across the PERCEPTUALLY visible range
// (roughness non-linear: ~0.5-0.6 part-shiny, ~0.7 rough/shape-visible, ~0.85 matte)
o.roughness= mix(0.25, 0.85, c.w);
"""

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

class Phase1App:
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

    # ── generate the .fxv2 from the surface-body STRING ──
    fxv2_path = materialize_surface_fxv2(
      SURFACE_BODY,
      libblock=LIBBLOCK,
      name_hint="castmetal")
    print("generated shader: %s" % fxv2_path)

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

    mat = PBRMaterial()
    mat.shaderpath = fxv2_path
    mat.assignImages(ctx,
                     color=Image.createRGB8FromColor(8, 8, vec3(1.0)),
                     normal=Image.createRGB8FromColor(8, 8, vec3(0.5, 1.0, 0.5)),
                     mtlruf=Image.createRGB8FromColor(8, 8, vec3(1.0)),
                     doConform=True)
    mat.baseColor = vec4(1, 1, 1, 1)
    mat.roughnessFactor = 1.0
    mat.metallicFactor = 1.0
    mat.gpuInit(ctx)
    self.material = mat

    self.node = self.prim.createNode("ptex1node", self.layer, mat)
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

Phase1App().ezapp.mainThreadLoop()
