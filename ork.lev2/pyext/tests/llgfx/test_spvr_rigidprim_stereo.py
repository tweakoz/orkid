#!/usr/bin/env ork.python
################################################################################
# SPVR — RIGID PRIMITIVES UNDER SINGLE-PASS STEREO (linux/NV, offscreen).
#
# THE DEFECT THIS CLOSES. Both RigidPrimitive draw callbacks build their
# FxPipelinePermutation BY HAND rather than through FxPipelineCache::findPipeline
# (RCID), and both PINNED permu._stereo = false. Under two-pass stereo that pin is
# invisible — each eye is a real mono pass — but under the single-pass node one
# draw carries both views, so a pinned-mono permutation writes THE SAME IMAGE INTO
# BOTH EYE LAYERS. Zero parallax, no validation error, no crash, and a still frame
# from either eye looks perfect. Two sites, same defect:
#   installInCallbackDrawable(drw, material)   the plain rigid-primitive path
#   InstancedRigidPrimitiveDrawable::enqueue.. the instanced twin
# Fixing one and leaving the other is the half-fix, so BOTH are measured here.
#
# WHY A NUMBER IS THE ONLY EVIDENCE. "It rendered" and "validation was clean" are
# both true of the broken build. The eye layers have to be DIFFERENCED, and the
# difference has to be shown to be the VIEW INDEX rather than scene motion, async
# jitter or readback noise — which is what the mono control below is for.
#
# LEGS (all must pass)
#   (a) SELECTION   the point-of-use arbiter [SPVR:FWDSEL] must name the _ST
#                   technique for BOTH paths, with permu_stereo<1> AND
#                   pass_stereo<1>. An _ST technique that merely EXISTS proves
#                   nothing; this is the line that fires at pipeline BIND.
#   (b) PARALLAX    per-eye layer difference above a floor, per path.
#   (c) MONO CONTROL the SAME rig with the mono technique forced inside the stereo
#                   pass (ORKID_GATE0_FORCE_MONO_TEK, the engine's own hook, set by
#                   this file for its control child only) must COLLAPSE the layers.
#                   This IS the pinned-permutation defect reproduced on demand: if
#                   leg (b)'s numbers survive here, they were never the view index.
#   (d) SOURCE PIN  neither callback may carry a literal `permu._stereo = false`
#                   again. The pin is a one-token regression with no runtime
#                   signature of its own, so it is pinned in the source too.
#
# The rig is deliberately NEAR-AXIS (a cube cluster inside ~20 degrees of the view
# axis at 2m): eye separation is measured as a straight image difference, so no
# projection approximation enters the number, and off-axis content would add none
# of the signal while widening the spread.
#
# Self-configuring: ORKID_VULKAN_VALIDATE=2 (continue mode — =1 traps with no
# printed evidence). Default invocation needs no arguments and no environment; the
# switches this file owns are stripped from the inherited environment so a value
# left in the caller's shell cannot decide a leg.
#
#   ork.python test_spvr_rigidprim_stereo.py [outdir]
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")

import sys
import re
import json
import time
import tempfile
import subprocess

sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "bin"))
sys.path.insert(0, os.path.join(_ROOT, "ork.lev2", "examples", "python"))

RIGID_PRIM_INL = os.path.join(_ROOT, "ork.lev2", "inc", "ork", "lev2", "gfx",
                              "meshutil", "rigid_primitive.inl")

W = H = 192
IPD = 0.25          # exaggerated but physical; parallax scales linearly with it
EYE_Z = 2.0         # head distance to the cube cluster
FRAMES = 10         # enough for the IBL + downsample chain to reach steady state

# the two paths, and the technique each must resolve to under stereo.
PATHS = (
    ("plain", "FWD_CV_NM_RI_NI_ST", "RigidPrimitive::installInCallbackDrawable"),
    ("instanced", "FWD_CV_NM_RI_IN_ST", "InstancedRigidPrimitiveDrawable::enqueueToRenderQueue"),
)

# bars, in 0..1 luma. DIFF_EPS/DIFF_FRAC_FLOOR are the sky test's metric verbatim —
# one 8-bit code either way is not a difference, and 2% of the frame must move.
DIFF_EPS = 2.0 / 255.0
DIFF_FRAC_FLOOR = 0.02      # leg (b): the eye layers must genuinely differ
COLLAPSE_FRAC_CEIL = 0.002  # leg (c): the mono control must be flat (measures 0.0)
RANGE_FLOOR = 0.05          # a flat capture matches any other flat capture

ARBITER = re.compile(
    r"\[SPVR:FWDSEL\] forward DRAW material<([^>]*)> technique<([^>]*)> "
    r"permu_stereo<(\d)> pass_stereo<(\d)>")


################################################################################
# CHILD: build the rig under the SPVR node, render, capture both eye layers.
################################################################################


def _render(outdir):
  from orkengine import core
  from orkengine import lev2
  from orkengine.core import vec3, vec4, dvec3, mtx4, quat, VarMap
  from ork.testing import headless_app, ensure_parent_dir
  from lev2utils.shaders import createPbrMaterialWithColor
  import numpy
  from PIL import Image

  results = {}

  def cube_submesh(ex):
    sm = lev2.meshutil.SubMesh()
    p = [dvec3(-ex, -ex, -ex), dvec3(ex, -ex, -ex), dvec3(ex, ex, -ex), dvec3(-ex, ex, -ex),
         dvec3(-ex, -ex, ex), dvec3(ex, -ex, ex), dvec3(ex, ex, ex), dvec3(-ex, ex, ex)]
    sm.addQuad(p[4], p[5], p[6], p[7])
    sm.addQuad(p[1], p[0], p[3], p[2])
    sm.addQuad(p[0], p[4], p[7], p[3])
    sm.addQuad(p[5], p[1], p[2], p[6])
    sm.addQuad(p[3], p[7], p[6], p[2])
    sm.addQuad(p[0], p[1], p[5], p[4])
    return sm.triangulated()

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2'], width=W, height=H) as app:
    ctx = app.ctx
    results["multiview"] = bool(ctx.supports_multiview)
    results["max_views"] = int(ctx.max_multiview_views)
    print("rigidprim: supports_multiview=%s max_views=%d"
          % (ctx.supports_multiview, ctx.max_multiview_views), flush=True)
    if not ctx.supports_multiview:
      with open(os.path.join(outdir, "child.json"), "w") as f:
        json.dump(results, f, indent=1)
      return 0

    # NoVR device with a nonzero IPD: the two eyes are one static pose, IPD
    #  separated — identical orientation and projection, so the ONLY thing that can
    #  differ between the eye layers is the per-view matrix the _ST vertex stage reads.
    vrdev = lev2.orkidvr.novr_device()
    vrdev.width = W
    vrdev.height = H
    vrdev.FOVD = 90.0
    vrdev.IPD = IPD
    vrdev.near = 0.1
    vrdev.far = 1000.0
    vrdev.setPoseMatrix("hmd", mtx4.lookAt(vec3(0, 0, EYE_Z), vec3(0, 0, 0), vec3(0, 1, 0)))

    camlut = lev2.CameraDataLut()
    cam = lev2.CameraData()
    cam.perspective(0.1, 1000.0, 45.0)
    cam.lookAt(vec3(0, 0, EYE_Z), vec3(0, 0, 0), vec3(0, 1, 0))
    # the per-view cull fan-out resolves its camera from "spawncam" outside XR
    #  presentation; without it the scene renders but no cull ever runs.
    camlut.addCamera("spawncam", cam)

    def build(kind):
      params = VarMap()
      params.preset = "FWDPBRSPVR"
      params.SkyboxIntensity = 1.0
      params.SpecularIntensity = 1.0
      params.DiffuseIntensity = 1.0
      params.AmbientLight = vec3(0.3)
      params.DepthFogDistance = float(1e6)
      # a REAL baked environment: the forward PBR state lambda dereferences the
      #  radiance maps unconditionally, so a scene whose envmap failed to load
      #  segfaults before it can render anything worth measuring.
      params.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"
      scene = lev2.scenegraph.Scene(params)
      layer = scene.createLayer("std_forward")
      scene.createLayer("depth_prepass")
      mtl = createPbrMaterialWithColor(
          ctx=ctx, color=vec4(1, 1, 1, 1), metallic=0.0, roughness=1.0)
      prim = lev2.RigidPrimitive(cube_submesh(0.35), ctx)
      if kind == "plain":
        node = prim.createNode("cube", layer, mtl)
        node.worldTransform.translation = vec3(0, 0, 0)
        node.worldTransform.scale = 2.0
      else:
        node = prim.createInstancedNode(9, "cubes", layer, mtl, cull=False)
        i = 0
        for dx in (-0.75, 0.0, 0.75):
          for dy in (-0.75, 0.0, 0.75):
            node.setInstanceMatrix(i, mtx4.composed(vec3(dx, dy, 0.0), quat(), 1.0))
            i += 1
      scene.lightingmanager.gpuInit(ctx)
      return scene, (prim, mtl, node)

    for (kind, _tek, _site) in PATHS:
      scene, keep = build(kind)
      for _ in range(FRAMES):
        scene.updateScene(camlut)
        ctx.beginFrame()
        scene.renderOnContext(ctx)
        ctx.endFrame()
      # the eye buffers are read back in a FRESH frame: capturing inside the frame
      #  that produced them leaves them in a host-read layout the composite then
      #  asserts on (the DMVR gate's finding, same accessor, same shape).
      outnode = scene.compositoroutputnode
      ctx.beginFrame()
      caps = {}
      futs = []
      for (nm, left) in (("L", True), ("R", False)):
        rtg = outnode.downsampledEyeRtGroup(left)
        cb = lev2.CaptureBuffer()
        caps[nm] = cb
        futs.append((nm, ctx.FBI.captureAsFormat(rtg.buffer(0), cb, "RGBA8")))
      ctx.endFrame()
      for (nm, fut) in futs:
        ok = fut.wait(caps[nm])
        assert ok, "rigidprim: capture never landed for %s %s" % (kind, nm)
        cb = caps[nm]
        arr = numpy.array(cb, dtype=numpy.uint8).reshape(cb.height, cb.width, 4)
        path = os.path.join(outdir, "%s_%s.png" % (kind, nm))
        ensure_parent_dir(path)
        Image.fromarray(arr[..., :3]).save(path)
      print("rigidprim: captured %s" % kind, flush=True)
      del scene, keep

    results["validation_armed"] = bool(ctx.validation_armed)
    results["validation_errors"] = int(ctx.validation_errors)

  with open(os.path.join(outdir, "child.json"), "w") as f:
    json.dump(results, f, indent=1)
  return 0


################################################################################
# LEG (d) -- SOURCE PIN. Pure text; no GPU, no engine.
################################################################################


def leg_source_pin():
  fails = []
  if not os.path.exists(RIGID_PRIM_INL):
    return ["rigid_primitive.inl not found at %s" % RIGID_PRIM_INL]
  src = open(RIGID_PRIM_INL).read()
  n_pin = len(re.findall(r"permu\._stereo\s*=\s*false", src))
  n_read = len(re.findall(r"permu\._stereo\s*=\s*[^;]*isSinglePassStereo", src))
  print("LEG_SOURCE_PIN pinned=%d cpd_reads=%d" % (n_pin, n_read), flush=True)
  if n_pin:
    fails.append("rigid_primitive.inl PINS permu._stereo = false in %d place(s) -- content on "
                 "that path takes the mono technique inside a stereo pass and both eye layers "
                 "receive the same image" % n_pin)
  if n_read != len(PATHS):
    fails.append("rigid_primitive.inl reads the CPD stereo bit in %d place(s), expected %d "
                 "(one per hand-built permutation)" % (n_read, len(PATHS)))
  return fails


################################################################################
# DRIVER
################################################################################


def _luma(path):
  import numpy
  from PIL import Image
  a = numpy.asarray(Image.open(path).convert("RGB"), dtype=numpy.float64) / 255.0
  return 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]


def _metrics(a, b):
  import numpy
  d = numpy.abs(a - b)
  return float(d.mean()), float((d > DIFF_EPS).mean())


def _run_child(outdir, force_mono):
  os.makedirs(outdir, exist_ok=True)
  env = dict(os.environ)
  env["ORKID_VULKAN_VALIDATE"] = "2"
  env.pop("ORKID_GATE0_FORCE_MONO_TEK", None)
  env.pop("ORKID_SPVR_NO_MULTIVIEW", None)
  env.pop("ORKID_FORCE_DMVR", None)
  if force_mono:
    env["ORKID_GATE0_FORCE_MONO_TEK"] = "1"
  argv = [sys.executable, os.path.abspath(__file__), "--render", outdir]
  proc = subprocess.run(argv, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=600)
  out = proc.stdout.decode("utf-8", "replace")
  rec = {}
  cjson = os.path.join(outdir, "child.json")
  if os.path.exists(cjson):
    rec = json.load(open(cjson))
  return proc.returncode, out, rec


def main():
  outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
      tempfile.gettempdir(), "spvr_rigidprim_stereo")
  os.makedirs(outdir, exist_ok=True)
  t0 = time.time()
  fails = []

  fails += leg_source_pin()

  dir_st = os.path.join(outdir, "st")
  dir_mo = os.path.join(outdir, "mono")

  rc, out_st, rec_st = _run_child(dir_st, force_mono=False)
  print(out_st, flush=True)
  if rc != 0:
    print("TESTVERDICT FAIL: stereo child exited rc=%d (a missing "
          "<ork_envmaps2>/cold4k.xir would abort it before any capture)" % rc, flush=True)
    return 1
  if not rec_st.get("multiview"):
    print("SKIP: device reports multiview=%s max_views=%s -- single-pass stereo has no "
          "meaningful answer here" % (rec_st.get("multiview"), rec_st.get("max_views")),
          flush=True)
    print("test_spvr_rigidprim_stereo: SKIP in %.1fs" % (time.time() - t0), flush=True)
    return 0

  rc_mo, out_mo, rec_mo = _run_child(dir_mo, force_mono=True)
  if rc_mo != 0:
    print(out_mo, flush=True)
    fails.append("mono-control child exited rc=%d -- leg (c) has no control" % rc_mo)

  # ---- leg (a): SELECTION, from the bind-time arbiter
  seen = {}
  for m in ARBITER.finditer(out_st):
    seen[m.group(2)] = (m.group(1), int(m.group(3)), int(m.group(4)))
  print("LEG_SELECTION techniques=%s" % sorted(seen.keys()), flush=True)
  for (kind, tek, site) in PATHS:
    got = seen.get(tek)
    print("LEG_SELECTION %-10s %-22s %s" % (kind, tek, "PASS" if got else "FAIL"), flush=True)
    if not got:
      fails.append("SELECTION %s: no [SPVR:FWDSEL] draw took %s -- %s did not reach its "
                   "stereo technique" % (kind, tek, site))
    else:
      _mtl, permu_st, pass_st = got
      if permu_st != 1 or pass_st != 1:
        fails.append("SELECTION %s: %s bound with permu_stereo<%d> pass_stereo<%d> -- both "
                     "must be 1" % (kind, tek, permu_st, pass_st))
  for tek in sorted(seen):
    if tek.endswith("_MO") or "_MO_" in tek:
      fails.append("SELECTION: a draw took the MONO technique %s inside the stereo pass -- "
                   "that draw writes one image into both eye layers" % tek)

  # ---- legs (b) + (c): the numbers
  have_mono = (rc_mo == 0)
  for (kind, _tek, site) in PATHS:
    try:
      st_L = _luma(os.path.join(dir_st, "%s_L.png" % kind))
      st_R = _luma(os.path.join(dir_st, "%s_R.png" % kind))
    except Exception as ex:
      fails.append("captures unreadable for %s (%s)" % (kind, ex))
      continue
    rng = float(min(st_L.max() - st_L.min(), st_R.max() - st_R.min()))
    s_mean, s_frac = _metrics(st_L, st_R)
    m_mean, m_frac = (float("nan"), float("nan"))
    if have_mono:
      try:
        mo_L = _luma(os.path.join(dir_mo, "%s_L.png" % kind))
        mo_R = _luma(os.path.join(dir_mo, "%s_R.png" % kind))
        m_mean, m_frac = _metrics(mo_L, mo_R)
      except Exception as ex:
        fails.append("mono-control captures unreadable for %s (%s)" % (kind, ex))
        have_mono = False
    print("LEG_PARALLAX %-10s stereo mean=%.6f frac=%.5f range=%.4f | mono-control "
          "mean=%.6f frac=%.5f" % (kind, s_mean, s_frac, rng, m_mean, m_frac), flush=True)
    if rng < RANGE_FLOOR:
      fails.append("%s: the eye captures are degenerate (range %.4f < %.4f)"
                   % (kind, rng, RANGE_FLOOR))
    if s_frac < DIFF_FRAC_FLOOR:
      fails.append("PARALLAX %s: the two eye layers carry the SAME image (frac %.5f < %.5f) "
                   "-- %s renders mono into both eyes" % (kind, s_frac, DIFF_FRAC_FLOOR, site))
    if have_mono and m_frac > COLLAPSE_FRAC_CEIL:
      fails.append("MONO CONTROL %s: the forced-mono rig still differs between eyes "
                   "(frac %.5f > %.5f) -- leg (b)'s difference is not the view index"
                   % (kind, m_frac, COLLAPSE_FRAC_CEIL))

  # ---- validation (a warning is a failure)
  armed = rec_st.get("validation_armed", False)
  verrs = int(rec_st.get("validation_errors", -1))
  print("VALIDATION armed=%s errors=%d" % (armed, verrs), flush=True)
  if not armed:
    fails.append("validation layer was not armed -- a zero error count proves nothing")
  elif verrs != 0:
    fails.append("validation reported %d error(s)" % verrs)

  dt = time.time() - t0
  if fails:
    print("TESTVERDICT FAIL (%d): %s" % (len(fails), "; ".join(fails[:6])), flush=True)
    print("test_spvr_rigidprim_stereo: FAIL in %.1fs" % dt, flush=True)
    return 1
  print("TESTVERDICT PASS -- both rigid-primitive draw paths BIND their _ST technique under "
        "the single-pass node and render parallax-positive eye layers, with the forced-mono "
        "control collapsing them (%.1fs)" % dt, flush=True)
  return 0


if __name__ == "__main__":
  if len(sys.argv) > 2 and sys.argv[1] == "--render":
    sys.exit(_render(sys.argv[2]))
  sys.exit(main())
