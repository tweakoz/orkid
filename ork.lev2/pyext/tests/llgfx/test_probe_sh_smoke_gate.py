#!/usr/bin/env ork.python
"""
SKYLIGHT lane C slice C1 step 1 — samplerCube-in-compute smoke gate (spec trouble
point T7, the named EARLY risk retirement of the probe pillar).

NOTHING in the SH lane is trusted until this passes: no compute shader in the repo
had ever sampled a cubemap (the HZB samples a sampler2D), and the Vulkan backend's
VkComputeInterface::bindImage is a stub — so the ONLY route from a probe cubemap to
a compute kernel is bindSampler + samplerCube + an SSBO write. This gate drives
exactly that seam and nothing else:

  1. a CUBEMAP RtGroup (RtGroup._cubeMap, VK_IMAGE_VIEW_TYPE_CUBE view built by
     vulkan_txi_from_rtg) is fragment-rastered face by face with six distinct
     known colors, one per cube array layer;
  2. that RTG's texture is bound to a compute shader as `samplerCube` and sampled
     with textureLod (compute has no implicit-LOD derivatives) along the six axis
     directions plus four off-axis directions;
  3. the results are read back out of an SSBO and matched against the colors.

A FAILURE here is the spec's fallback trigger (fragment-raster reduction instead of
compute) — it is not an SH-math problem. A PASS also pins the face-layer convention
the projector relies on: cubeRenderFace f targets array layer f, and layer order is
+X,-X,+Y,-Y,+Z,-Z.
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

import struct

tokens = core.CrcStringProxy()

FACE_DIM = 32

# one per cube array layer, in Vulkan face order: +X,-X,+Y,-Y,+Z,-Z
FACE_COLORS = [
  (0.90, 0.10, 0.10),
  (0.10, 0.90, 0.10),
  (0.10, 0.10, 0.90),
  (0.90, 0.90, 0.10),
  (0.90, 0.10, 0.90),
  (0.10, 0.90, 0.90),
]

AXIS_DIRS = [
  (1.0, 0.0, 0.0), (-1.0, 0.0, 0.0),
  (0.0, 1.0, 0.0), (0.0, -1.0, 0.0),
  (0.0, 0.0, 1.0), (0.0, 0.0, -1.0),
]

# off-axis probes: direction, and the layer whose |component| dominates
OFFAXIS = [
  ((0.9, 0.3, 0.2), 0),
  ((-0.2, -0.9, 0.3), 3),
  ((0.1, 0.2, -0.95), 5),
  ((-0.8, 0.1, -0.3), 1),
]

NUM_SAMPLES = len(AXIS_DIRS) + len(OFFAXIS)

FILL_SHADER = """
fxconfig fxcfg_default {
  glsl_version = "330";
}
uniform_block ub_fill (descriptor_set 0) {
  vec4 FillColor;
}
vertex_interface vif_fill {
  inputs {
    vec4 position : POSITION;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec2 frg_uv0;
  }
}
fragment_interface fif_fill : vif_fill : ub_fill {
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
vertex_shader vs_fill : vif_fill {
  gl_Position = position;
  frg_uv0     = uv0;
}
fragment_shader ps_fill : fif_fill {
  out_clr = FillColor;
}
state_block sb_fill : default {
  BlendMode = OFF;
  DepthTest = OFF;
  DepthMask = false;
  CullTest  = OFF;
}
technique tek_fill {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_fill;
    fragment_shader = ps_fill;
    state_block     = sb_fill;
  }
}
"""

# storage_interface bindings are allocated before sampler bindings (the HZB
# precedent: two storage blocks at 0,1 and its sampler2D at 2).
SAMPLE_SHADER = """
fxconfig fxcfg_default {}
sampler_set sset_cs (descriptor_set 0) { samplerCube u_cube; }
storage_interface sif_out (descriptor_set 0) { buffer layout(std430) souter { float OUT[]; }; }
storage_interface sif_dir (descriptor_set 0) { buffer layout(std430) sdir { float DIR[]; }; }
compute_interface iface_cs : sset_cs { storage { sif_out sif_dir }
                                       inputs { layout(local_size_x = 16); } }
compute_shader cs_probe_cube_smoke : iface_cs {
  uint i = gl_GlobalInvocationID.x;
  if (i >= %NUMSAMPLES%u) { return; }
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

    ############################################################
    # 1. cubemap RTT with six known face colors
    ############################################################
    print(f"=== cubemap RTT {FACE_DIM}x{FACE_DIM}x6 ===", flush=True)
    rtg = lev2.RtGroup(ctx, FACE_DIM, FACE_DIM)
    rtg.name = "ProbeSHSmokeCube"
    rtg.cubeMap = True   # BEFORE createBuffer — it selects ETEXTYPE_CUBE
    rtg.createBuffer(tokens.RGBA32F, tokens.color)

    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "probe_sh_smoke_fill", FILL_SHADER)
    tek_fill  = mtl.technique("tek_fill")
    par_color = mtl.param("FillColor")
    rcfd      = lev2.RenderContextFrameData(ctx)

    ctx.beginFrame()
    for iface in range(6):
      r, g, b = FACE_COLORS[iface]
      rtg.cubeRenderFace = iface
      ctx.FBI.rtGroupPush(rtg)
      mtl.begin(tek_fill, rcfd)
      mtl.bindParamVec4(par_color, core.vec4(r, g, b, 1.0))
      ctx.DWI.fullscreenQuad()
      mtl.end(rcfd)
      ctx.FBI.rtGroupPop()
    ctx.endFrame()

    cubetex = rtg.texture(0)
    check("cube_rtt_texture_exists", cubetex is not None)

    ############################################################
    # 2. compute samples the cube -> SSBO
    ############################################################
    print("=== samplerCube in compute ===", flush=True)
    dirs = list(AXIS_DIRS) + [d for d, _ in OFFAXIS]

    ssbo_out = fxi.createShaderStorageBufferWithLength(NUM_SAMPLES * 4 * 4)
    ssbo_dir = fxi.createShaderStorageBufferWithLength(NUM_SAMPLES * 4 * 4)

    mapping = fxi.mapStorageBuffer(ssbo_dir, 0, NUM_SAMPLES * 4 * 4, tokens.WRITE_ONLY)
    payload = b"".join(struct.pack('4f', d[0], d[1], d[2], 0.0) for d in dirs)
    mapping.writeBytes(payload, 0)
    fxi.unmapStorageBuffer(mapping)

    text = SAMPLE_SHADER.replace("%NUMSAMPLES%", str(NUM_SAMPLES))
    shader = fxi.shaderFromShaderText("probe_sh_smoke_cs", text)
    cs = fxi.computeShader(shader, "cs_probe_cube_smoke")
    check("compute_shader_compiled", cs is not None)

    ci.beginDispatchPhase()
    ci.bindStorageBuffer(cs, 0, ssbo_out)
    ci.bindStorageBuffer(cs, 1, ssbo_dir)
    ci.bindSampler(cs, 2, cubetex)
    ci.dispatch(cs, 1, 1, 1)
    ci.endDispatchPhase()

    mapping = fxi.mapStorageBuffer(ssbo_out, 0, NUM_SAMPLES * 4 * 4, tokens.READ_ONLY)
    raw = bytes(mapping.data)
    fxi.unmapStorageBuffer(mapping)
    samples = [struct.unpack_from('4f', raw, i * 16) for i in range(NUM_SAMPLES)]

    ############################################################
    # 3. verdict on the samples
    ############################################################
    print("=== axis-direction face identity ===", flush=True)
    worst = 0.0
    for i, expect_layer in enumerate(range(6)):
      got = samples[i]
      exp = FACE_COLORS[expect_layer]
      err = max(abs(got[j] - exp[j]) for j in range(3))
      worst = max(worst, err)
      check("axis_%d_reads_layer_%d" % (i, expect_layer), err < 1.0e-3,
            "got=(%.4f,%.4f,%.4f) expect=(%.2f,%.2f,%.2f)" % (got[0], got[1], got[2], exp[0], exp[1], exp[2]))

    print("=== off-axis dominance ===", flush=True)
    for k, (d, expect_layer) in enumerate(OFFAXIS):
      got = samples[6 + k]
      exp = FACE_COLORS[expect_layer]
      err = max(abs(got[j] - exp[j]) for j in range(3))
      worst = max(worst, err)
      check("offaxis_%d_layer_%d" % (k, expect_layer), err < 1.0e-3,
            "dir=%s got=(%.4f,%.4f,%.4f)" % (str(d), got[0], got[1], got[2]))

    # a null/black descriptor is the classic silent failure of an unsupported
    # sampler binding — assert non-black independently of the identity checks.
    maxc = max(max(s[0], s[1], s[2]) for s in samples)
    check("compute_cube_samples_nonblack", maxc > 0.05, "max=%.4f" % maxc)

    ok = (len(failures) == 0)
    detail = "dim=%d samples=%d worst_err=%.6f" % (FACE_DIM, NUM_SAMPLES, worst)
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)

  sys.exit(0 if ok else 1)


main()
