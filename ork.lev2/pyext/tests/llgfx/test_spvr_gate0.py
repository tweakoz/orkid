#!/usr/bin/env ork.python
################################################################################
# SPVR GATE 0 — single-pass-stereo cheap-kill probe (linux/NV, offscreen).
#
# THE QUESTION: can this engine render two DIFFERENT views into two layers of one
# multiview RtGroup, in ONE pass, validation-clean? If not, the SPVR program stops.
#
# SUBJECT. Four world-space quads at four depths, drawn below the compositor with a
# FreestyleMaterial that carries a mono/stereo technique PAIR. The stereo variant is
# the production idiom verbatim — `(spvr_vp[ofx_viewIndex] * MatM) * position`, the
# per-view view-projection read out of ublk_stereo and indexed by the multiview view
# selector. The two views are two camera positions with different eye offsets, so the
# near quads shift much more than the far ones: real depth-dependent parallax, not a
# uniform slide that a broken index-select could imitate.
#
# REFERENCE. The SAME scene, same cameras, rendered one view at a time through the
# ordinary non-multiview path into a 1-layer RtGroup. Layer i must match reference i.
#
# LEGS (all must pass)
#   (a) layer0 != layer1                     -- the two views really are two views
#   (b) SSIM(layer_i, mono_ref_i) > 0.98     -- each layer is the RIGHT view
#       plus the cross-pair sanity: SSIM(layer_i, mono_ref_1-i) must NOT pass, or
#       the views are indistinguishable and leg (b) proves nothing
#   (c) zero VULKAN ERROR and zero VULKAN WARNING across every child run
#
# NEGATIVE CONTROLS (both required; each armed in its own process, because both
# engine hooks cache their env read once)
#   NC1  ORKID_GATE0_VIEWMASK=1        -> layer1 is never rendered; it comes back at
#                                         the clear value and leg (b) for layer1 FAILS
#   NC2  ORKID_GATE0_FORCE_MONO_TEK=1  -> both views take the MONO technique, both
#                                         layers render view 0, and leg (a) FAILS
# Each control is followed by the RESTORED run: a control that goes red while the
# restored run stays green is a control; one that goes red and stays red is a break.
#
# ARMED-VALIDATION SMOKE. "Zero validation lines" is evidence only if the validator
# was armed, and the arming banner rides a default-off log channel. One child runs
# with ORKID_LOGCHAN_VKIMPL=1 purely to show the banner under the same binary/env.
#
# Self-configuring: ORKID_VULKAN_VALIDATE=2 (continue mode — =1 traps via
# __builtin_trap with no printed evidence, which would false-negative the grep).
# Default invocation needs no arguments and no environment.
#
#   ork.python test_spvr_gate0.py [outdir]
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
# CONTINUE mode, set before any engine init. =1 traps directly (no log line at all),
# so a validation-counting gate MUST use =2 or it counts zero and calls that clean.
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")

import sys
import time
import subprocess
sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "bin"))

W = H = 512
ASPECT = float(W) / float(H)

# leg (b) bar: the only in-tree SSIM pass precedent (ork.vet.image.py golden.ssim).
SSIM_PASS = 0.98
# a cross-pair at or above this would mean the two views are not distinguishable,
# which would make leg (b) vacuous. Kept well under SSIM_PASS deliberately.
SSIM_CROSS_CEIL = 0.98
# leg (a) bar, in 0..1 luma: the fraction of pixels that must differ between views.
DIFF_FRAC_FLOOR = 0.02
DIFF_EPS = 2.0 / 255.0   # one 8-bit code either way is not a difference
# non-degenerate bars, 0..1 luma (a black or flat capture passes SSIM against another
# black capture, so every image is floor-checked before any comparison is believed).
MEAN_FLOOR = 0.02
RANGE_FLOOR = 0.05

# eyes: distinct positions AND distinct heights, both looking at the origin.
EYES = ((-0.75, 0.20, 6.0), (0.75, -0.20, 6.0))

# world-space quads: (center_x, center_y, half_extent, world_z, tint). Ordered
# FAR to NEAR so the painter order matches the depth order. The z spread is what
# makes the disparity depth-dependent.
QUADS = (
    (-0.90, 0.50, 2.20, -6.0, (1.00, 0.30, 0.30)),
    (1.10, -0.60, 1.60, -2.5, (0.30, 1.00, 0.40)),
    (-0.40, -0.20, 1.00, 0.5, (0.40, 0.50, 1.00)),
    (0.60, 0.90, 0.55, 2.5, (1.00, 0.95, 0.35)),
)

################################################################################
# The mono/stereo technique pair. ONE fragment stage, ONE geometry stream, ONE
# uniform block: the only difference between the two vertex stages is where the
# clip transform comes from — which is exactly the difference GATE 0 is about.
#
# MatMVP is bound on the stereo draws too even though the stereo stage does not
# read it: NC2 forces the MONO technique inside the stereo pass, and the technique
# it lands on has to find a usable transform there or the control would collapse
# the layers to two blank images (identical for the wrong reason).
################################################################################

SHADER_TEXT = """
fxconfig fxcfg_default {
  glsl_version = "330";
}
uniform_block ub_gate0 (descriptor_set 0) {
  mat4 MatMVP;
  mat4 MatM;
  vec4 Tint;
}
uniform_block ublk_stereo (descriptor_set 0) {
  mat4 spvr_vp[2];
  mat4 spvr_inv_vp[2];
  vec4 spvr_eyepos[2];
}
vertex_interface vif_gate0 {
  inputs {
    vec4 position : POSITION;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec2 frg_uv0;
  }
}
fragment_interface fif_gate0 : vif_gate0 {
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
vertex_shader vs_gate0_mono : vif_gate0 : ub_gate0 {
  gl_Position = MatMVP * position;
  frg_uv0     = uv0;
}
vertex_shader vs_gate0_stereo : vif_gate0 : ub_gate0 : ublk_stereo {
  gl_Position = (spvr_vp[ofx_viewIndex] * MatM) * position;
  frg_uv0     = uv0;
}
fragment_shader ps_gate0 : fif_gate0 : ub_gate0 {
  vec2 cell     = floor(frg_uv0 * 8.0);
  float checker = mod(cell.x + cell.y, 2.0);
  out_clr       = vec4(Tint.rgb * (0.30 + 0.70 * checker), 1.0);
}
state_block sb_gate0 : default {
  BlendMode = OFF;
  DepthTest = LEQUALS;
  CullTest  = OFF;
}
technique tek_gate0_mono {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_gate0_mono;
    fragment_shader = ps_gate0;
    state_block     = sb_gate0;
  }
}
technique tek_gate0_stereo {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_gate0_stereo;
    fragment_shader = ps_gate0;
    state_block     = sb_gate0;
  }
}
"""

################################################################################
# CHILD: render one full set (two mono references + the 2-layer stereo pair).
################################################################################


def _render(outdir):
  from orkengine import core
  from orkengine import lev2
  from ork.testing import headless_app, ensure_parent_dir
  import numpy
  from PIL import Image

  tokens = core.CrcStringProxy()

  def _camera(eye):
    cam = lev2.CameraData()
    cam.perspective(0.5, 60.0, 45.0)
    cam.lookAt(core.vec3(eye[0], eye[1], eye[2]), core.vec3(0, 0, 0), core.vec3(0, 1, 0))
    return cam

  def _cammtx(cam):
    cm = lev2.CameraMatrices()
    cm.setCustomView(cam.vMatrix())
    cm.setCustomProjection(cam.pMatrix(ASPECT))
    return cm

  def _write_png(capbuf, path):
    arr = numpy.array(capbuf, dtype=numpy.uint8).reshape(capbuf.height, capbuf.width, 4)
    ensure_parent_dir(path)
    Image.fromarray(arr[..., :3]).save(path)
    return path

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx
    print("gate0: supports_multiview=%s max_views=%d" %
          (ctx.supports_multiview, ctx.max_multiview_views), flush=True)

    cams = [_camera(e) for e in EYES]
    vps = [c.vpMatrix(ASPECT) for c in cams]
    cammtx = [_cammtx(c) for c in cams]

    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "spvr_gate0", SHADER_TEXT)
    tek_mono = mtl.technique("tek_gate0_mono")
    tek_stereo = mtl.technique("tek_gate0_stereo")
    assert tek_mono, "technique tek_gate0_mono not found"
    assert tek_stereo, "technique tek_gate0_stereo not found"
    par_mvp = mtl.param("MatMVP")
    par_m = mtl.param("MatM")
    par_tint = mtl.param("Tint")
    identity = core.mtx4()

    def _make_rtg(name, layers):
      rtg = lev2.RtGroup(ctx, W, H)
      rtg.name = name
      if layers > 1:
        # both BEFORE the first createBuffer: the layer count is copied into each
        # RtBuffer at construction.
        rtg.numLayers = layers
        rtg.multiview = True
      rtb = rtg.createBuffer(tokens.RGBA8, tokens.color)
      rtb.clearColor = core.vec4(0, 0, 0, 1)
      rtg.createDepthBuffer(tokens.Z32F, False)
      return rtg

    def _draw_scene(rcfd, mvp, stereo=None):
      """One begin/end block per quad — production draw granularity, and the only
      shape in which a per-draw uniform (Tint) is honored per draw."""
      for (cx, cy, half, z, tint) in QUADS:
        mtl.begin(tek_mono, tek_stereo, rcfd)
        if stereo is not None:
          mtl.publishStereoBlock(rcfd, stereo[0], stereo[1])
        mtl.bindParamMatrix4(par_mvp, mvp)
        mtl.bindParamMatrix4(par_m, identity)
        mtl.bindParamVec4(par_tint, core.vec4(tint[0], tint[1], tint[2], 1.0))
        ctx.DWI.quad2D(core.vec4(cx - half, cy - half, half * 2.0, half * 2.0),
                       core.vec4(0, 0, 1, 1), core.vec4(0, 0, 0, 0), z)
        mtl.end(rcfd)

    rtg_mono = [_make_rtg("Gate0Mono0", 1), _make_rtg("Gate0Mono1", 1)]
    rtg_stereo = _make_rtg("Gate0Stereo", 2)
    print("gate0: stereo rtg viewMask=0x%x numLayers=%d" %
          (rtg_stereo.viewMask, rtg_stereo.numLayers), flush=True)

    caps = {}
    for key in ("mono_ref_0", "mono_ref_1", "layer0", "layer1"):
      caps[key] = lev2.CaptureBuffer()
    caps["layer0"].capture_layer = 0
    caps["layer1"].capture_layer = 1

    ctx.beginFrame()

    # --- mono references: ordinary non-multiview path, no stereo CPD anywhere ---
    for i in (0, 1):
      rcfd_mono = lev2.RenderContextFrameData(ctx)
      ctx.FBI.rtGroupPush(rtg_mono[i])
      ctx.FBI.rtGroupClear(rtg_mono[i])
      _draw_scene(rcfd_mono, vps[i])
      ctx.FBI.rtGroupPop()

    # --- the subject: ONE multiview pass, both views, per-view matrices ---
    rcfd_stereo = lev2.RenderContextFrameData(ctx)
    cimpl = lev2.CompositingImpl(lev2.CompositingData())
    rcfd_stereo.pushCompositor(cimpl)
    cpd = lev2.CompositingPassData()
    cpd.setSinglePassStereo(True)
    cimpl.pushCPD(cpd)
    ctx.FBI.rtGroupPush(rtg_stereo)
    ctx.FBI.rtGroupClear(rtg_stereo)
    # mvp = view 0's: unread by the stereo stage, and the transform NC2's forced
    # mono technique lands on (see the shader note).
    _draw_scene(rcfd_stereo, vps[0], stereo=cammtx)
    ctx.FBI.rtGroupPop()
    cimpl.popCPD()
    rcfd_stereo.popCompositor()

    futures = [
        ctx.FBI.captureAsFormat(rtg_mono[0].buffer(0), caps["mono_ref_0"], "RGBA8"),
        ctx.FBI.captureAsFormat(rtg_mono[1].buffer(0), caps["mono_ref_1"], "RGBA8"),
        ctx.FBI.captureAsFormat(rtg_stereo.buffer(0), caps["layer0"], "RGBA8"),
        ctx.FBI.captureAsFormat(rtg_stereo.buffer(0), caps["layer1"], "RGBA8"),
    ]
    ctx.endFrame()

    for key, fut in zip(("mono_ref_0", "mono_ref_1", "layer0", "layer1"), futures):
      ok = fut.wait(caps[key])
      assert ok, "gate0: capture never landed for %s" % key

    for key in ("mono_ref_0", "mono_ref_1", "layer0", "layer1"):
      path = _write_png(caps[key], os.path.join(outdir, key + ".png"))
      print("gate0: wrote %s" % path, flush=True)

  return 0


################################################################################
# DRIVER: run the children, then the arithmetic.
################################################################################


def _child(tag, outdir, extra_env, log_dir, timeout_s=180):
  """Run this same script in --render mode in a FRESH process. Fresh is not
  optional: both negative-control hooks read their env var ONCE into a function
  static, so arming one in-process after the engine has already read it does
  nothing at all."""
  env = dict(os.environ)
  env.update(extra_env)
  env["ORKID_VULKAN_VALIDATE"] = "2"
  os.makedirs(outdir, exist_ok=True)
  os.makedirs(log_dir, exist_ok=True)
  logpath = os.path.join(log_dir, tag + ".log")
  argv = [sys.executable, os.path.abspath(__file__), "--render", outdir]
  t0 = time.time()
  try:
    proc = subprocess.run(argv, env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, timeout=timeout_s)
    out = proc.stdout.decode("utf-8", "replace")
    rc = proc.returncode
  except subprocess.TimeoutExpired as ex:
    # offscreen device-loss presents as a SILENT HANG (fence results discarded,
    # and the device-lost printf is present-time only) — a stall is a crash-mode
    # result, never a timeout to quietly raise.
    out = (ex.stdout or b"").decode("utf-8", "replace") + "\n<<< GATE0 WALL-CLOCK STALL >>>\n"
    rc = -9
  with open(logpath, "w") as f:
    f.write(out)
  print("gate0: child %-10s rc=%-4d %5.1fs env=%s log=%s" %
        (tag, rc, time.time() - t0, extra_env or "{}", logpath), flush=True)
  return rc, out


def _images(outdir):
  import _ork_vet_common as vet
  out = {}
  for key in ("mono_ref_0", "mono_ref_1", "layer0", "layer1"):
    p = os.path.join(outdir, key + ".png")
    out[key] = vet.luma(vet.load_rgb(p)) if os.path.isfile(p) else None
  return out


def _ssim(a, b):
  import _ork_vet_common as vet
  return float(vet.ssim_map(a, b).mean())


def _diff_frac(a, b):
  import numpy
  return float((numpy.abs(a - b) > DIFF_EPS).mean())


def _alive(a):
  """non-degenerate: has brightness AND has structure."""
  if a is None:
    return False, "missing"
  m = float(a.mean())
  r = float(a.max() - a.min())
  return (m >= MEAN_FLOOR and r >= RANGE_FLOOR), "mean=%.4f range=%.4f" % (m, r)


def main():
  outroot = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
      os.environ.get("TMPDIR", "/tmp"), "spvr_gate0")
  os.makedirs(outroot, exist_ok=True)
  logdir = os.path.join(outroot, "logs")

  from ork.testing import verdict

  fails = []
  notes = []

  def check(label, ok, detail=""):
    print("  %s %-34s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
      fails.append(label)
    return ok

  ############################################################
  # children — every one under the identical binary and env baseline
  ############################################################
  runs = (
      ("smoke", {"ORKID_LOGCHAN_VKIMPL": "1"}),
      ("base", {}),
      ("nc1", {"ORKID_GATE0_VIEWMASK": "1"}),
      ("nc2", {"ORKID_GATE0_FORCE_MONO_TEK": "1"}),
      ("restored", {}),
  )
  results = {}
  for tag, extra in runs:
    rc, out = _child(tag, os.path.join(outroot, tag), extra, logdir)
    results[tag] = (rc, out)

  ############################################################
  # leg (c) + arming, first: an unarmed validator makes the rest meaningless
  ############################################################
  print("=== validation ===", flush=True)
  smoke_out = results["smoke"][1]
  check("validation_armed_banner",
        "ENABLE VALIDATION (CONTINUE MODE)" in smoke_out,
        "(smoke child, ORKID_LOGCHAN_VKIMPL=1)")
  check("validation_layer_enabled",
        "VK_LAYER_KHRONOS_validation found and enabled" in smoke_out)

  for tag, _ in runs:
    out = results[tag][1]
    nerr = out.count("VULKAN ERROR")
    nwarn = out.count("VULKAN WARNING")
    check("validation_clean_%s" % tag, (nerr == 0 and nwarn == 0),
          "errors=%d warnings=%d" % (nerr, nwarn))
    check("child_ran_%s" % tag, results[tag][0] == 0, "rc=%d" % results[tag][0])

  # the multiview pass really was a multiview pass
  check("viewmask_armed_base", "viewMask=0x3" in results["base"][1],
        "(0x3 = both views)")
  check("viewmask_narrowed_nc1", "viewMask=0x1" in results["nc1"][1],
        "(NC1 narrowed the mask)")

  ############################################################
  # POSITIVE legs, on the base run
  ############################################################
  print("=== positive legs (base) ===", flush=True)
  base = _images(os.path.join(outroot, "base"))
  for key in ("mono_ref_0", "mono_ref_1", "layer0", "layer1"):
    ok, det = _alive(base[key])
    check("nondegenerate_%s" % key, ok, det)

  if all(base[k] is not None for k in base):
    d_ab = _diff_frac(base["layer0"], base["layer1"])
    check("leg_a_layers_differ", d_ab > DIFF_FRAC_FLOOR,
          "differing_pixel_frac=%.4f (floor %.2f)" % (d_ab, DIFF_FRAC_FLOOR))
    notes.append("leg_a_diff_frac=%.4f" % d_ab)

    s00 = _ssim(base["layer0"], base["mono_ref_0"])
    s11 = _ssim(base["layer1"], base["mono_ref_1"])
    check("leg_b_layer0_vs_ref0", s00 > SSIM_PASS, "ssim=%.5f (bar %.2f)" % (s00, SSIM_PASS))
    check("leg_b_layer1_vs_ref1", s11 > SSIM_PASS, "ssim=%.5f (bar %.2f)" % (s11, SSIM_PASS))

    s01 = _ssim(base["layer0"], base["mono_ref_1"])
    s10 = _ssim(base["layer1"], base["mono_ref_0"])
    check("leg_b_cross_0x1_must_fail", s01 <= SSIM_CROSS_CEIL, "ssim=%.5f" % s01)
    check("leg_b_cross_1x0_must_fail", s10 <= SSIM_CROSS_CEIL, "ssim=%.5f" % s10)
    notes.append("leg_b_ssim=%.5f/%.5f cross=%.5f/%.5f" % (s00, s11, s01, s10))

    # depth-dependent disparity: the mono references themselves must differ, or the
    # two "views" were never two views and every leg above is comparing one image.
    d_refs = _diff_frac(base["mono_ref_0"], base["mono_ref_1"])
    check("refs_are_two_views", d_refs > DIFF_FRAC_FLOOR, "differing_pixel_frac=%.4f" % d_refs)

  ############################################################
  # NC1 — viewMask narrowed to 0x1: layer1 never rendered
  ############################################################
  print("=== NC1 (ORKID_GATE0_VIEWMASK=1) ===", flush=True)
  nc1 = _images(os.path.join(outroot, "nc1"))
  ok0, det0 = _alive(nc1["layer0"])
  ok1, det1 = _alive(nc1["layer1"])
  check("nc1_layer0_still_rendered", ok0, det0)
  check("nc1_layer1_unwritten_RED", not ok1, det1 + " (clear value expected)")
  if nc1["layer1"] is not None and nc1["mono_ref_1"] is not None:
    s = _ssim(nc1["layer1"], nc1["mono_ref_1"])
    check("nc1_leg_b_layer1_fails", s <= SSIM_PASS, "ssim=%.5f (must NOT pass)" % s)
    notes.append("nc1_layer1_ssim=%.5f" % s)

  ############################################################
  # NC2 — mono technique forced inside the stereo pass: layers collapse
  ############################################################
  print("=== NC2 (ORKID_GATE0_FORCE_MONO_TEK=1) ===", flush=True)
  nc2 = _images(os.path.join(outroot, "nc2"))
  ok0, det0 = _alive(nc2["layer0"])
  ok1, det1 = _alive(nc2["layer1"])
  # both layers must still be REAL images — a blank pair is identical for the wrong
  # reason and would let a broken multiview pass masquerade as a working control.
  check("nc2_layer0_nondegenerate", ok0, det0)
  check("nc2_layer1_nondegenerate", ok1, det1)
  if nc2["layer0"] is not None and nc2["layer1"] is not None:
    d = _diff_frac(nc2["layer0"], nc2["layer1"])
    check("nc2_layers_identical_RED", d == 0.0,
          "differing_pixel_frac=%.6f (leg (a) must go red)" % d)
    notes.append("nc2_diff_frac=%.6f" % d)
    s = _ssim(nc2["layer1"], nc2["mono_ref_0"])
    check("nc2_both_layers_are_view0", s > SSIM_PASS, "ssim(layer1,ref0)=%.5f" % s)

  ############################################################
  # RESTORED — the controls were the controls, not a break
  ############################################################
  print("=== restored ===", flush=True)
  res = _images(os.path.join(outroot, "restored"))
  if all(res[k] is not None for k in res):
    d = _diff_frac(res["layer0"], res["layer1"])
    s0 = _ssim(res["layer0"], res["mono_ref_0"])
    s1 = _ssim(res["layer1"], res["mono_ref_1"])
    check("restored_leg_a_GREEN", d > DIFF_FRAC_FLOOR, "differing_pixel_frac=%.4f" % d)
    check("restored_leg_b_GREEN", s0 > SSIM_PASS and s1 > SSIM_PASS,
          "ssim=%.5f/%.5f" % (s0, s1))
    notes.append("restored_diff_frac=%.4f ssim=%.5f/%.5f" % (d, s0, s1))
  else:
    check("restored_images_present", False, "missing captures")

  ok = (len(fails) == 0)
  detail = "SPVR GATE0 %dx%d | %s" % (W, H, " ".join(notes))
  if fails:
    detail += " | failed=" + ",".join(fails)
  sys.exit(verdict(ok, detail))


if "--render" in sys.argv:
  idx = sys.argv.index("--render")
  sys.exit(_render(sys.argv[idx + 1]))

main()
