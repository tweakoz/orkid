#!/usr/bin/env ork.python
"""
SKYLIGHT lane C slice C1 step 2 — L2 SH projection NUMERIC gate.

ProbeSHProjector integrates a probe cubemap against the 9 real L2 basis functions
with exact per-texel solid angles. Every case here is an ANALYTIC radiance field
whose SH coefficients are known in closed form, rendered into a cubemap RTT and
projected on the GPU, so the check is against mathematics rather than against a
recorded blob:

  A. UNIFORM  L(d) = C.
     Only Y00 survives: c0 = C * Y00 * 4pi = C * 3.5449077. Every other
     coefficient is 0. This case alone pins the solid-angle weighting (the
     weights must sum to 4pi) and the Y00 normalization.

  B. AXIS GRADIENT  L(d) = a + b*d.y   (a > |b|, so the field stays positive).
     c0 = a * 3.5449077, c1 (the Y1-1 = 0.4886025*y lobe) = b * 0.4886025 * 4pi/3
     = b * 2.0466534, everything else 0 — the L1 band, on the right axis, at the
     right amplitude.

  C. CLAMPED COSINE  L(d) = max(0, d.y).
     The one case with a non-trivial L2 band: c0 = sqrt(pi)/2, c1 = sqrt(pi/3),
     and the l=2 zonal about +y splits across c6 (Y20) and c8 (Y22) as
     -0.2477089 / -0.4290544 (their quadrature sum is the zonal amplitude
     sqrt(5*pi)/8, which is the rotation-invariance identity for the band).

Case B additionally SPOT-CHECKS the rendered cube itself with a direct compute
sample along the six axes — so a face-convention error shared between the
generating fragment shader and the projector cannot hide behind a self-consistent
(but wrong) SH result.

TOLERANCE: 1.0e-4 absolute for the uniform case (exact quadrature), 5.0e-3 for the
direction-varying cases at a 64px face (finite-texel discretization of a C0 field).
"""
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing
# resolves from the same tree as this test (worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine import core   # core before lev2
from orkengine import lev2
from ork.testing import headless_app, verdict

import math, struct

tokens = core.CrcStringProxy()

FACE_DIM = 64

TOL_EXACT = 1.0e-4
TOL_DISCR = 5.0e-3

FOURPI  = 4.0 * math.pi
Y00     = 0.2820948
Y1      = 0.4886025

# ---- closed forms -----------------------------------------------------------
UNIFORM_C   = (0.35, 0.62, 0.81)
GRAD_A      = 0.5
GRAD_B      = 0.3

C0_PER_UNIT = Y00 * FOURPI          # 3.5449077
C1_PER_UNIT = Y1 * FOURPI / 3.0     # 2.0466534

COSLOBE = [
  math.sqrt(math.pi) / 2.0,     # c0
  math.sqrt(math.pi / 3.0),     # c1  (Y1-1, the +y lobe)
  0.0,                          # c2
  0.0,                          # c3
  0.0,                          # c4
  0.0,                          # c5
  -0.2477089,                   # c6  (Y20)
  0.0,                          # c7
  -0.4290544,                   # c8  (Y22)
]

AXIS_DIRS = [
  (1.0, 0.0, 0.0), (-1.0, 0.0, 0.0),
  (0.0, 1.0, 0.0), (0.0, -1.0, 0.0),
  (0.0, 0.0, 1.0), (0.0, 0.0, -1.0),
]

# analytic-cubemap generator. The face->direction mapping is the SAME GL cube
# convention probe_sh.cpp uses, and gl_FragCoord row 0 is memory row 0 (no
# Y-flip in this renderer), so texel (u,v) here is the texel probe_sh.cpp will
# assign that direction to.
GEN_SHADER = """
fxconfig fxcfg_default {
  glsl_version = "330";
}
uniform_block ub_gen (descriptor_set 0) {
  vec4 Dims;    // xy = face dim, zw = 1/dim
  vec4 Params;  // x = mode, y = a, z = b, w = face index
  vec4 Color;   // uniform-mode radiance
}
vertex_interface vif_gen {
  inputs {
    vec4 position : POSITION;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec2 frg_uv0;
  }
}
fragment_interface fif_gen : vif_gen : ub_gen {
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
vertex_shader vs_gen : vif_gen {
  gl_Position = position;
  frg_uv0     = uv0;
}
fragment_shader ps_gen : fif_gen {
  vec2 st = gl_FragCoord.xy * Dims.zw;
  float u = st.x * 2.0 - 1.0;
  float v = st.y * 2.0 - 1.0;
  int f = int(Params.w);
  vec3 d;
  if (f == 0) { d = vec3( 1.0,  -v,  -u); }
  else if (f == 1) { d = vec3(-1.0,  -v,   u); }
  else if (f == 2) { d = vec3(   u, 1.0,   v); }
  else if (f == 3) { d = vec3(   u,-1.0,  -v); }
  else if (f == 4) { d = vec3(   u,  -v, 1.0); }
  else { d = vec3(  -u,  -v,-1.0); }
  d = normalize(d);
  float m = Params.x;
  float s = 0.0;
  vec3 rgb = Color.xyz;
  if (m > 0.5 && m < 1.5) {
    s = Params.y + Params.z * d.y;
    rgb = vec3(s, s, s);
  }
  if (m > 1.5) {
    s = max(0.0, d.y);
    rgb = vec3(s, s, s);
  }
  out_clr = vec4(rgb, 1.0);
}
state_block sb_gen : default {
  BlendMode = OFF;
  DepthTest = OFF;
  DepthMask = false;
  CullTest  = OFF;
}
technique tek_gen {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_gen;
    fragment_shader = ps_gen;
    state_block     = sb_gen;
  }
}
"""

# direct read of the generated cube (the convention spot-check)
PROBE_SHADER = """
fxconfig fxcfg_default {}
sampler_set sset_pc (descriptor_set 0) { samplerCube u_cube; }
storage_interface pif_out (descriptor_set 0) { buffer layout(std430) pouter { float OUT[]; }; }
storage_interface pif_dir (descriptor_set 0) { buffer layout(std430) pdir { float DIR[]; }; }
compute_interface iface_pc : sset_pc { storage { pif_out pif_dir }
                                       inputs { layout(local_size_x = 8); } }
compute_shader cs_probe_cube_read : iface_pc {
  uint i = gl_GlobalInvocationID.x;
  if (i >= 6u) { return; }
  vec3 d = normalize(vec3(DIR[i * 4u + 0u], DIR[i * 4u + 1u], DIR[i * 4u + 2u]));
  vec4 c = textureLod(u_cube, d, 0.0);
  OUT[i * 4u + 0u] = c.r;
  OUT[i * 4u + 1u] = c.g;
  OUT[i * 4u + 2u] = c.b;
  OUT[i * 4u + 3u] = c.a;
}
"""


def main():
  failures = []

  def check(label, ok, detail=""):
    print(f"  {'PASS' if ok else 'FAIL'} {label} {detail}", flush=True)
    if not ok:
      failures.append(label)
    return ok

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx
    fxi = ctx.FXI
    ci  = ctx.CI

    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "probe_sh_gen", GEN_SHADER)
    tek_gen    = mtl.technique("tek_gen")
    par_dims   = mtl.param("Dims")
    par_params = mtl.param("Params")
    par_color  = mtl.param("Color")
    rcfd       = lev2.RenderContextFrameData(ctx)
    dims       = core.vec4(float(FACE_DIM), float(FACE_DIM), 1.0 / FACE_DIM, 1.0 / FACE_DIM)

    projector = lev2.ProbeSHProjector()

    def make_cube(name):
      rtg = lev2.RtGroup(ctx, FACE_DIM, FACE_DIM)
      rtg.name = name
      rtg.cubeMap = True   # BEFORE createBuffer
      rtg.createBuffer(tokens.RGBA32F, tokens.color)
      return rtg

    def render_cube(rtg, mode, a, b, color):
      ctx.beginFrame()
      for iface in range(6):
        rtg.cubeRenderFace = iface
        ctx.FBI.rtGroupPush(rtg)
        mtl.begin(tek_gen, rcfd)
        mtl.bindParamVec4(par_dims, dims)
        mtl.bindParamVec4(par_params, core.vec4(float(mode), a, b, float(iface)))
        mtl.bindParamVec4(par_color, core.vec4(color[0], color[1], color[2], 1.0))
        ctx.DWI.fullscreenQuad()
        mtl.end(rcfd)
        ctx.FBI.rtGroupPop()
      ctx.endFrame()

    def project(rtg, slot):
      projector.project(ctx, rtg.texture(0), FACE_DIM, slot)
      return projector.coefficients(ctx, slot)

    def coeff_check(tag, coeffs, expect, tol, channel=None):
      # expect: list of 9 scalars; compared on all 3 channels unless `channel`
      # selects one (the uniform case is per-channel).
      worst = 0.0
      for i in range(9):
        c = coeffs[i]
        vals = [c.x, c.y, c.z] if channel is None else [[c.x, c.y, c.z][channel]]
        for v in vals:
          worst = max(worst, abs(v - expect[i]))
      check(tag, worst < tol, "worst=%.6f tol=%.6f" % (worst, tol))
      return worst

    ############################################################
    # A. uniform radiance
    ############################################################
    print("=== A. uniform radiance -> L0 only ===", flush=True)
    cube_u = make_cube("SHGateUniform")
    render_cube(cube_u, 0, 0.0, 0.0, UNIFORM_C)
    cu = project(cube_u, 0)
    check("uniform_readback_shape", len(cu) == 9, "len=%d" % len(cu))
    if len(cu) == 9:
      worst_l0 = 0.0
      for ch in range(3):
        exp = UNIFORM_C[ch] * C0_PER_UNIT
        got = [cu[0].x, cu[0].y, cu[0].z][ch]
        worst_l0 = max(worst_l0, abs(got - exp))
        check("uniform_c0_ch%d" % ch, abs(got - exp) < TOL_EXACT,
              "got=%.6f expect=%.6f" % (got, exp))
      worst_rest = 0.0
      for i in range(1, 9):
        c = cu[i]
        worst_rest = max(worst_rest, abs(c.x), abs(c.y), abs(c.z))
      check("uniform_higher_bands_zero", worst_rest < TOL_EXACT, "worst=%.8f" % worst_rest)
      # the solid-angle weights must sum to 4pi — read straight off c0/(C*Y00)
      implied_4pi = ([cu[0].x, cu[0].y, cu[0].z][0]) / (UNIFORM_C[0] * Y00)
      check("solid_angle_sums_to_4pi", abs(implied_4pi - FOURPI) < 1.0e-3,
            "implied=%.6f expect=%.6f" % (implied_4pi, FOURPI))

    ############################################################
    # B. axis gradient (+ cube-content spot check)
    ############################################################
    print("=== B. axis gradient -> L1 band on +y ===", flush=True)
    cube_g = make_cube("SHGateGradient")
    render_cube(cube_g, 1, GRAD_A, GRAD_B, (0.0, 0.0, 0.0))

    # spot-check the rendered cube along the axes before believing its SH
    ssbo_out = fxi.createShaderStorageBufferWithLength(6 * 4 * 4)
    ssbo_dir = fxi.createShaderStorageBufferWithLength(6 * 4 * 4)
    m = fxi.mapStorageBuffer(ssbo_dir, 0, 6 * 4 * 4, tokens.WRITE_ONLY)
    m.writeBytes(b"".join(struct.pack('4f', d[0], d[1], d[2], 0.0) for d in AXIS_DIRS), 0)
    fxi.unmapStorageBuffer(m)
    shader_pc = fxi.shaderFromShaderText("probe_sh_gate_read", PROBE_SHADER)
    cs_pc = fxi.computeShader(shader_pc, "cs_probe_cube_read")
    ci.beginDispatchPhase()
    ci.bindStorageBuffer(cs_pc, 0, ssbo_out)
    ci.bindStorageBuffer(cs_pc, 1, ssbo_dir)
    ci.bindSampler(cs_pc, 2, cube_g.texture(0))
    ci.dispatch(cs_pc, 1, 1, 1)
    ci.endDispatchPhase()
    m = fxi.mapStorageBuffer(ssbo_out, 0, 6 * 4 * 4, tokens.READ_ONLY)
    raw = bytes(m.data)
    fxi.unmapStorageBuffer(m)
    worst_axis = 0.0
    for i, d in enumerate(AXIS_DIRS):
      got = struct.unpack_from('4f', raw, i * 16)[0]
      exp = GRAD_A + GRAD_B * d[1]
      worst_axis = max(worst_axis, abs(got - exp))
    check("gradient_cube_content_matches_direction", worst_axis < 2.0e-3,
          "worst=%.6f" % worst_axis)

    cg = project(cube_g, 1)
    if len(cg) == 9:
      expect_g = [GRAD_A * C0_PER_UNIT, GRAD_B * C1_PER_UNIT, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
      coeff_check("gradient_all_coefficients", cg, expect_g, TOL_DISCR)
      check("gradient_c1_is_the_y_lobe",
            abs(cg[1].x - GRAD_B * C1_PER_UNIT) < TOL_DISCR
            and abs(cg[2].x) < TOL_DISCR and abs(cg[3].x) < TOL_DISCR,
            "c1=%.6f (expect %.6f) c2=%.6f c3=%.6f" %
            (cg[1].x, GRAD_B * C1_PER_UNIT, cg[2].x, cg[3].x))

    ############################################################
    # C. clamped cosine lobe (L2 band)
    ############################################################
    print("=== C. clamped cosine -> L0/L1/L2 bands ===", flush=True)
    cube_c = make_cube("SHGateCosLobe")
    render_cube(cube_c, 2, 0.0, 0.0, (0.0, 0.0, 0.0))
    cc = project(cube_c, 2)
    if len(cc) == 9:
      coeff_check("coslobe_all_coefficients", cc, COSLOBE, TOL_DISCR)
      # rotation invariance of the l=2 band: sqrt(c6^2+c8^2) == sqrt(5pi)/8
      band2 = math.sqrt(cc[6].x * cc[6].x + cc[8].x * cc[8].x)
      exp2  = math.sqrt(5.0 * math.pi) / 8.0
      check("coslobe_l2_band_energy", abs(band2 - exp2) < TOL_DISCR,
            "band2=%.6f expect=%.6f" % (band2, exp2))

    ############################################################
    # slot independence — three probes projected into one SSBO
    ############################################################
    print("=== slot independence ===", flush=True)
    cu2 = projector.coefficients(ctx, 0)
    check("slot0_survived_later_projections",
          len(cu2) == 9 and abs(cu2[0].x - UNIFORM_C[0] * C0_PER_UNIT) < TOL_EXACT,
          "c0.x=%.6f" % (cu2[0].x if len(cu2) == 9 else float('nan')))

    ok = (len(failures) == 0)
    detail = "dim=%d tol_exact=%.1e tol_discr=%.1e" % (FACE_DIM, TOL_EXACT, TOL_DISCR)
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)

  sys.exit(0 if ok else 1)


main()
