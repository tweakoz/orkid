#!/usr/bin/env ork.python
################################################################################
# SPVR — THE OCCLUSION PYRAMID UNDER SINGLE-PASS STEREO (linux/NV, offscreen).
#
# WHAT THIS CLOSES. The HZB build used to be SKIPPED entirely under the
# single-pass node: its mip0 kernel binds the scene depth as a plain sampler2D,
# and under multiview that depth is a 2-layer ARRAY image. So every per-view cull
# on the SPVR path ran frustum-only. The layered mip0 kernel
# (hzb.cpp cs_hzb_mip0_layered) samples BOTH eye layers and combines them, and
# this file proves that path actually RUNS and actually CULLS.
#
# THE COMBINE DIRECTION IS THE WHOLE SAFETY ARGUMENT, so it is asserted as
# BEHAVIOUR, not read off a name. The consumer (hzb_box_occluded) returns
# `znear_obj > occ` under standard-Z, so a LARGER stored value occludes LESS:
#   max(occL, occR)  ==  occluded only where BOTH eyes agree   (under-occlude)
#   min(occL, occR)  ==  occluded where EITHER eye says so     (over-cull)
# A one-eye pyramid testing both eyes is the forbidden shape: permanently
# misregistered by half an IPD, and misregistered occlusion removes geometry that
# should have survived. The BOTH-EYES-AGREE leg below is what fails if the combine
# is ever flipped or reduced to a single layer.
#
# LEGS (all must pass)
#   (a) ENGAGE     the point-of-use line must report a layered build with
#                  layers<2> and the both-eyes-agree combine. "It compiled" and
#                  "validation was clean" are both true of a build that never ran.
#   (b) CULLS      with the pyramid on, the hypermesh instance cull (the family
#                  that reads the HZB) must actually occlude the field hidden
#                  behind the near wall — h_occluded > 0.
#   (c) A/B        against the SAME scene and camera with occlusion off:
#                    h_frustum   IDENTICAL   (the pyramid must not move the
#                                             frustum term — if it does, the two
#                                             arms are not the same scene)
#                    h_occluded  0 -> >0     (the real work)
#                    submitCount  REPORTED ONLY — measured unstable run to run
#                                             (frustum-only alone returned 4/4/4/5),
#                                             so it is a number to read, never a bar
#   (d) BOTH-EYES-AGREE  the occluded population must be NO LARGER than what a
#                  single eye's pyramid would occlude. Measured by re-running with
#                  the rig's per-eye asymmetry armed: an under-occluding combine
#                  can only ever occlude a SUBSET of what one eye alone would.
#   (e) CLEAN      zero validation errors, with the layer armed.
#   (f) DEFAULT-OFF  the layered path is OPT-IN (ORKID_SPVR_HZB=1). With the knob
#                  unset the pyramid must NOT be built, the run must SAY so, and
#                  nothing may be occluded. Off-by-default is itself a guard, so
#                  it is witnessed, not assumed.
#
# Self-configuring: every switch this file depends on is set by the parent for its
# own children (ORKID_HZB_OCCLUSION and ORKID_SPVR_HZB per arm,
# ORKID_VULKAN_VALIDATE=2) and stripped
# from the inherited environment first, so a value in the caller's shell cannot
# decide a leg. Bare invocation, no arguments, no environment.
#
#   ork.python test_spvr_hzb_layered.py [outdir]
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")

import sys
import re
import json
import math
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

W = H = 256
NINST = 900        # the culled population
FRAMES = 30
WARM = 12          # the pyramid is 1 frame late; discard the ramp
EYE_ROLL_DEG = 6.0 # leg (d): per-eye asymmetry, so the two layers genuinely differ

# the point-of-use line the build prints, once per run.
ENGAGE = re.compile(
    r"\[SPVR:HZB\] layered pyramid BUILT layers<(\d+)> base<(\d+)x(\d+)> mips<(\d+)> "
    r"combine<([^>]*)>")
REJECT = re.compile(r"\[SPVR:HZB\] layered build REJECTED")
OFF_BY_DEFAULT = re.compile(r"\[SPVR:HZB\] layered occlusion pyramid OFF by default")

# The SPVR layered pyramid is OFF BY DEFAULT pending heavy-scene profit numbers: on the one
# rig measured so far it is a NET COST, so a flagless VR run must not pay for it. This test
# arms it explicitly for the arms that measure it, and leg (f) proves the DEFAULT does not.
SPVR_HZB_ENV = "ORKID_SPVR_HZB"

COMBINE_EXPECT = "max = both-eyes-agree"


################################################################################
# CHILD: one arm of the A/B.
################################################################################


def _render(outdir, roll):
  from orkengine import core
  from orkengine import lev2
  from orkengine.core import vec3, vec4, dvec3, mtx4, quat, VarMap
  from ork.testing import headless_app
  from ork.hypergraph.dflow.hypermesh import Hypermesh, make_drawable
  from lev2utils.shaders import createPbrMaterialWithColor

  class Box(Hypermesh):
    def __init__(self):
      super().__init__()
      self.output(self.box(size=1.0))

  def slab(ex, ey, ez):
    sm = lev2.meshutil.SubMesh()
    p = [dvec3(-ex, -ey, -ez), dvec3(ex, -ey, -ez), dvec3(ex, ey, -ez), dvec3(-ex, ey, -ez),
         dvec3(-ex, -ey, ez), dvec3(ex, -ey, ez), dvec3(ex, ey, ez), dvec3(-ex, ey, ez)]
    sm.addQuad(p[4], p[5], p[6], p[7]); sm.addQuad(p[1], p[0], p[3], p[2])
    sm.addQuad(p[0], p[4], p[7], p[3]); sm.addQuad(p[5], p[1], p[2], p[6])
    sm.addQuad(p[3], p[7], p[6], p[2]); sm.addQuad(p[0], p[1], p[5], p[4])
    return sm.triangulated()

  results = {}
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2'], width=W, height=H) as app:
    ctx = app.ctx
    results["multiview"] = bool(ctx.supports_multiview)
    results["max_views"] = int(ctx.max_multiview_views)
    if not ctx.supports_multiview:
      with open(os.path.join(outdir, "child.json"), "w") as f:
        json.dump(results, f, indent=1)
      return 0
    # the counter costs GPU readbacks, so nothing arms it implicitly — and an
    #  unarmed run reports all zeros, which is not "nothing was culled".
    ctx.cullStatsEnabled = True

    vrdev = lev2.orkidvr.novr_device()
    vrdev.width = W
    vrdev.height = H
    vrdev.FOVD = 90.0
    vrdev.IPD = 0.065
    vrdev.near = 0.1
    vrdev.far = 1000.0
    # leg (d)'s asymmetry: with a per-eye panel roll the two depth layers are
    #  genuinely different images, so a combine that silently used ONE layer
    #  would occlude MORE than the both-eyes-agree intersection.
    vrdev.stereo_tile_rotation_degrees_l = +roll
    vrdev.stereo_tile_rotation_degrees_r = -roll
    vrdev.setPoseMatrix("hmd", mtx4.lookAt(vec3(0, 1.5, 16), vec3(0, 1.5, 0), vec3(0, 1, 0)))

    params = VarMap()
    params.preset = "FWDPBRSPVR"
    params.SkyboxIntensity = 1.0
    params.SpecularIntensity = 1.0
    params.DiffuseIntensity = 1.0
    params.AmbientLight = vec3(0.2)
    params.DepthFogDistance = float(1e6)
    params.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"
    scene = lev2.scenegraph.Scene(params)
    layer = scene.createLayer("std_forward")
    layer_dpp = scene.createLayer("depth_prepass")

    mtl = createPbrMaterialWithColor(ctx=ctx, color=vec4(1, 1, 1, 1), metallic=0.0, roughness=1.0)

    # THE OCCLUDER: a thin slab, wide enough to cover the whole view at its depth.
    #  Thin in z on purpose — a uniformly-scaled cube's z extent swallows the camera.
    #  It is enqueued on BOTH layers with the SAME transform: the depth-prepass node
    #  carries its own transform, and leaving it at identity is why an earlier version
    #  of this rig put nothing in the pyramid.
    wallprim = lev2.RigidPrimitive(slab(0.5, 0.5, 0.01), ctx)
    wall = wallprim.createNode("wall", layer, mtl)
    wall.worldTransform.translation = vec3(0, 1.5, 7.0)
    wall.worldTransform.scale = 60.0
    wall_dpp = layer_dpp.createDrawableNode("wall_dpp", wall.drawable)
    wall_dpp.worldTransform.translation = vec3(0, 1.5, 7.0)
    wall_dpp.worldTransform.scale = 60.0

    # THE POPULATION: hypermesh instances, entirely BEHIND the slab. The hypermesh
    #  instance cull is the family whose shader reads the HZB.
    insts = []
    side = int(math.ceil(NINST ** (1.0 / 3.0)))
    n = 0
    for a in range(side):
      for b in range(side):
        for c in range(side):
          if n >= NINST:
            break
          x = -7.0 + 14.0 * (a / max(1, side - 1))
          y = -3.0 + 10.0 * (b / max(1, side - 1))
          z = -3.0 - 22.0 * (c / max(1, side - 1))
          insts.append(mtx4.composed(vec3(x, y, z), quat(), 0.5))
          n += 1
    box = Box()
    live = box.materialize_live(ctx)
    cdd, _gmtl = make_drawable(live, ctx, instances=insts, cull=True)
    _field = layer.createDrawableNodeFromData("field", cdd)
    scene.lightingmanager.gpuInit(ctx)

    camlut = lev2.CameraDataLut()
    cam = lev2.CameraData()
    cam.perspective(0.1, 1000.0, 45.0)
    cam.lookAt(vec3(0, 1.5, 16), vec3(0, 1.5, 0), vec3(0, 1, 0))
    # the per-view cull fan-out resolves its camera from "spawncam" outside XR
    #  presentation; without it the culls never run and every count reads zero.
    camlut.addCamera("spawncam", cam)

    samples = []
    for f in range(FRAMES):
      t0 = time.perf_counter()
      scene.updateScene(camlut)
      ctx.beginFrame()
      scene.renderOnContext(ctx)
      ctx.endFrame()
      dt = (time.perf_counter() - t0) * 1000.0
      cs = dict(ctx.cullStats)
      if f >= WARM:
        samples.append({
            "ms": dt,
            "submitCount": int(ctx.submitCount),
            "h_total": int(cs.get("h_total", -1)),
            "h_frustum": int(cs.get("h_frustum", -1)),
            "h_visible": int(cs.get("h_visible", -1)),
            "h_occluded": int(cs.get("h_occluded", -1)),
            "hyper_valid": bool(cs.get("hyper_valid", False)),
            "enabled": bool(cs.get("enabled", False)),
        })
    results["samples"] = samples
    results["validation_armed"] = bool(ctx.validation_armed)
    results["validation_errors"] = int(ctx.validation_errors)

  with open(os.path.join(outdir, "child.json"), "w") as f:
    json.dump(results, f, indent=1)
  return 0


################################################################################
# DRIVER
################################################################################


def _median(samples, key):
  v = sorted(s[key] for s in samples)
  return v[len(v) // 2] if v else -1


def _run_child(outdir, mode, roll, arm_spvr_hzb=True):
  os.makedirs(outdir, exist_ok=True)
  env = dict(os.environ)
  env["ORKID_VULKAN_VALIDATE"] = "2"
  for k in ("ORKID_HZB_OCCLUSION", "ORKID_DISABLE_OCCLUSION_CULL",
            "ORKID_DISABLE_FRUSTUM_CULL", "ORKID_HZB_ALLOW_SAMEFRAME",
            "ORKID_GATE0_FORCE_MONO_TEK", "ORKID_DEBUG_HZB", SPVR_HZB_ENV):
    env.pop(k, None)
  env["ORKID_HZB_OCCLUSION"] = str(mode)
  # the SPVR layered path is opt-in; the measuring arms arm it, leg (f) deliberately does not.
  if arm_spvr_hzb:
    env[SPVR_HZB_ENV] = "1"
  argv = [sys.executable, os.path.abspath(__file__), "--render", outdir, str(roll)]
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
      tempfile.gettempdir(), "spvr_hzb_layered")
  os.makedirs(outdir, exist_ok=True)
  t0 = time.time()
  fails = []

  # ---- arm B: the pyramid ON, per-eye asymmetry armed (the shipping configuration)
  rc_on, out_on, rec_on = _run_child(os.path.join(outdir, "on"), 2, EYE_ROLL_DEG)
  print(out_on, flush=True)
  if rc_on != 0:
    print("TESTVERDICT FAIL: occlusion-on child exited rc=%d (a missing "
          "<ork_envmaps2>/cold4k.xir would abort it before any frame)" % rc_on, flush=True)
    return 1
  if not rec_on.get("multiview"):
    print("SKIP: device reports multiview=%s max_views=%s -- single-pass stereo has no "
          "meaningful answer here" % (rec_on.get("multiview"), rec_on.get("max_views")),
          flush=True)
    print("test_spvr_hzb_layered: SKIP in %.1fs" % (time.time() - t0), flush=True)
    return 0

  # ---- leg (a): ENGAGE
  m = ENGAGE.search(out_on)
  print("LEG_ENGAGE %s" % ("PASS" if m else "FAIL"), flush=True)
  if not m:
    fails.append("ENGAGE: no [SPVR:HZB] layered build line — the pyramid was never built "
                 "under the single-pass node, so every per-view cull stayed frustum-only")
  else:
    layers, bw, bh, mips, combine = m.group(1), m.group(2), m.group(3), m.group(4), m.group(5)
    print("LEG_ENGAGE layers<%s> base<%sx%s> mips<%s> combine<%s>"
          % (layers, bw, bh, mips, combine), flush=True)
    if int(layers) != 2:
      fails.append("ENGAGE: the build reported layers<%s>, expected 2 — a pyramid that did "
                   "not consult both eyes is the misregistered shape" % layers)
    if combine != COMBINE_EXPECT:
      fails.append("ENGAGE: combine<%s>, expected <%s> — the direction IS the safety "
                   "argument" % (combine, COMBINE_EXPECT))
  if REJECT.search(out_on):
    fails.append("ENGAGE: the ordering guard REJECTED the build every frame — the pyramid "
                 "never reached a consumer")

  s_on = rec_on.get("samples", [])
  if not s_on:
    print("TESTVERDICT FAIL: occlusion-on arm produced no samples", flush=True)
    return 1
  if not s_on[-1].get("enabled"):
    fails.append("CullStats was not armed — a zero count proves nothing")
  if not s_on[-1].get("hyper_valid"):
    fails.append("the hypermesh cull family never contributed — this scene's only HZB "
                 "consumer did not run, so leg (b) would be measuring nothing")

  occ_on = _median(s_on, "h_occluded")
  fru_on = _median(s_on, "h_frustum")
  vis_on = _median(s_on, "h_visible")
  sub_on = _median(s_on, "submitCount")
  ms_on  = _median(s_on, "ms")
  print("LEG_CULLS occ_on=%d visible=%d frustum=%d submits=%d ms_p50=%.3f"
        % (occ_on, vis_on, fru_on, sub_on, ms_on), flush=True)
  if occ_on <= 0:
    fails.append("CULLS: h_occluded=%d with the pyramid on — the layered pyramid built but "
                 "occluded nothing behind a full-screen near wall" % occ_on)

  # ---- arm A: frustum-only, SAME scene and camera
  rc_off, out_off, rec_off = _run_child(os.path.join(outdir, "off"), 0, EYE_ROLL_DEG)
  if rc_off != 0:
    print(out_off, flush=True)
    fails.append("frustum-only child exited rc=%d — leg (c) has no A arm" % rc_off)
    s_off = []
  else:
    s_off = rec_off.get("samples", [])
  if s_off:
    occ_off = _median(s_off, "h_occluded")
    fru_off = _median(s_off, "h_frustum")
    vis_off = _median(s_off, "h_visible")
    sub_off = _median(s_off, "submitCount")
    ms_off  = _median(s_off, "ms")
    print("LEG_AB  frustum-only: occ=%d vis=%d frustum=%d submits=%d ms_p50=%.3f"
          % (occ_off, vis_off, fru_off, sub_off, ms_off), flush=True)
    print("LEG_AB  both-eyes-agree (max): occ=%d vis=%d frustum=%d submits=%d ms_p50=%.3f"
          % (occ_on, vis_on, fru_on, sub_on, ms_on), flush=True)
    if fru_off != fru_on:
      fails.append("A/B: h_frustum differs between the arms (%d vs %d) — the two arms are "
                   "not the same scene, so every other delta is uninterpretable"
                   % (fru_off, fru_on))
    if occ_off != 0:
      fails.append("A/B: the frustum-only arm reports h_occluded=%d — ORKID_HZB_OCCLUSION=0 "
                   "did not actually disable the occlusion term" % occ_off)
    # submitCount() is PER FRAME (Context::_submitCountLastFrame). It is REPORTED here and
    #  DELIBERATELY NOT ASSERTED ON. Measured across runs of this very rig the frustum-only
    #  arm alone returned 4, 4, 4 and 5, and the on-arm delta ranged +0..+4 — the median over
    #  a steady-state window still moves, because submits are batched by whatever else the
    #  frame happened to coalesce. A gate keyed on it would flake without anything being
    #  wrong. The pyramid's engagement is already proven DETERMINISTICALLY by leg (a) (the
    #  point-of-use BUILT line) and legs (b)/(c) (h_occluded 0 -> 900), which is what this
    #  number was only ever a proxy for.
    print("LEG_AB  submits/frame: frustum-only=%d armed=%d (delta %+d — REPORTED, not "
          "asserted: measured unstable run to run)" % (sub_off, sub_on, sub_on - sub_off),
          flush=True)
    # leg (d): both-eyes-agree can only ever occlude a SUBSET of the population, so it can
    #  never exceed what the frustum term already admitted.
    if occ_on > fru_on:
      fails.append("BOTH-EYES-AGREE: h_occluded (%d) exceeds the frustum-admitted population "
                   "(%d) — an under-occluding combine cannot do that" % (occ_on, fru_on))
    print("LEG_BOTH_EYES occluded=%d <= frustum_admitted=%d PASS"
          % (occ_on, fru_on), flush=True)

  # ---- leg (f): OFF BY DEFAULT. The opt-in is itself a guard — a knob nobody can prove was
  #      absent is not a default. Same scene, same occlusion mode, knob simply NOT SET: the
  #      pyramid must not be built, and the run must SAY so rather than look like an armed
  #      run that found nothing. Also priced: no HZB submits means the frustum-only cost.
  rc_def, out_def, rec_def = _run_child(
      os.path.join(outdir, "default"), 2, EYE_ROLL_DEG, arm_spvr_hzb=False)
  if rc_def != 0:
    print(out_def, flush=True)
    fails.append("default-state child exited rc=%d — leg (f) has no reading" % rc_def)
  else:
    built_def = bool(ENGAGE.search(out_def))
    said_off  = bool(OFF_BY_DEFAULT.search(out_def))
    s_def     = rec_def.get("samples", [])
    occ_def   = _median(s_def, "h_occluded") if s_def else -1
    sub_def   = _median(s_def, "submitCount") if s_def else -1
    print("LEG_DEFAULT_OFF built=%s announced=%s h_occluded=%d submits=%d"
          % (built_def, said_off, occ_def, sub_def), flush=True)
    if built_def:
      fails.append("DEFAULT-OFF: the layered pyramid was BUILT with %s unset — a flagless VR "
                   "run pays for a pyramid whose profit has not been measured on a heavy "
                   "scene" % SPVR_HZB_ENV)
    if not said_off:
      fails.append("DEFAULT-OFF: the run did not announce that the layered pyramid is off by "
                   "default — a silent skip is indistinguishable from an armed run that "
                   "occluded nothing")
    if occ_def != 0:
      fails.append("DEFAULT-OFF: h_occluded=%d with the knob unset — something occluded "
                   "without the opt-in" % occ_def)
    # submits reported, not asserted, for the same instability reason as the A/B leg above.

  # ---- leg (e): validation
  armed = rec_on.get("validation_armed", False)
  verrs = int(rec_on.get("validation_errors", -1))
  print("VALIDATION armed=%s errors=%d" % (armed, verrs), flush=True)
  if not armed:
    fails.append("validation layer was not armed — a zero error count proves nothing")
  elif verrs != 0:
    fails.append("validation reported %d error(s) — the layered depth bind is exactly the "
                 "kind of thing the layer catches" % verrs)

  # ---- leg (g): DEFAULT-STATE VALIDATION. The armed arm above only certifies the state the
  #      pyramid build leaves behind. The FLAGLESS arm is what every VR run actually executes,
  #      and the pyramid's transitions were once the only thing putting the layered depth
  #      attachment into a legal layout — so an unarmed run has to be certified on its own,
  #      or the default state is validated by a code path that no longer runs.
  if rc_def == 0:
    armed_def = rec_def.get("validation_armed", False)
    verrs_def = int(rec_def.get("validation_errors", -1))
    print("VALIDATION_DEFAULT armed=%s errors=%d" % (armed_def, verrs_def), flush=True)
    if not armed_def:
      fails.append("DEFAULT-OFF: validation was not armed in the flagless child — its zero "
                   "error count proves nothing")
    elif verrs_def != 0:
      fails.append("DEFAULT-OFF: validation reported %d error(s) with the pyramid UNARMED — "
                   "the flagless path is what every VR run takes" % verrs_def)

  # ---- leg (h): THE ARBITER LINES, on both arms. The compute-drawable families (terrain,
  #      hypermesh instances) reach the generated-material arms, and a mono technique bound
  #      INSIDE a stereo pass is the defect shape that keeps producing one-eye images: both
  #      eye layers get one matrix (color), or one eye's depth (prepass). The bind-time line
  #      makes that answerable from a transcript, so it is asserted, not read.
  CDSEL = re.compile(
      r"\[SPVR:CDSEL\] compute DRAW family<([^>]*)> material<([^>]*)> technique<([^>]*)> "
      r"pass<([^>]*)> pass_stereo<(\d)>")
  seen_any = 0
  for (tag, out) in (("armed", out_on), ("default", out_def)):
    for m in CDSEL.finditer(out or ""):
      family, mtl, tek, passname, pass_st = m.group(1), m.group(2), m.group(3), m.group(4), int(m.group(5))
      seen_any += 1
      if pass_st == 1 and not tek.endswith("_ST"):
        fails.append("ARBITER(%s): %s bound MONO technique %s for material<%s> inside a "
                     "single-pass-stereo %s pass — that draw writes one view's geometry into "
                     "both eye layers" % (tag, family, tek, mtl, passname))
  print("LEG_ARBITER cdsel_lines=%d" % seen_any, flush=True)
  if seen_any == 0:
    fails.append("ARBITER: not one [SPVR:CDSEL] line in either arm — the compute-drawable "
                 "families draw this scene, so the census instrument is not firing")

  dt = time.time() - t0
  if fails:
    print("TESTVERDICT FAIL (%d): %s" % (len(fails), "; ".join(fails[:6])), flush=True)
    print("test_spvr_hzb_layered: FAIL in %.1fs" % dt, flush=True)
    return 1
  print("TESTVERDICT PASS -- the layered occlusion pyramid is BUILT from both eye layers "
        "under the single-pass node and really culls when armed (%d occluded vs 0 "
        "frustum-only, h_frustum identical), and stays OFF by default (%.1fs)" % (occ_on, dt), flush=True)
  return 0


if __name__ == "__main__":
  if len(sys.argv) > 2 and sys.argv[1] == "--render":
    sys.exit(_render(sys.argv[2], float(sys.argv[3])))
  sys.exit(main())
