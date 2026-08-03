#!/usr/bin/env ork.python
################################################################################
# SPVR — THE PROCEDURAL SKY, THROUGH THE REAL NODE (linux/NV, offscreen).
#
# WHAT THIS ADDS TO test_spvr_sky_stereo. That file proves the shipping peer
# EXISTS, COMPILES and carries the per-view idiom, and proves the IDIOM behaves —
# but it drives a PROBE, because the shipping skybox fragment cannot be reached
# below the compositor (it wants the sky-view LUT, the cookie set and the
# atmosphere block). So FWD_SKYBOX_PROC_ST had never actually been RENDERED by
# the single-pass node: no run had ever printed its name. This file closes that
# gap by building a real scenegraph on SinglePassStereoVrOutputNode with
# sky_source=procedural and reading the answer off the bind-time arbiter.
#
# WHY THE MEASUREMENT NEEDS A PANEL ROLL. The headless VR device separates the
# eyes by IPD and nothing else: identical orientation, identical projection. A
# sky at infinity is then correctly IDENTICAL in both eyes — a translation cannot
# move a ray that is built as (far - near). So on a pure-IPD rig "the eyes differ"
# is not the correct verdict, it is the BUG's verdict (see leg AGREE). The only
# per-eye ASYMMETRY a headless rig can hand the shader is the per-eye display
# panel roll (Device::_stereoTileRotationDegrees{L,R} — a rotated-panel HMD),
# which puts a genuinely different projection in each eye. With it, a per-view sky
# MUST diverge and a one-matrix sky CANNOT.
#
# LEGS (all must pass)
#   (a) ARBITER   the point-of-use [SPVR:SKYSEL] line must name
#                 technique<FWD_SKYBOX_PROC_ST> source<procedural> with BOTH
#                 permu_stereo<1> and pass_stereo<1>. An _ST technique that merely
#                 resolves proves an intent; this line is a DRAW.
#   (b) DIVERGE   with the per-eye panel roll armed, the two eye layers' skies must
#                 DIFFER — each eye unprojected through its OWN spvr_inv_vp.
#   (c) AGREE     with the roll DISARMED (pure IPD translation), the same scene's
#                 skies must AGREE. The opposite-direction bar: without it, leg (b)
#                 is equally satisfied by a ray that picked up the eye POSITION,
#                 which a sky at infinity must never do.
#   (d) CONTROL   the roll-armed rig re-run with the mono technique forced inside
#                 the stereo pass (ORKID_GATE0_FORCE_MONO_TEK — the engine's own
#                 hook, set by this file for its control child only) must COLLAPSE
#                 the layers and must report technique<FWD_SKYBOX_PROC>
#                 permu_stereo<0>. This is the pre-peer behaviour reproduced on
#                 demand: if leg (b)'s number survives here it was never the view
#                 index.
#
# Self-configuring: ORKID_VULKAN_VALIDATE=2 in-code, ibl_crossfade_frames=0 pinned
# on the atmosphere (a half-faded IBL is not a lighting reading). Default
# invocation needs no arguments and no environment.
#
#   ork.python test_spvr_sky_proc_stereo.py [outdir]
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

W = H = 192
IPD = 0.25
ROLL_DEG = 8.0      # per-eye panel roll, equal and opposite
FRAMES = 12         # the procedural LUT chain needs a few frames to publish

STEREO_TEK = "FWD_SKYBOX_PROC_ST"
MONO_TEK = "FWD_SKYBOX_PROC"

# bars, in 0..1 luma. Same metric as test_spvr_sky_stereo: one 8-bit code either
# way is not a difference.
DIFF_EPS = 2.0 / 255.0
DIFF_FRAC_FLOOR = 0.02      # leg (b)
AGREE_FRAC_CEIL = 0.01      # leg (c)
COLLAPSE_FRAC_CEIL = 0.002  # leg (d)
RANGE_FLOOR = 0.05          # a flat sky matches any other flat sky

ARBITER = re.compile(
    r"\[SPVR:SKYSEL\] skybox DRAW technique<([^>]*)> source<([^>]*)> "
    r"permu_stereo<(\d)> pass_stereo<(\d)>")


################################################################################
# CHILD: the real node, the real procedural sky, two takes.
################################################################################


def _render(outdir):
  from orkengine import core
  from orkengine import lev2
  from orkengine.core import vec3, vec4, mtx4, VarMap
  from ork.testing import headless_app, ensure_parent_dir
  import numpy
  from PIL import Image

  results = {}

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2'], width=W, height=H) as app:
    ctx = app.ctx
    results["multiview"] = bool(ctx.supports_multiview)
    results["max_views"] = int(ctx.max_multiview_views)
    print("sky_proc: supports_multiview=%s max_views=%d"
          % (ctx.supports_multiview, ctx.max_multiview_views), flush=True)
    if not ctx.supports_multiview:
      with open(os.path.join(outdir, "child.json"), "w") as f:
        json.dump(results, f, indent=1)
      return 0

    vrdev = lev2.orkidvr.novr_device()
    vrdev.width = W
    vrdev.height = H
    vrdev.FOVD = 90.0
    vrdev.IPD = IPD
    vrdev.near = 0.1
    vrdev.far = 10000.0
    # looking slightly ABOVE the horizon: the sky-view LUT's gradient and the sun
    #  disc both land in frame, so the two eyes have real structure to disagree on.
    vrdev.setPoseMatrix("hmd", mtx4.lookAt(vec3(0, 0, 0), vec3(0, 0.35, -1), vec3(0, 1, 0)))

    params = VarMap()
    params.preset = "FWDPBRSPVR"
    params.SkyboxIntensity = 1.0
    params.SpecularIntensity = 1.0
    params.DiffuseIntensity = 1.0
    params.AmbientLight = vec3(0.1)
    params.DepthFogDistance = float(1e6)
    # a REAL baked environment behind the procedural one: through the warm-up
    #  window the IBL still reads the baked maps, and the forward state lambda
    #  dereferences them unconditionally.
    params.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"
    scene = lev2.scenegraph.Scene(params)
    layer = scene.createLayer("std_forward")
    scene.createLayer("depth_prepass")

    pbc = scene.pbr_common
    pbc.enable_skybox = True
    atmo = lev2.SkyAtmosphereData()
    atmo.ibl_crossfade_frames = 0   # a half-faded IBL is not a lighting reading
    pbc.atmosphere = atmo
    pbc.sky_source = "procedural"
    results["sky_source"] = str(pbc.sky_source)

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 5.0
    sun.lookAt(vec3(-30, 20, 5), vec3(0, 0, 0), vec3(0, 1, 0))
    _sun_node = layer.createLightNode("sun", sun)
    scene.lightingmanager.gpuInit(ctx)

    camlut = lev2.CameraDataLut()
    cam = lev2.CameraData()
    cam.perspective(0.1, 10000.0, 45.0)
    cam.lookAt(vec3(0, 0, 0), vec3(0, 0.35, -1), vec3(0, 1, 0))
    camlut.addCamera("spawncam", cam)

    def take(tag, roll):
      vrdev.stereo_tile_rotation_degrees_l = +roll
      vrdev.stereo_tile_rotation_degrees_r = -roll
      for _ in range(FRAMES):
        scene.updateScene(camlut)
        ctx.beginFrame()
        scene.renderOnContext(ctx)
        ctx.endFrame()
      # the eye buffers are read back in a FRESH frame: capturing inside the frame
      #  that produced them leaves them in a host-read layout the composite asserts on.
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
        assert ok, "sky_proc: capture never landed for %s %s" % (tag, nm)
        cb = caps[nm]
        arr = numpy.array(cb, dtype=numpy.uint8).reshape(cb.height, cb.width, 4)
        path = os.path.join(outdir, "%s_%s.png" % (tag, nm))
        ensure_parent_dir(path)
        Image.fromarray(arr[..., :3]).save(path)
      print("sky_proc: captured %s (roll %.1f deg)" % (tag, roll), flush=True)

    take("roll", ROLL_DEG)
    take("noroll", 0.0)

    results["validation_armed"] = bool(ctx.validation_armed)
    results["validation_errors"] = int(ctx.validation_errors)

  with open(os.path.join(outdir, "child.json"), "w") as f:
    json.dump(results, f, indent=1)
  return 0


################################################################################
# DRIVER
################################################################################


def _luma(path):
  import numpy
  from PIL import Image
  a = numpy.asarray(Image.open(path).convert("RGB"), dtype=numpy.float64) / 255.0
  return 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]


def _metrics(dirname, tag):
  import numpy
  L = _luma(os.path.join(dirname, "%s_L.png" % tag))
  R = _luma(os.path.join(dirname, "%s_R.png" % tag))
  d = numpy.abs(L - R)
  rng = float(min(L.max() - L.min(), R.max() - R.min()))
  return float(d.mean()), float((d > DIFF_EPS).mean()), rng


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
      tempfile.gettempdir(), "spvr_sky_proc_stereo")
  os.makedirs(outdir, exist_ok=True)
  t0 = time.time()
  fails = []

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
    print("test_spvr_sky_proc_stereo: SKIP in %.1fs" % (time.time() - t0), flush=True)
    return 0
  if rec_st.get("sky_source") != "procedural":
    fails.append("the scene reports sky_source=%s -- this file measures the PROCEDURAL arm"
                 % rec_st.get("sky_source"))

  rc_mo, out_mo, _rec_mo = _run_child(dir_mo, force_mono=True)
  if rc_mo != 0:
    print(out_mo, flush=True)
    fails.append("mono-control child exited rc=%d -- leg (d) has no control" % rc_mo)

  # ---- leg (a): ARBITER
  st_seen = {m.group(1): (m.group(2), int(m.group(3)), int(m.group(4)))
             for m in ARBITER.finditer(out_st)}
  print("LEG_ARBITER techniques=%s" % sorted(st_seen.keys()), flush=True)
  got = st_seen.get(STEREO_TEK)
  print("LEG_ARBITER %-22s %s" % (STEREO_TEK, "PASS" if got else "FAIL"), flush=True)
  if not got:
    fails.append("ARBITER: no skybox DRAW took %s -- the procedural sky rendered through "
                 "%s and both eye layers got one unprojection matrix"
                 % (STEREO_TEK, ", ".join(sorted(st_seen)) or "no announced technique"))
  else:
    source, permu_st, pass_st = got
    if source != "procedural":
      fails.append("ARBITER: %s announced source<%s>, expected procedural" % (STEREO_TEK, source))
    if permu_st != 1 or pass_st != 1:
      fails.append("ARBITER: %s bound with permu_stereo<%d> pass_stereo<%d> -- both must be 1"
                   % (STEREO_TEK, permu_st, pass_st))
  if MONO_TEK in st_seen:
    fails.append("ARBITER: a skybox DRAW took the MONO technique %s inside the stereo pass"
                 % MONO_TEK)

  # ---- legs (b)(c): the two directions
  try:
    d_mean, d_frac, d_rng = _metrics(dir_st, "roll")
    a_mean, a_frac, a_rng = _metrics(dir_st, "noroll")
  except Exception as ex:
    print("TESTVERDICT FAIL: captures unreadable (%s)" % ex, flush=True)
    return 1
  print("LEG_DIVERGE roll=%.1fdeg mean=%.6f frac=%.5f range=%.4f"
        % (ROLL_DEG, d_mean, d_frac, d_rng), flush=True)
  print("LEG_AGREE   roll=0      mean=%.6f frac=%.5f range=%.4f"
        % (a_mean, a_frac, a_rng), flush=True)
  if min(d_rng, a_rng) < RANGE_FLOOR:
    fails.append("the sky captures are degenerate (range %.4f/%.4f < %.4f) -- a flat sky "
                 "matches any other flat sky" % (d_rng, a_rng, RANGE_FLOOR))
  if d_frac < DIFF_FRAC_FLOOR:
    fails.append("DIVERGE: the two eye layers carry the SAME sky under per-eye panel roll "
                 "(frac %.5f < %.5f) -- one unprojection matrix served both views"
                 % (d_frac, DIFF_FRAC_FLOOR))
  if a_frac > AGREE_FRAC_CEIL:
    fails.append("AGREE: the sky DIFFERS between the eyes under a pure eye TRANSLATION "
                 "(frac %.5f > %.5f) -- the view ray is picking up the eye position; a sky "
                 "at infinity must not" % (a_frac, AGREE_FRAC_CEIL))

  # ---- leg (d): MONO CONTROL
  if rc_mo == 0:
    mo_seen = {m.group(1): (m.group(2), int(m.group(3)), int(m.group(4)))
               for m in ARBITER.finditer(out_mo)}
    try:
      c_mean, c_frac, _c_rng = _metrics(dir_mo, "roll")
    except Exception as ex:
      c_mean, c_frac = float("nan"), float("nan")
      fails.append("mono-control captures unreadable (%s)" % ex)
    print("LEG_CONTROL techniques=%s mean=%.6f frac=%.5f"
          % (sorted(mo_seen.keys()), c_mean, c_frac), flush=True)
    if MONO_TEK not in mo_seen:
      fails.append("CONTROL: the forced-mono child did not announce %s -- the control never "
                   "armed, so leg (b) has no negative" % MONO_TEK)
    elif mo_seen[MONO_TEK][1] != 0:
      fails.append("CONTROL: %s announced permu_stereo<%d>, expected 0"
                   % (MONO_TEK, mo_seen[MONO_TEK][1]))
    if c_frac == c_frac and c_frac > COLLAPSE_FRAC_CEIL:
      fails.append("CONTROL: the forced-mono sky still differs between eyes (frac %.5f > "
                   "%.5f) -- leg (b)'s divergence is not the view index"
                   % (c_frac, COLLAPSE_FRAC_CEIL))

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
    print("test_spvr_sky_proc_stereo: FAIL in %.1fs" % dt, flush=True)
    return 1
  print("TESTVERDICT PASS -- %s was DRAWN by the single-pass node against a procedural sky; "
        "the eye layers diverge under a per-eye panel roll, agree under a pure eye "
        "translation, and collapse when the mono technique is forced (%.1fs)"
        % (STEREO_TEK, dt), flush=True)
  return 0


if __name__ == "__main__":
  if len(sys.argv) > 2 and sys.argv[1] == "--render":
    sys.exit(_render(sys.argv[2]))
  sys.exit(main())
