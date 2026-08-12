#!/usr/bin/env ork.python
################################################################################
# STEREO OCCLUSION CULLING — the pyramid built from the stereo depth buffer must
# reject instances, must reject the SAME ones every frame, and must never reject
# one an eye can still see.
#
# WHAT THIS CLOSES. Under single-pass stereo the occlusion pyramid used to be
# skipped outright: the depth one multiview pass writes is a 2-layer ARRAY image,
# and the mip0 kernel bound it as a plain sampler2D. Every per-view cull in VR
# therefore ran frustum-only, silently. Building it from both eye layers then
# introduced the opposite failure: the stored depth is what the EYES saw, the
# cull indexes the pyramid with the CENTER camera, and a same-texel combine
# answers for the wrong world point near a silhouette — it removed geometry the
# eyes can see, and removed a DIFFERENT set at each head position (VR flicker
# that mono cannot produce). The build now widens its reduction by the measured
# eye->center displacement, and refuses to publish at all when the frame's cull
# would index it with the desktop camera instead of the head camera.
#
# THE COMBINE DIRECTION IS THE SAFETY ARGUMENT. The consumer (hzb_box_occluded)
# ends in `znear_obj > occ` under standard-Z, so a LARGER stored value occludes
# LESS. max() is the under-occlude direction, which is also why widening the
# reduction window is always safe: a max over more samples can only grow.
#
# LEGS (all must pass)
#   (a) ENGAGE   the point-of-use line reports a layered build, layers<2>, the
#                both-eyes-agree combine, AND a solved registration whose 1/z
#                term is nonzero — a stereo rig with a real IPD cannot register
#                at zero displacement, so a zero there means the model collapsed.
#   (b) CULLS    the hypermesh instance cull rejects part of the field hidden
#                behind the near fence: h_occluded > 0.
#   (c) A/B      the SAME scene and camera with the pyramid disarmed
#                (ORKID_SPVR_HZB=0): h_frustum IDENTICAL, h_occluded >0 -> 0.
#   (d) DEFAULT  the armed arm sets NOTHING: stereo occlusion is on by default.
#   (e) STABLE   at a frozen head pose every sampled frame reports the SAME
#                h_occluded. An occluded population that breathes across
#                identical frames IS the flicker, measured at its source.
#   (f) CONSERVATIVE  per-eye renders with the pyramid armed are pixel-identical
#                to the disarmed renders. Culling only what is truly hidden
#                cannot change the picture, so any differing pixel is geometry
#                the cull ate. The scene is a picket fence with the field
#                straddling its gaps ON PURPOSE: a full-screen occluder hides
#                everything and would pass this leg while proving nothing.
#   (g) REFUSED  an arm with NO tracked pose (so the per-view cull resolves the
#                desktop camera, not the head camera) must REFUSE to publish and
#                cull frustum-only. That configuration measured 60-745 eaten
#                pixels when it was allowed to publish.
#   (h) ASYM     with ASYMMETRIC per-eye frusta (what every runtime reports) the
#                registration must carry the asymmetry in its at-infinity MAP and
#                leave a residual window small enough to survive the reduction's
#                cap. Charged to the window instead, a measured headset needed ~65
#                mip0 texels — past the cap on every texel, so the pyramid
#                published far everywhere and VR occlusion was inert while both
#                symmetric arms culled normally.
#
# LEGS (b) (c) (e) (f) RUN AT THREE RIG SETTINGS, and the last two are the
# point. With multisampling on there are two depth images: the depth TEST reads the
# multisample attachment, but no shader can sample that, so every sampler-side
# consumer — this pyramid included — reads the single-sample resolve copy. One
# multiview pass cannot resolve more than one view (Metal names a single array slice
# per pass), and measured here that copy carried NEITHER eye — it sat at the far clear
# in both layers, so the both-eyes-agree combine read far everywhere and occluded
# NOTHING. The backend now resolves the eyes one at a time after the depth pass, and this gate
# holds it to the same standard at both settings — rejecting, stable, and unable to
# eat a pixel. Antialiasing is what VR actually runs at, so a pyramid that only works
# with it off is a pyramid that never runs.
#
# Self-configuring: every switch it depends on is stripped from the inherited
# environment and set per arm, so a value in the caller's shell cannot decide a
# leg. Bare invocation, no arguments, no environment.
#
#   ork.python test_spvr_hzb_stereo_cull.py [outdir]
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"

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
sys.path.insert(0, os.path.join(_ROOT, "ork.lev2", "examples", "python"))

W = H = 256
NINST = 640     # the culled population
FRAMES = 20
WARM = 10       # the pyramid is one frame late; discard the ramp

# the point-of-use line the stereo build prints, once per run
ENGAGE = re.compile(
    r"\[SPVR:HZB\] layered pyramid BUILT layers<(\d+)> base<(\d+)x(\d+)> mips<(\d+)> "
    r"combine<([^>]*)> reg<c0 ([-\d.]+) c1 ([-\d.]+) texels>")
# the per-eye at-infinity registration map the same line reports. Its translation terms are
# what an asymmetric rig makes large, its PROJECTIVE terms are what a CANTED rig makes
# nonzero, and applying the map (rather than searching over it) is what keeps the reduction
# inside its window cap. The map is a homography because a canted display is not an affine
# reparameterization of the center image — a headset cants ~3.8 degrees per eye, and the
# residual an affine map left at infinity was alone past the cap.
REGMAP = re.compile(
    r"map<L s ([-\d.]+),([-\d.]+) t ([-\d.]+),([-\d.]+) p ([-\d.]+),([-\d.]+) \| "
    r"R s ([-\d.]+),([-\d.]+) t ([-\d.]+),([-\d.]+) p ([-\d.]+),([-\d.]+)>")
# the forward node announces the antialiasing level it actually built at, once — the
# only proof that an arm asking for multisampling got it (the level is clamped to the
# device maximum, and a device that gave 1x would make the whole arm vacuous)
MSAA_LINE = re.compile(r"ForwardPBR MSAA: level<(\d+)> -> (\d+)x \(device max (\d+)\)")
REJECT = re.compile(r"\[SPVR:HZB\] layered build REJECTED")
REFUSE = re.compile(r"\[SPVR:HZB\] layered build REFUSED")
COMBINE_EXPECT = "max = both-eyes-agree"

# the ONE knob this path has, and it only ever DISARMS (leg (d) is what makes that
# statement checkable): the shipped default is the pyramid ON under stereo.
SPVR_HZB_ENV = "ORKID_SPVR_HZB"

# the rig settings every cull leg is measured at: (antialiasing level, nose-side
# per-eye frustum inset in degrees, tag).
#   aa-off / aa-4x   the two antialiasing settings, symmetric frusta
#   aa-4x-asym       ASYMMETRIC per-eye frusta with the center view averaged from them —
#                    the shape every real runtime reports, and the one a headless rig
#                    never produces on its own. Measured on a headset, the eye->center
#                    displacement that survives at infinity was ~65 mip0 texels; folding
#                    it into the reduction's search window put every texel over the
#                    window cap, so the pyramid published FAR everywhere and stereo
#                    occlusion was silently inert in VR while both symmetric arms above
#                    culled normally.
MSAA_ARMS = [(0, 0.0, "aa-off"), (2, 0.0, "aa-4x"), (2, 12.0, "aa-4x-asym")]


################################################################################
# CHILD: one arm.
################################################################################


def _render(outdir, tracked, msaa, asym=0.0):
  import numpy
  from PIL import Image
  from orkengine import core
  from orkengine import lev2
  from orkengine.core import vec3, vec4, dvec3, mtx4, quat, VarMap
  from ork.testing import headless_app
  from ork.testing.capture import ensure_parent_dir
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

  EYE = vec3(0, 1.5, 3.0)

  results = {}
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2'], width=W, height=H) as app:
    ctx = app.ctx
    results["multiview"] = bool(ctx.supports_multiview)
    results["max_views"] = int(ctx.max_multiview_views)
    # headless kwargs cannot reach the app init data, so the antialiasing setting is
    #  set through the live handle. Before the scene exists: the forward node sizes its
    #  primary target from this and only rebuilds when the level it built at changes.
    lev2.setForwardMsaaLevel(int(msaa))
    results["msaa_level"] = int(lev2.forwardMsaaLevel())
    if not ctx.supports_multiview:
      with open(os.path.join(outdir, "child.json"), "w") as f:
        json.dump(results, f, indent=1)
      return 0
    # the counters cost GPU readbacks, so nothing arms them implicitly — and an
    #  unarmed run reports all zeros, which is not "nothing was culled".
    ctx.cullStatsEnabled = True

    vrdev = lev2.orkidvr.novr_device()
    vrdev.width = W
    vrdev.height = H
    vrdev.FOVD = 90.0
    vrdev.IPD = 0.065
    vrdev.near = 0.1
    vrdev.far = 1000.0
    # ASYMMETRY: with this at zero all three cameras share one projection, which no headset
    #  does — the at-infinity eye->center map is then identity and never exercised.
    vrdev.eye_fov_inset_degrees = float(asym)
    results["asym_deg"] = float(asym)
    if tracked:
      # A TRACKED pose is what makes Scene::_renderIMPL resolve the HEAD camera for the
      #  per-view cull — the camera the stereo pyramid is registered against. Looking
      #  straight down -z, the HMD pose world matrix is identity rotation at the eye.
      vrdev.setTrackedPose(EYE, quat(), vec3(0, 0, 0), vec3(0, 0, 0))
      vrdev.prediction_bias = 0.0
    else:
      # leg (g): pose set directly -> no tracked pose -> the cull falls back to the
      #  desktop camera, and the build must refuse rather than publish misregistered.
      vrdev.setPoseMatrix("hmd", mtx4.lookAt(EYE, vec3(0, 1.5, 0), vec3(0, 1, 0)))

    params = VarMap()
    params.preset = "FWDPBRSPVR"
    params.SkyboxIntensity = 1.0
    params.SpecularIntensity = 1.0
    params.DiffuseIntensity = 1.0
    params.AmbientLight = vec3(0.4)
    params.DepthFogDistance = float(1e6)
    params.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"
    scene = lev2.scenegraph.Scene(params)
    layer = scene.createLayer("std_forward")
    layer_dpp = scene.createLayer("depth_prepass")

    mtl = createPbrMaterialWithColor(ctx=ctx, color=vec4(1, 1, 1, 1), metallic=0.0, roughness=1.0)

    # THE OCCLUDER: a PICKET FENCE, not a wall. Leg (f) can only see an over-cull where
    #  the population straddles a silhouette, and a full-screen occluder has exactly one.
    #  Thin in z on purpose — a uniformly-scaled cube's z extent swallows the camera.
    #  Enqueued on BOTH layers with the SAME transform: the depth-prepass node carries
    #  its own transform, and leaving it at identity puts nothing in the pyramid.
    #  Picket WIDTH is load-bearing: the registered reduction trims a picket's occluding
    #  core by the eye->center displacement at its depth (a few mip0 texels), so pickets
    #  narrower than that trim have no core left and leg (b) would read zero.
    keep = []
    for i in range(5):
      x = -1.8 + 0.9 * i
      pr = lev2.RigidPrimitive(slab(0.25, 2.0, 0.02), ctx)
      nd = pr.createNode("bar%d" % i, layer, mtl)
      nd.worldTransform.translation = vec3(x, 1.5, 1.2)
      nd_d = layer_dpp.createDrawableNode("bar%d_dpp" % i, nd.drawable)
      nd_d.worldTransform.translation = vec3(x, 1.5, 1.2)
      keep += [pr, nd, nd_d]

    # THE POPULATION: instances the size of an IPD, straddling the fence's gaps — the
    #  regime where the eye->center displacement is a large fraction of a footprint.
    insts = []
    side = int(math.ceil(NINST ** (1.0 / 3.0)))
    n = 0
    for a in range(side):
      for b in range(side):
        for c in range(side):
          if n >= NINST:
            break
          x = -2.4 + 4.8 * (a / max(1, side - 1))
          y = 0.2 + 2.6 * (b / max(1, side - 1))
          z = 0.2 - 3.2 * (c / max(1, side - 1))
          insts.append(mtx4.composed(vec3(x, y, z), quat(), 0.09))
          n += 1
    box = Box()
    live = box.materialize_live(ctx)
    cdd, _gmtl = make_drawable(live, ctx, instances=insts, cull=True)
    _field = layer.createDrawableNodeFromData("field", cdd)
    scene.lightingmanager.gpuInit(ctx)

    camlut = lev2.CameraDataLut()
    cam = lev2.CameraData()
    # RADIANS: CameraData::Persp feeds glm::perspectiveRH directly. Passing degrees here
    #  gives the cull a ~117-degree frustum against a 90-degree render — a mismatch that
    #  reads as an occlusion bug and is not one.
    cam.perspective(0.1, 1000.0, math.radians(90.0))
    cam.lookAt(EYE, vec3(0, 1.5, 0), vec3(0, 1, 0))
    # the per-view cull fan-out resolves its camera from "spawncam" outside XR
    #  presentation; without it the culls never run and every count reads zero.
    camlut.addCamera("spawncam", cam)

    samples = []
    for f in range(FRAMES):
      scene.updateScene(camlut)
      ctx.beginFrame()
      scene.renderOnContext(ctx)
      ctx.endFrame()
      cs = dict(ctx.cullStats)
      if f >= WARM:
        samples.append({
            "h_total": int(cs.get("h_total", -1)),
            "h_frustum": int(cs.get("h_frustum", -1)),
            "h_visible": int(cs.get("h_visible", -1)),
            "h_occluded": int(cs.get("h_occluded", -1)),
            "hyper_valid": bool(cs.get("hyper_valid", False)),
            "enabled": bool(cs.get("enabled", False)),
        })
    results["samples"] = samples

    # per-eye readback for leg (f). A FRESH frame: capturing inside the frame that
    #  produced the eye buffers leaves them in a host-read layout the composite then
    #  asserts on (the capture gate's finding, same accessor, same shape).
    outnode = scene.compositoroutputnode
    ctx.beginFrame()
    caps, futs = {}, []
    for (nm, left) in (("L", True), ("R", False)):
      rtg = outnode.downsampledEyeRtGroup(left)
      cb = lev2.CaptureBuffer()
      caps[nm] = cb
      futs.append((nm, ctx.FBI.captureAsFormat(rtg.buffer(0), cb, "RGBA8")))
    ctx.endFrame()
    for (nm, fut) in futs:
      ok = fut.wait(caps[nm])
      assert ok, "spvr hzb gate: eye capture never landed (%s)" % nm
      cb = caps[nm]
      arr = numpy.array(cb, dtype=numpy.uint8).reshape(cb.height, cb.width, 4)
      path = os.path.join(outdir, "eye_%s.png" % nm)
      ensure_parent_dir(path)
      Image.fromarray(arr[..., :3]).save(path)
      results["eye_%s" % nm] = path

  with open(os.path.join(outdir, "child.json"), "w") as f:
    json.dump(results, f, indent=1)
  return 0


################################################################################
# DRIVER
################################################################################


def _median(samples, key):
  v = sorted(s[key] for s in samples)
  return v[len(v) // 2] if v else -1


def _run_child(outdir, disarm=False, tracked=True, msaa=0, asym=0.0):
  os.makedirs(outdir, exist_ok=True)
  env = dict(os.environ)
  for k in ("ORKID_HZB_OCCLUSION", "ORKID_DISABLE_OCCLUSION_CULL", "ORKID_DISABLE_FRUSTUM_CULL",
            "ORKID_HZB_ALLOW_SAMEFRAME", "ORKID_DEBUG_HZB", SPVR_HZB_ENV):
    env.pop(k, None)
  # the armed arm sets NOTHING — leg (d): the shipped default IS the pyramid.
  if disarm:
    env[SPVR_HZB_ENV] = "0"
  argv = [sys.executable, os.path.abspath(__file__), "--render", outdir,
          "tracked" if tracked else "untracked", str(int(msaa)), str(float(asym))]
  proc = subprocess.run(argv, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=300)
  out = proc.stdout.decode("utf-8", "replace")
  rec = {}
  cjson = os.path.join(outdir, "child.json")
  if os.path.exists(cjson):
    rec = json.load(open(cjson))
  return proc.returncode, out, rec


def _eaten_pixels(armed_png, disarmed_png):
  """Pixels where the armed render differs from the disarmed one = geometry the cull
  removed but an eye can see. Returns (count over threshold, max channel delta)."""
  import numpy
  from PIL import Image
  a = numpy.asarray(Image.open(armed_png).convert("RGB"), dtype=numpy.int32)
  b = numpy.asarray(Image.open(disarmed_png).convert("RGB"), dtype=numpy.int32)
  if a.shape != b.shape:
    return (-1, -1)
  d = numpy.abs(a - b).max(axis=2)
  return (int((d > 8).sum()), int(d.max()))


def _run_setting(outdir, msaa, asym, tag, fails):
  """Every cull leg at ONE rig setting. Returns (occluded, skip_reason)."""
  pfx = "[%s] " % tag

  # ---- arm A: the shipping configuration (nothing set)
  rc_on, out_on, rec_on = _run_child(os.path.join(outdir, tag, "on"), msaa=msaa, asym=asym)
  print(out_on, flush=True)
  if rc_on != 0:
    fails.append("%schild exited rc=%d (a missing <ork_envmaps2>/cold4k.xir would abort it "
                 "before any frame)" % (pfx, rc_on))
    return (-1, None)
  if not rec_on.get("multiview"):
    return (-1, "device reports multiview=%s max_views=%s -- single-pass stereo has no "
                "meaningful answer here" % (rec_on.get("multiview"), rec_on.get("max_views")))

  # the setting has to have LANDED. An arm that silently ran with antialiasing off would
  #  pass every leg below while proving nothing about the resolve this gate exists for.
  got = int(rec_on.get("msaa_level", -1))
  mm  = MSAA_LINE.search(out_on)
  samples = int(mm.group(2)) if mm else 1
  print("%sLEG_SETTING level<%d> samples<%dx>" % (pfx, got, samples), flush=True)
  if got != msaa:
    fails.append("%sasked for antialiasing level %d, the engine reports %d" % (pfx, msaa, got))
  if msaa > 0 and samples < 2:
    fails.append("%santialiasing level %d resolved to %dx — the multisample depth path this "
                 "arm exists to exercise never engaged" % (pfx, msaa, samples))
  if msaa == 0 and mm:
    fails.append("%santialiasing was supposed to be OFF but the forward node reports %dx"
                 % (pfx, samples))

  # ---- leg (a) ENGAGE + leg (d) DEFAULT (this arm set no environment at all)
  m = ENGAGE.search(out_on)
  print("%sLEG_ENGAGE %s" % (pfx, "PASS" if m else "FAIL"), flush=True)
  if not m:
    fails.append("%sENGAGE: no [SPVR:HZB] layered build line with NOTHING set — stereo occlusion "
                 "is not on by default, so a plain VR run gets frustum-only culling" % pfx)
  else:
    layers, bw, bh, mips, combine, c0, c1 = m.groups()
    print("%sLEG_ENGAGE layers<%s> base<%sx%s> mips<%s> combine<%s> reg<c0 %s c1 %s>"
          % (pfx, layers, bw, bh, mips, combine, c0, c1), flush=True)
    if int(layers) != 2:
      fails.append("%sENGAGE: the build reported layers<%s>, expected 2 — a pyramid that did not "
                   "consult both eyes is the misregistered shape" % (pfx, layers))
    if combine != COMBINE_EXPECT:
      fails.append("%sENGAGE: combine<%s>, expected <%s> — the direction IS the safety argument"
                   % (pfx, combine, COMBINE_EXPECT))
    if float(c1) <= 0.0:
      fails.append("%sENGAGE: the registration solved c1=%s — with a nonzero IPD the eye->center "
                   "displacement cannot be zero, so the reduction is back to a same-texel "
                   "combine" % (pfx, c1))
    # ---- leg (h) ASYMMETRY: the at-infinity map must carry the rig's asymmetry, and the
    #  residual window must stay small enough to survive the cap (the CULLS leg below is
    #  what proves it did, since an overflowed texel publishes far and occludes nothing).
    mm2 = REGMAP.search(out_on)
    if not mm2:
      fails.append("%sENGAGE: the build line reports no per-eye registration map — the "
                   "asymmetric-frustum term is unobservable" % pfx)
    else:
      (lsx, lsy, ltx, lty, lpx, lpy,
       rsx, rsy, rtx, rty, rpx, rpy) = (float(v) for v in mm2.groups())
      print("%sLEG_ASYM inset<%.1fdeg> map L s %.4f,%.4f t %.4f,%.4f p %.4f,%.4f | "
            "R s %.4f,%.4f t %.4f,%.4f p %.4f,%.4f resid c0 %s"
            % (pfx, asym, lsx, lsy, ltx, lty, lpx, lpy,
               rsx, rsy, rtx, rty, rpx, rpy, c0), flush=True)
      biggest_t = max(abs(ltx), abs(rtx))
      if asym > 0.0 and biggest_t < 0.01:
        fails.append("%sASYM: the rig has %.1f degrees of per-eye frustum inset but the solved "
                     "at-infinity map is a translation of %.4f — the registration did not see the "
                     "asymmetry, so the reduction is charging it to the search window"
                     % (pfx, asym, biggest_t))
      if asym == 0.0 and biggest_t > 0.01:
        fails.append("%sASYM: symmetric rig solved a nonzero at-infinity translation (%.4f)"
                     % (pfx, biggest_t))
      # THE CAP IS ABSOLUTE, THE DISPLACEMENT IS RELATIVE. A residual constant that is a
      #  FRACTION of the pyramid width blows past the cap as soon as the pyramid is big
      #  (measured: 0.10 of the width = 13 texels at this 256px rig, 65 at a headset's
      #  1280px eye — under the cap here, far past it there). So the leg is scale-free:
      #  what survives at infinity belongs in the map, not in the window.
      resid_frac = float(c0) / max(1.0, float(bw))
      if resid_frac > 0.02:
        fails.append("%sASYM: the residual window constant is %s mip0 texels = %.3f of the "
                     "pyramid width — the reduction's cap does not scale with resolution, so a "
                     "constant this size publishes far on every texel at eye resolution and "
                     "stereo occlusion goes inert" % (pfx, c0, resid_frac))
      if float(c0) > 16.0:
        fails.append("%sASYM: the residual window constant is %s mip0 texels, past the reduction's "
                     "cap outright — every texel overflows and publishes far" % (pfx, c0))
  if REJECT.search(out_on) or REFUSE.search(out_on):
    fails.append("%sENGAGE: the build was rejected/refused — the pyramid never reached a consumer"
                 % pfx)

  s_on = rec_on.get("samples", [])
  if not s_on:
    fails.append("%sthe armed arm produced no samples" % pfx)
    return (-1, None)
  if not s_on[-1].get("enabled"):
    fails.append("%sCullStats was not armed — a zero count proves nothing" % pfx)
  if not s_on[-1].get("hyper_valid"):
    fails.append("%sthe hypermesh cull family never contributed — this scene's only pyramid "
                 "consumer did not run, so leg (b) would be measuring nothing" % pfx)

  occ_on = _median(s_on, "h_occluded")
  fru_on = _median(s_on, "h_frustum")
  vis_on = _median(s_on, "h_visible")
  print("%sLEG_CULLS occluded=%d visible=%d frustum_admitted=%d"
        % (pfx, occ_on, vis_on, fru_on), flush=True)
  if occ_on <= 0:
    fails.append("%sCULLS: h_occluded=%d with the pyramid on — it built but rejected nothing behind "
                 "a near fence" % (pfx, occ_on))
  if occ_on > fru_on:
    fails.append("%sBOTH-EYES-AGREE: h_occluded (%d) exceeds the frustum-admitted population (%d) — "
                 "an under-occluding combine cannot do that" % (pfx, occ_on, fru_on))

  # ---- leg (e) STABLE: a frozen pose must reject the same population every frame
  occ_seq = [s["h_occluded"] for s in s_on]
  uniq = sorted(set(occ_seq))
  print("%sLEG_STABLE frames=%d distinct h_occluded=%s" % (pfx, len(occ_seq), uniq), flush=True)
  if len(uniq) != 1:
    fails.append("%sSTABLE: h_occluded took %d distinct values %s across %d renders of ONE frozen "
                 "frame — a breathing occluded population is the flicker itself"
                 % (pfx, len(uniq), uniq, len(occ_seq)))

  # ---- arm B: same scene, pyramid disarmed
  rc_off, out_off, rec_off = _run_child(os.path.join(outdir, tag, "off"), disarm=True, msaa=msaa, asym=asym)
  if rc_off != 0:
    print(out_off, flush=True)
    fails.append("%sdisarmed child exited rc=%d — legs (c)/(f) have no B arm" % (pfx, rc_off))
  else:
    s_off = rec_off.get("samples", [])
    occ_off = _median(s_off, "h_occluded")
    fru_off = _median(s_off, "h_frustum")
    print("%sLEG_AB  disarmed: occluded=%d frustum_admitted=%d | armed: occluded=%d "
          "frustum_admitted=%d" % (pfx, occ_off, fru_off, occ_on, fru_on), flush=True)
    if ENGAGE.search(out_off):
      fails.append("%sA/B: %s=0 did not stop the build — the disarm knob does not disarm"
                   % (pfx, SPVR_HZB_ENV))
    if fru_off != fru_on:
      fails.append("%sA/B: h_frustum differs between the arms (%d vs %d) — the two arms are not the "
                   "same scene, so every other delta is uninterpretable" % (pfx, fru_off, fru_on))
    if occ_off != 0:
      fails.append("%sA/B: the disarmed arm reports h_occluded=%d — something occluded without a "
                   "pyramid" % (pfx, occ_off))

    # ---- leg (f) CONSERVATIVE: the armed render must be the disarmed render
    for nm in ("L", "R"):
      pa = rec_on.get("eye_%s" % nm)
      pb = rec_off.get("eye_%s" % nm)
      if not (pa and pb and os.path.exists(pa) and os.path.exists(pb)):
        fails.append("%sCONSERVATIVE: eye %s capture missing — the over-cull oracle did not run"
                     % (pfx, nm))
        continue
      eaten, worst = _eaten_pixels(pa, pb)
      print("%sLEG_CONSERVATIVE eye %s: eaten_px=%d worst_delta=%d" % (pfx, nm, eaten, worst),
            flush=True)
      if eaten != 0:
        fails.append("%sCONSERVATIVE: eye %s differs from the disarmed render in %d pixels (worst "
                     "channel delta %d) — the cull removed geometry that eye can see"
                     % (pfx, nm, eaten, worst))
  return (occ_on, None)


def main():
  outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
      tempfile.gettempdir(), "spvr_hzb_stereo_cull")
  os.makedirs(outdir, exist_ok=True)
  t0 = time.time()
  fails = []

  occ_by_tag = {}
  for (msaa, asym, tag) in MSAA_ARMS:
    occ, skip = _run_setting(outdir, msaa, asym, tag, fails)
    if skip:
      print("SKIP: %s" % skip, flush=True)
      print("test_spvr_hzb_stereo_cull: SKIP in %.1fs" % (time.time() - t0), flush=True)
      return 0
    occ_by_tag[tag] = occ
    if fails:
      break  # a broken setting makes every later measurement noise

  # ---- leg (g) REFUSED: no tracked pose -> the cull uses the desktop camera -> no pyramid
  rc_nt, out_nt, rec_nt = _run_child(os.path.join(outdir, "untracked"), tracked=False)
  if rc_nt != 0:
    print(out_nt, flush=True)
    fails.append("untracked child exited rc=%d — leg (g) has no arm" % rc_nt)
  else:
    s_nt = rec_nt.get("samples", [])
    occ_nt = _median(s_nt, "h_occluded")
    refused = bool(REFUSE.search(out_nt))
    print("LEG_REFUSED refused_line=%s occluded=%d" % (refused, occ_nt), flush=True)
    if not refused:
      fails.append("REFUSED: no refusal line with the cull on the desktop camera — the pyramid is "
                   "registered to the eye views and indexing it with an unrelated projection "
                   "removes visible geometry")
    if occ_nt != 0:
      fails.append("REFUSED: h_occluded=%d without the head camera — a refused build must leave "
                   "the culls frustum-only, including any pyramid from an earlier frame" % occ_nt)

  dt = time.time() - t0
  if fails:
    print("TESTVERDICT FAIL (%d): %s" % (len(fails), "; ".join(fails[:6])), flush=True)
    print("test_spvr_hzb_stereo_cull: FAIL in %.1fs" % dt, flush=True)
    return 1
  print("TESTVERDICT PASS -- single-pass stereo builds a REGISTERED occlusion pyramid from both "
        "eye layers with nothing set; at every antialiasing setting (%s) it rejects, rejects the "
        "same population every frame, eats zero pixels in either eye, and it refuses to publish "
        "when the cull would index it with the desktop camera (%.1fs)"
        % (", ".join("%s=%d occluded" % (k, v) for (k, v) in occ_by_tag.items()), dt), flush=True)
  return 0


if __name__ == "__main__":
  if len(sys.argv) > 2 and sys.argv[1] == "--render":
    sys.exit(_render(sys.argv[2],
                     tracked=(len(sys.argv) < 4 or sys.argv[3] == "tracked"),
                     msaa=(int(sys.argv[4]) if len(sys.argv) > 4 else 0),
                     asym=(float(sys.argv[5]) if len(sys.argv) > 5 else 0.0)))
  sys.exit(main())
