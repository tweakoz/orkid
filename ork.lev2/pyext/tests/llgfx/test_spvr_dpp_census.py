#!/usr/bin/env ork.python
################################################################################
# SPVR — DEPTH-PREPASS TECHNIQUE CENSUS ([SPVR:DPPSEL]) (linux/NV, offscreen).
#
# THE BLIND SPOT THIS CLOSES. The forward COLOR creator announces every generated
# arm it binds ([SPVR:GENSEL]) and ComputeDrawable announces its draws
# ([SPVR:CDSEL]) — so terrain and hypermesh were the only families a transcript
# could see inside the depth prepass. PBRMaterial::_createFxPipelineDPP announced
# NOTHING, which made RIGID / SKINNED / PLAIN-INSTANCED / IMPOSTOR content in the
# prepass indistinguishable from content that was never drawn. A mono technique
# bound inside a stereo prepass writes ONE eye's depth into BOTH layers and the
# other eye then fails LEQUALS across the whole surface — the defect is silent,
# and a silent defect needs a line that names it.
#
# WHAT IS ACTUALLY MEASURED. Not "the string exists": the census line is emitted
# from a BIND-time state lambda, so a line only appears if a real draw took that
# pipeline, and its arm<> verdict is derived from the technique name against the
# LIVE CPD stereo bit.
#
# LEGS (all must pass)
#   (a) PRESENT     under the single-pass node, a rigid drawable registered in the
#                   depth_prepass layer must produce a [SPVR:DPPSEL] line with
#                   pass<depth_prepass> pass_stereo<1> arm<per-view>, on an _ST
#                   technique. This is the positive direction.
#   (b) FALLBACK    the SAME rig with the mono technique forced inside the stereo
#                   pass (ORKID_GATE0_FORCE_MONO_TEK, the engine's own hook, set by
#                   this file for its control child only) must produce a
#                   [SPVR:DPPSEL] line reading arm<MONO-FALLBACK> at
#                   pass_stereo<1>. A census that has never printed MONO-FALLBACK
#                   on a deliberately broken run proves nothing.
#
#                   Leg (b) is NOT redundant with leg (a): the same mono prepass
#                   pipeline is also bound by the mono shadow/cascade passes, which
#                   run FIRST, so a once-per-pipeline announce throttle spends its
#                   only line on arm<mono pass> and the fallback never prints. That
#                   is precisely the regression this leg pins.
#
# Self-configuring: no arguments, no environment. The switches this file owns are
# stripped from the inherited environment so a value left in the caller's shell
# cannot decide a leg.
#
#   ork.python test_spvr_dpp_census.py [outdir]
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

W = H = 192
IPD = 0.25
EYE_Z = 2.0
FRAMES = 10

CENSUS = re.compile(
    r"\[SPVR:DPPSEL\] prepass DRAW family<([^>]*)> material<([^>]*)> technique<([^>]*)> "
    r"pass<([^>]*)> permu_stereo<(\d)> pass_stereo<(\d)> arm<([^>]*)>")


################################################################################
# CHILD: one rigid drawable, in the color layer AND the depth_prepass layer,
# rendered under the single-pass stereo node. The census is stdout.
################################################################################


def _render(outdir):
  from orkengine import core
  from orkengine import lev2
  from orkengine.core import vec3, vec4, dvec3, mtx4, VarMap
  from ork.testing import headless_app
  from lev2utils.shaders import createPbrMaterialWithColor

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
    print("dpp_census: supports_multiview=%s max_views=%d"
          % (ctx.supports_multiview, ctx.max_multiview_views), flush=True)
    if not ctx.supports_multiview:
      with open(os.path.join(outdir, "child.json"), "w") as f:
        json.dump(results, f, indent=1)
      return 0

    # NoVR device with a nonzero IPD: two views, one static pose. Nothing here
    #  depends on the parallax NUMBER — the census line is the observable — but a
    #  real two-view device is what makes the pass stereo in the first place.
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
    camlut.addCamera("spawncam", cam)

    params = VarMap()
    params.preset = "FWDPBRSPVR"
    params.SkyboxIntensity = 1.0
    params.SpecularIntensity = 1.0
    params.DiffuseIntensity = 1.0
    params.AmbientLight = vec3(0.3)
    params.DepthFogDistance = float(1e6)
    params.DepthPrepass = True
    # a REAL baked environment: the forward PBR state lambda dereferences the
    #  radiance maps unconditionally, so a scene whose envmap failed to load
    #  segfaults before it can render anything worth measuring.
    params.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"
    scene = lev2.scenegraph.Scene(params)
    layer = scene.createLayer("std_forward")
    dpp_layer = scene.createLayer("depth_prepass")
    mtl = createPbrMaterialWithColor(
        ctx=ctx, color=vec4(1, 1, 1, 1), metallic=0.0, roughness=1.0)
    prim = lev2.RigidPrimitive(cube_submesh(0.35), ctx)
    node = prim.createNode("cube", layer, mtl)
    node.worldTransform.translation = vec3(0, 0, 0)
    node.worldTransform.scale = 2.0
    # the raw lev2 scenegraph has no ECS auto-dpp wiring (SceneGraphSystem's
    #  _skipAutoDepthPrepass path); the prepass draws the depth_prepass LAYER, so
    #  a drawable that is not registered there is simply absent from the prepass.
    dpp_layer.addDrawableNode(node)
    scene.lightingmanager.gpuInit(ctx)

    for _ in range(FRAMES):
      scene.updateScene(camlut)
      ctx.beginFrame()
      scene.renderOnContext(ctx)
      ctx.endFrame()

    results["validation_armed"] = bool(ctx.validation_armed)
    results["validation_errors"] = int(ctx.validation_errors)
    print("dpp_census: rendered %d frames" % FRAMES, flush=True)
    del scene, prim, mtl, node

  with open(os.path.join(outdir, "child.json"), "w") as f:
    json.dump(results, f, indent=1)
  return 0


################################################################################
# DRIVER
################################################################################


def _run_child(outdir, force_mono):
  os.makedirs(outdir, exist_ok=True)
  env = dict(os.environ)
  env["ORKID_VULKAN_VALIDATE"] = "2"
  env.pop("ORKID_GATE0_FORCE_MONO_TEK", None)
  env.pop("ORKID_SPVR_NO_MULTIVIEW", None)
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


def _census(out):
  rows = []
  for m in CENSUS.finditer(out):
    rows.append({
        "family": m.group(1),
        "material": m.group(2),
        "technique": m.group(3),
        "pass": m.group(4),
        "permu_stereo": int(m.group(5)),
        "pass_stereo": int(m.group(6)),
        "arm": m.group(7),
    })
  return rows


def main():
  outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
      tempfile.gettempdir(), "spvr_dpp_census")
  os.makedirs(outdir, exist_ok=True)
  t0 = time.time()
  fails = []

  rc, out_st, rec_st = _run_child(os.path.join(outdir, "st"), force_mono=False)
  if rc != 0:
    print(out_st, flush=True)
    print("TESTVERDICT FAIL: stereo child exited rc=%d (a missing "
          "<ork_envmaps2>/cold4k.xir would abort it before any draw)" % rc, flush=True)
    return 1
  if not rec_st.get("multiview"):
    print("SKIP: device reports multiview=%s max_views=%s -- single-pass stereo has no "
          "meaningful answer here" % (rec_st.get("multiview"), rec_st.get("max_views")),
          flush=True)
    print("test_spvr_dpp_census: SKIP in %.1fs" % (time.time() - t0), flush=True)
    return 0

  rc_mo, out_mo, _rec_mo = _run_child(os.path.join(outdir, "mono"), force_mono=True)
  if rc_mo != 0:
    print(out_mo, flush=True)
    fails.append("mono-control child exited rc=%d -- leg (b) has no control" % rc_mo)

  rows_st = _census(out_st)
  rows_mo = _census(out_mo)

  # ---- leg (a): PRESENT
  hits_st = [r for r in rows_st
             if r["pass"] == "depth_prepass" and r["pass_stereo"] == 1 and r["arm"] == "per-view"]
  print("LEG_PRESENT total_census_lines=%d per-view_stereo_prepass=%d" %
        (len(rows_st), len(hits_st)), flush=True)
  for r in hits_st:
    print("  %s" % json.dumps(r, sort_keys=True), flush=True)
  if not rows_st:
    fails.append("PRESENT: the depth prepass emitted NO [SPVR:DPPSEL] line at all -- the "
                 "prepass is unobservable again (announce removed, or nothing reached "
                 "_createFxPipelineDPP)")
  elif not hits_st:
    fails.append("PRESENT: no [SPVR:DPPSEL] line with pass<depth_prepass> pass_stereo<1> "
                 "arm<per-view> -- a rigid drawable in the depth_prepass layer never bound "
                 "an _ST prepass technique inside the single-pass node")
  else:
    if not any(r["family"] == "rigid" for r in hits_st):
      fails.append("PRESENT: the per-view prepass lines name families %s -- family<rigid> is "
                   "missing, and rigid is the family this rig draws"
                   % sorted(set(r["family"] for r in hits_st)))
    for r in hits_st:
      if not r["technique"].endswith("_ST"):
        fails.append("PRESENT: %s reported arm<per-view> on technique<%s>, which carries no "
                     "_ST suffix -- the arm derivation disagrees with the technique name"
                     % (r["family"], r["technique"]))

  # ---- leg (b): FALLBACK (the guard must be OBSERVED firing)
  hits_mo = [r for r in rows_mo
             if r["pass"] == "depth_prepass" and r["pass_stereo"] == 1
             and r["arm"] == "MONO-FALLBACK"]
  print("LEG_FALLBACK total_census_lines=%d mono_fallback=%d" % (len(rows_mo), len(hits_mo)),
        flush=True)
  for r in hits_mo:
    print("  %s" % json.dumps(r, sort_keys=True), flush=True)
  if rc_mo == 0 and not hits_mo:
    fails.append("FALLBACK: with ORKID_GATE0_FORCE_MONO_TEK=1 the census printed no "
                 "arm<MONO-FALLBACK> at pass_stereo<1> (%d prepass lines seen) -- the census "
                 "cannot name the very defect it exists for; the usual cause is an announce "
                 "throttle that already spent its line on a mono shadow pass"
                 % len(rows_mo))
  for r in hits_mo:
    if r["technique"].endswith("_ST"):
      fails.append("FALLBACK: %s reported arm<MONO-FALLBACK> on technique<%s>, which DOES "
                   "carry the _ST suffix -- the arm derivation is inverted"
                   % (r["family"], r["technique"]))

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
    print(out_st, flush=True)
    print("TESTVERDICT FAIL (%d): %s" % (len(fails), "; ".join(fails[:6])), flush=True)
    print("test_spvr_dpp_census: FAIL in %.1fs" % dt, flush=True)
    return 1
  print("TESTVERDICT PASS -- the depth prepass names every family it binds: %d per-view "
        "census line(s) under the single-pass node, and %d MONO-FALLBACK line(s) when the "
        "mono technique is forced into the stereo pass (%.1fs)"
        % (len(hits_st), len(hits_mo), dt), flush=True)
  return 0


if __name__ == "__main__":
  if len(sys.argv) > 2 and sys.argv[1] == "--render":
    sys.exit(_render(sys.argv[2]))
  sys.exit(main())
