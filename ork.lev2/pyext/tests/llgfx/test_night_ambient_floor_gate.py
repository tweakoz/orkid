#!/usr/bin/env ork.python
################################################################################
# NIGHT AMBIENT FLOOR gate (procsky wave4 slice W4-S7).
#
# THE CLAIM UNDER TEST: with the sun below the horizon and no moon, the sky
# still delivers light to surfaces through the captured-IBL path — and it does
# so at magnitudes that survive an RGBA16F snapshot and an RGBA16F prefiltered
# map instead of flushing to zero.
#
# WHAT WAS BROKEN. The sky-view LUT is a SUN-ONLY scattering integral, so with
# the sun down it converges toward zero; whatever was left was then under the
# fp16 minimum normal (6.1e-5) and prefiltered to EXACTLY zero. Measured before
# this slice, at the sun 12 degrees below the horizon: sky peak near 1e-4, lit
# receiver exactly 0.0. Two independent fixes are needed and both are measured
# here — an EMISSION floor (airglow + starlight, the light a real moonless
# night sky actually carries) and a CAPTURE PRE-SCALE (the snapshot is written
# at a known gain and the gain is divided back out at every env read).
#
# THE FOUR LEGS, one static scene, one warm process, sun parked at -12 degrees
# throughout and no moon declared anywhere:
#
#   night        the shipped defaults.
#   scale512     ibl_capture_scale raised 16x. The gain is an ENCODE/DECODE
#                pair, so the shaded result must not move — this is the
#                round-trip proof, and it needs no absolute calibration.
#   no_airglow   airglow_intensity 0. Isolates the airglow contribution.
#   no_star      starlight_intensity 0, airglow restored. Isolates starlight.
#
# THE ORDERING ORACLE is the last two against the first: removing the airglow
# must cost far more light than removing the starlight, because a real moonless
# night sky is ~65% airglow and ~6% starlight. That is a RATIO of differences,
# so it is immune to whatever absolute scale the scene happens to run at.
#
# THE SHIPPED STATE STAYS ALWAYS-ON. Legs 3 and 4 zero a property at runtime to
# isolate a term; they are measurements, not a toggle. Nothing here can be
# reached without a scene deliberately writing a zero.
#
# WHY THE PREFILTERED MAP IS MEASURED THROUGH THE BALL. The prefiltered diffuse
# map is a Texture, and only RtBuffers are capturable from python. The
# lambertian receiver IS the readout: with ambient, specular and every analytic
# light at zero, its radiance is albedo x the diffuse env sample and nothing
# else, so a nonzero ball is a nonzero prefiltered map at those normals — the
# same quantity, read where it actually matters.
#
# WARMUP PINNED: ibl_crossfade_frames 0 (hard swap) and the fade weight is
# asserted at 1.0 before every capture, so no capture can read a blend of two
# legs' maps.
#
# THE FLOAT SURFACE — the recipe and its three failure modes are documented in
# test_env_hdr_range_gate.py, whose harness this gate reuses.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import math
import time
import numpy
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

tokens = CrcStringProxy()

WIDTH, HEIGHT = 512, 384
SETTLE_FRAMES = 24
WAIT_SECONDS  = 120.0

# NAUTICAL TWILIGHT, the case S3 measured flushing to exactly zero.
SUN_ELEV_DEG = -12.0
SUN_AZIM_DEG = 0.0

BALL_POS = vec3(0, 2, 0)
CAM_EYE  = vec3(0, 2, -7.0)

# A DECLARED CADENCE rather than the sun-motion trigger: the sun never moves in
# this gate, so an angle threshold would publish exactly once and no property
# edit could ever reach a snapshot. This makes cycles run back to back, which is
# what lets four legs each get their own published sky.
SNAPSHOT_INTERVAL = 0.05

LEGS = [
    dict(key="night",      scale=None, airglow=None, star=None),
    dict(key="scale512",   scale=512.0, airglow=None, star=None),
    dict(key="no_airglow", scale=None, airglow=0.0,  star=None),
    dict(key="no_star",    scale=None, airglow=None, star=0.0),
]

# TOLERANCES, from the measurement.
#  the encode/decode pair is exact in float and rounds only in fp16, so a
#  16x change in the gain must not move the shaded ball by more than the
#  half-ulp of an fp16 mantissa accumulated through the convolution.
ROUNDTRIP_TOL = 2.0e-2
#  airglow must cost at least this many times what starlight costs. Shipped
#  ratio is 10x; the bar is set well under it so the gate tests the ORDERING,
#  not the exact split.
ORDERING_MIN = 3.0


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class FloatOutSGC(StandardSceneGraphComponent):
  """scenegraph that composites into an RGBA32F RtGroup (see the header)."""

  def _onGpuInit(self, ctx):
    rtg = lev2.RtGroup(ctx, WIDTH, HEIGHT)
    rtg.name = "nightfloor_f32"
    rtg.createBuffer(tokens.RGBA32F, tokens.color)
    self.float_rtg = rtg
    self.sg_params.outputRTG = rtg
    super()._onGpuInit(ctx)


class NightFloorApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._leg = 0
    self._state = 0
    self._state_frame = 0
    self._state_time = time.time()
    self._gen_mark = 0
    self._done = False
    self._exit_code = None
    self._realized = False
    self._inflight = None
    self._shots = {}
    self.SGC = self.addComponent(
        "std_scenegraph", FloatOutSGC,
        eye=CAM_EYE, tgt=BALL_POS, up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          # NO SPECULAR: the receiver must read the DIFFUSE prefiltered map and
          # nothing else, or a mirror lobe would carry light the diffuse chain
          # never delivered.
          "SpecularIntensity": 0.0,
          # NO FLAT AMBIENT: a constant floor would pass this gate on a number
          # the sky never produced.
          "AmbientLevel":     vec3(0),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    # black background, so a surface pixel is identifiable by radiance alone
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    # HARD SWAP (fleet rule for any gate reading lighting after an IBL publish):
    # the auto-sized fade window is hundreds of frames wide offscreen.
    self.atmo.ibl_crossfade_frames = 0
    self.atmo.ibl_snapshot_interval = SNAPSHOT_INTERVAL
    self._def_scale   = float(self.atmo.ibl_capture_scale)
    self._def_airglow = float(self.atmo.airglow_intensity)
    self._def_star    = float(self.atmo.starlight_intensity)
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    self.ball = SGC.createBallNode("recv", ctx=ctx, position=BALL_POS,
                                   color=vec4(1, 1, 1, 1), metallic=0.0,
                                   roughness=1.0, scale=1.6)

    # THE SUN IS DOWN and stays down. Intensity zero as well: this gate measures
    # the sky's own emission, and a sun below the horizon that still lit
    # geometry directly would be measuring the analytic path instead.
    d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0
    sun.data.sky_body = 1
    sun.shadowCaster = False
    sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass

  ##############################################################

  def _pbr(self):
    return self.SGC.pbr_common

  def _note(self, txt):
    print("[night-floor] %s" % txt, flush=True)

  def _fail(self, why):
    verdict(False, why)
    self._exit_code = 1
    self._done = True
    self.ezapp.signalExit()

  def _issueCapture(self, ctx, key, rtg, fmt):
    buf = lev2.CaptureBuffer()
    fut = ctx.FBI.captureAsFormat(rtg.buffer(0), buf, fmt)
    self._inflight = (fut, buf, key, fmt)

  def _collect(self):
    fut, buf, key, fmt = self._inflight
    if not bool(fut.is_ready):
      return None
    w, h = buf.width, buf.height
    img = numpy.array(buf, dtype=numpy.float32).reshape(h, w, 4)[..., :3].copy()
    self._shots[key] = img.astype(numpy.float64)
    self._inflight = None
    print("[night-floor] captured %s %dx%d mean=%.6g min=%.6g max=%.6g" %
          (key, w, h, float(img.mean()), float(img.min()), float(img.max())), flush=True)
    return key

  def _restate(self, s):
    self._state = s
    self._state_frame = self._frame
    self._state_time = time.time()

  ##############################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if bool(self._pbr().sky_ibl_ready) and (not bool(self._pbr().sky_ibl_inflight)):
        self._built = True
        self._restate(0)
      elif (time.time() - self._state_time) > WAIT_SECONDS:
        self._fail("the first procedural refilter never published")
      return
    if self._leg >= len(LEGS):
      return

    leg = LEGS[self._leg]

    if self._state == 0:                       # apply this leg's atmosphere state
      self.atmo.ibl_capture_scale = self._def_scale if leg["scale"] is None else leg["scale"]
      self.atmo.airglow_intensity = self._def_airglow if leg["airglow"] is None else leg["airglow"]
      self.atmo.starlight_intensity = self._def_star if leg["star"] is None else leg["star"]
      self._note("leg %s: capture_scale=%.6g airglow=%.6g starlight=%.6g"
                 % (leg["key"], float(self.atmo.ibl_capture_scale),
                    float(self.atmo.airglow_intensity), float(self.atmo.starlight_intensity)))
      self._gen_mark = int(self._pbr().sky_ibl_generation)
      self._restate(1)
      return

    if self._state == 1:                       # wait for TWO fresh publishes
      # two, not one: a cycle already in flight when the edit landed was fed the
      # PREVIOUS state, so the first publish after an edit can still be stale.
      if (int(self._pbr().sky_ibl_generation) - self._gen_mark) >= 2:
        if not bool(self._pbr().sky_ibl_inflight):
          self._restate(2)
      elif (time.time() - self._state_time) > WAIT_SECONDS:
        self._fail("leg %s: no fresh refilter published" % leg["key"])
      return

    if self._state == 2:                       # settle
      if (self._frame - self._state_frame) >= SETTLE_FRAMES:
        fw = float(self._pbr().sky_ibl_fade_weight)
        if fw < 0.9999:
          self._fail("leg %s: crossfade still running (fade weight %.6f) - the "
                     "capture would blend two legs' maps" % (leg["key"], fw))
          return
        self._restate(3)
      return

    if self._state == 3:                       # render float + capture the frame
      self.SGC.scenegraph.renderOnContext(ctx)
      if not self._realized:
        ctx.FBI.rtGroupInit(self.SGC.float_rtg)
        self._realized = True
        return
      self._issueCapture(ctx, "lit_" + leg["key"], self.SGC.float_rtg, "RGBA32F")
      self._restate(4)
      return

    if self._state == 4:                       # collect, then capture the source
      if self._collect() is None:
        return
      rtg = self._pbr().sky_ibl_snapshot_rtgroup
      if rtg is None:
        self._fail("leg %s: no equirect snapshot RTG after a published cycle" % leg["key"])
        return
      self._issueCapture(ctx, "snap_" + leg["key"], rtg, "RGBA32F")
      self._restate(5)
      return

    if self._collect() is None:                # 5: collect, advance
      return
    self._leg += 1
    self._restate(0)
    if self._leg >= len(LEGS):
      self._emitVerdict()

  ##############################################################

  def _ballMean(self, img):
    """mean linear radiance over the lambertian receiver, projected through the
    ENGINE's camera rather than guessed from the framing."""
    h, w, _ = img.shape
    ndc = self.SGC.camera.project(float(w) / float(h), BALL_POS)
    cx = (ndc.x * 0.5 + 0.5) * w
    cy = (ndc.y * 0.5 + 0.5) * h
    half = int(h * 0.09)
    r0, r1 = max(int(cy) - half, 0), min(int(cy) + half, h)
    c0, c1 = max(int(cx) - half, 0), min(int(cx) + half, w)
    return float(img[r0:r1, c0:c1].mean())

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    have = all(("lit_" + L["key"]) in self._shots and ("snap_" + L["key"]) in self._shots
               for L in LEGS)
    check("captures_present", have)
    if not have:
      self._fail("missing captures")
      return

    snap_n = self._shots["snap_night"]
    lit = {L["key"]: self._ballMean(self._shots["lit_" + L["key"]]) for L in LEGS}

    ##########################################################
    # fp16 SURVIVAL, capture side: EVERY texel of the night snapshot carries
    # energy. A minimum of zero anywhere is the flush this slice removes.
    ##########################################################
    print("=== source (equirect night snapshot, RGBA16F, PRE-SCALED) ===", flush=True)
    snap_min = float(snap_n.min())
    snap_max = float(snap_n.max())
    print("  night  min=%.6g  max=%.6g  mean=%.6g"
          % (snap_min, snap_max, float(snap_n.mean())), flush=True)
    check("snapshot_night_all_texels_nonzero", snap_min > 0.0, "min=%.6g" % snap_min)
    # fp16's minimum NORMAL. Below it the format is subnormal and the prefilter
    # accumulation is where the zeros came from.
    check("snapshot_night_above_fp16_min_normal", snap_min > 6.1e-5,
          "min=%.6g (bar 6.1e-05)" % snap_min)
    check("snapshot_night_under_fp16_max", snap_max < 65504.0, "max=%.6g" % snap_max)

    ##########################################################
    # fp16 SURVIVAL, shaded side: the prefiltered DIFFUSE map delivered light.
    ##########################################################
    print("=== lit receiver, pre-tonemap linear radiance ===", flush=True)
    for L in LEGS:
      print("  %-11s ball_mean=%.6g" % (L["key"], lit[L["key"]]), flush=True)
    check("night_ball_nonzero", lit["night"] > 0.0, "ball_mean=%.6g" % lit["night"])

    ##########################################################
    # PRE-SCALE ROUND TRIP: 16x more gain at the capture, same light at the
    # surface. The encode and the decode are one pair or this number moves.
    ##########################################################
    rel = abs(lit["scale512"] - lit["night"]) / max(lit["night"], 1.0e-30)
    print("=== capture pre-scale round trip ===", flush=True)
    print("  scale %.6g -> ball %.6g ;  scale %.6g -> ball %.6g ;  rel_delta=%.4g"
          % (self._def_scale, lit["night"], 512.0, lit["scale512"], rel), flush=True)
    check("prescale_roundtrip_cancels", rel < ROUNDTRIP_TOL,
          "rel_delta=%.4g (tol %.3g)" % (rel, ROUNDTRIP_TOL))

    ##########################################################
    # ORDERING: airglow is the dominant term, starlight the minor one.
    ##########################################################
    d_air  = lit["night"] - lit["no_airglow"]
    d_star = lit["night"] - lit["no_star"]
    ratio  = d_air / max(d_star, 1.0e-30)
    print("=== term isolation ===", flush=True)
    print("  airglow contributes %.6g   starlight contributes %.6g   ratio=%.4g"
          % (d_air, d_star, ratio), flush=True)
    check("airglow_contributes", d_air > 0.0, "delta=%.6g" % d_air)
    check("starlight_contributes", d_star > 0.0, "delta=%.6g" % d_star)
    check("airglow_dominates_starlight", ratio > ORDERING_MIN,
          "ratio=%.4g (bar %.3g)" % (ratio, ORDERING_MIN))

    ok = (len(failures) == 0)
    detail = ("snap_min=%.6g night_ball=%.6g roundtrip_rel=%.4g airglow=%.6g "
              "starlight=%.6g ordering=%.4g"
              % (snap_min, lit["night"], rel, d_air, d_star, ratio))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = NightFloorApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
