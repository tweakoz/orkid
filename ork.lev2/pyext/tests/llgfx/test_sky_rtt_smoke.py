#!/usr/bin/env ork.python
"""
SKYLIGHT lane B slice B1 — Vulkan RTT smoke gate (the named EARLY risk retirement
from ~/JUL22_skylight.md §4.B.1). Everything the Hillaire LUT chain rests on is
proven HERE, before any atmosphere math is trusted:

  A. EQUIRECT 2D RTT ROUND TRIP (hard requirement of this slice).
     A non-square (2:1) offscreen RtGroup is fragment-rastered with a
     direction-encoded equirect pattern, then a SECOND pass samples that RTG's
     texture into another RtGroup. Both are read back. This drives the exact
     seam every LUT bake uses — RtGroup(w!=h) -> PushRtGroup -> fullscreen quad
     -> PopRtGroup -> sample-as-texture -> captureAsFormat — so a failure here
     is a platform/engine problem, not a sky-math problem. The transmittance
     (256x64) and sky-view (192x108) LUTs are both non-square.

  B. VOLUME (3D-texture) RENDER TARGET — DEVICE CAPABILITY PROBE.
     The aerial-perspective froxel volume (later slice) needs fragment-raster
     into a slice of a 3D texture: a VK_IMAGE_TYPE_3D image created
     2D_ARRAY_COMPATIBLE with COLOR_ATTACHMENT usage. Context::
     supportsVolumeRenderTarget asks the driver that exact question.

     NOTE (reported by this gate, deliberately not fixed in slice B1): the
     ENGINE has no volume render-target path today — RtGroup can target a 2D
     buffer, a cube face or a TextureArray slice, and RtBuffer images are
     always created with depth=1 (vulkan_txi_from_rtg.cpp). ETEXTYPE_3D exists
     only as a default-texture placeholder plus a sampler3D bind path. So a
     PASS here means "the hardware can do it, build the path"; it does NOT mean
     the engine can already render into a volume.

Runs entirely inline on the bound context (the test_rtg_resize_extent idiom) —
no scene, no compositor. Shader is runtime text so this gate stays independent
of any .fxv2 asset it is meant to de-risk.
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

import numpy

tokens = core.CrcStringProxy()

EQUIRECT_W = 256
EQUIRECT_H = 128   # 2:1 equirect aspect

SHADER_TEXT = """
fxconfig fxcfg_default {
  glsl_version = "330";
}
uniform_block ub_rtt (descriptor_set 0) {
  vec4 Dims;  // xy = target dims, zw = 1/dims
}
sampler_set sset_rtt (descriptor_set 0) {
  sampler2D SrcMap;
}
vertex_interface vif_rtt {
  inputs {
    vec4 position : POSITION;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec2 frg_uv0;
  }
}
fragment_interface fif_rtt : vif_rtt : ub_rtt {
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
vertex_shader vs_rtt : vif_rtt {
  gl_Position = position;
  frg_uv0     = uv0;
}
fragment_shader ps_rtt_equirect : fif_rtt {
  // direction-encoded equirect pattern, straight off gl_FragCoord so the
  // result is independent of the fullscreen quad's UV-flip convention.
  vec2 uv     = gl_FragCoord.xy * Dims.zw;
  float phi   = uv.x * PI2 - PI;
  float theta = uv.y * PI;
  float st    = sin(theta);
  vec3 dir    = vec3(st * cos(phi), cos(theta), st * sin(phi));
  out_clr     = vec4(dir * 0.5 + 0.5, 1.0);
}
fragment_shader ps_rtt_resample : fif_rtt : sset_rtt {
  vec2 uv = gl_FragCoord.xy * Dims.zw;
  out_clr = texture(SrcMap, uv);
}
state_block sb_rtt : default {
  BlendMode = OFF;
  DepthTest = OFF;
  DepthMask = false;
  CullTest  = OFF;
}
technique tek_equirect {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_rtt;
    fragment_shader = ps_rtt_equirect;
    state_block     = sb_rtt;
  }
}
technique tek_resample {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_rtt;
    fragment_shader = ps_rtt_resample;
    state_block     = sb_rtt;
  }
}
"""


def _make_rtg(ctx, w, h, name):
  rtg = lev2.RtGroup(ctx, w, h)
  rtg.name = name
  rtg.createBuffer(tokens.RGBA32F, tokens.color)
  return rtg


def _readback(capbuf, w, h):
  return numpy.array(capbuf, dtype=numpy.float32).reshape(h, w, 4)


def main():
  failures = []
  results = {}

  def check(label, ok, detail=""):
    print(f"  {'PASS' if ok else 'FAIL'} {label} {detail}", flush=True)
    if not ok:
      failures.append(label)
    return ok

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx

    ############################################################
    # B. device capability probe (cheap, no frame needed)
    ############################################################
    print("=== volume render-target DEVICE capability ===", flush=True)
    vol_rgba16f = ctx.supportsVolumeRenderTarget("RGBA16F")
    vol_rgba32f = ctx.supportsVolumeRenderTarget("RGBA32F")
    print(f"  supportsVolumeRenderTarget(RGBA16F) = {vol_rgba16f}", flush=True)
    print(f"  supportsVolumeRenderTarget(RGBA32F) = {vol_rgba32f}", flush=True)
    print("  NOTE: engine has NO volume render-target path yet (RtBuffer images "
          "are depth=1); this probe answers the DEVICE question only.", flush=True)
    check("tex3d_rtt_device_capability", bool(vol_rgba16f))
    results["vol16f"] = bool(vol_rgba16f)
    results["vol32f"] = bool(vol_rgba32f)

    ############################################################
    # A. equirect 2D RTT round trip
    ############################################################
    print(f"=== equirect RTT round trip {EQUIRECT_W}x{EQUIRECT_H} ===", flush=True)
    rtg_write  = _make_rtg(ctx, EQUIRECT_W, EQUIRECT_H, "SmokeEquirectWrite")
    rtg_sample = _make_rtg(ctx, EQUIRECT_W, EQUIRECT_H, "SmokeEquirectSample")

    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "sky_rtt_smoke", SHADER_TEXT)
    tek_write  = mtl.technique("tek_equirect")
    tek_sample = mtl.technique("tek_resample")
    par_dims   = mtl.param("Dims")
    par_src    = mtl.param("SrcMap")
    rcfd       = lev2.RenderContextFrameData(ctx)

    dims = core.vec4(float(EQUIRECT_W), float(EQUIRECT_H),
                     1.0 / EQUIRECT_W, 1.0 / EQUIRECT_H)

    cap_write  = lev2.CaptureBuffer()
    cap_sample = lev2.CaptureBuffer()

    ctx.beginFrame()

    ctx.FBI.rtGroupPush(rtg_write)
    mtl.begin(tek_write, rcfd)
    mtl.bindParamVec4(par_dims, dims)
    ctx.DWI.fullscreenQuad()
    mtl.end(rcfd)
    ctx.FBI.rtGroupPop()

    ctx.FBI.rtGroupPush(rtg_sample)
    mtl.begin(tek_sample, rcfd)
    mtl.bindParamVec4(par_dims, dims)
    mtl.bindParamTexture(par_src, rtg_write.texture(0))
    ctx.DWI.fullscreenQuad()
    mtl.end(rcfd)
    ctx.FBI.rtGroupPop()

    fut_w = ctx.FBI.captureAsFormat(rtg_write.buffer(0), cap_write, "RGBA32F")
    fut_s = ctx.FBI.captureAsFormat(rtg_sample.buffer(0), cap_sample, "RGBA32F")

    ctx.endFrame()

    fut_w.wait(cap_write)
    fut_s.wait(cap_sample)

    arr_w = _readback(cap_write, EQUIRECT_W, EQUIRECT_H)
    arr_s = _readback(cap_sample, EQUIRECT_W, EQUIRECT_H)

    check("capture_extent_write", cap_write.width == EQUIRECT_W and cap_write.height == EQUIRECT_H,
          f"({cap_write.width}x{cap_write.height})")
    check("capture_extent_sample", cap_sample.width == EQUIRECT_W and cap_sample.height == EQUIRECT_H,
          f"({cap_sample.width}x{cap_sample.height})")

    rgb_w = arr_w[..., :3]
    check("equirect_rtt_nonblack", float(rgb_w.max()) > 0.05, f"max={float(rgb_w.max()):.4f}")

    # the encoded direction must be a unit vector everywhere (decode is *2-1)
    decoded = rgb_w * 2.0 - 1.0
    lens = numpy.sqrt((decoded * decoded).sum(axis=2))
    check("equirect_direction_unit_length", float(numpy.abs(lens - 1.0).max()) < 2.0e-2,
          f"maxdev={float(numpy.abs(lens - 1.0).max()):.5f}")

    # top row is the +Y pole, bottom row the -Y pole (v=0 -> theta=0)
    top_y    = float(decoded[0, :, 1].mean())
    bottom_y = float(decoded[-1, :, 1].mean())
    check("equirect_pole_orientation", top_y > 0.9 and bottom_y < -0.9,
          f"top_y={top_y:.4f} bottom_y={bottom_y:.4f}")

    # the round trip itself: sampling the RTT texture reproduces it
    maxdiff = float(numpy.abs(arr_s[..., :3] - rgb_w).max())
    check("equirect_sample_roundtrip", maxdiff < 1.0e-3, f"maxdiff={maxdiff:.6f}")
    results["roundtrip_maxdiff"] = maxdiff

    ok = (len(failures) == 0)
    detail = ("equirect=%dx%d roundtrip_maxdiff=%.6f vol3d_rt_device=%s" %
              (EQUIRECT_W, EQUIRECT_H, maxdiff, results["vol16f"]))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)

  sys.exit(0 if ok else 1)


main()
