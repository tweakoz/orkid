#!/usr/bin/env ork.python
################################################################################
# NIGHT SH SURVIVAL gate (procsky wave4 slice W4-S8).
#
# THE CLAIM UNDER TEST: the faint sky of a moonless night survives all the way
# into the diffuse ambient's new carrier — the nine L2 coefficients of the sky
# SH probe — instead of flushing to zero somewhere between the capture and the
# shaded fragment.
#
# Sibling of test_night_ambient_floor_gate.py, which proves the same survival
# through the shaded receiver. This one reads the coefficients THEMSELVES, which
# is where the S8 swap put the ambient: a probe whose L0 is exactly zero lights
# nothing, and no amount of receiver-side tolerance can tell that apart from a
# very dark sky. Three independent facts are asserted about them.
#
#  NONZERO      L0 (the mean radiance term) and the L1 band (the direction term)
#               must be strictly positive / nonzero on every channel. fp16's
#               minimum normal is 6.1e-5 and a moonless night sky is around 1e-5
#               of a day sky, so this is exactly the quantity the capture
#               pre-scale exists to keep alive.
#
#  ROUND TRIP   the same night, captured at TWO gains 12.5x apart, must project to
#               the SAME decoded coefficients. The gain is an encode/decode pair
#               applied once each, so this needs no absolute calibration: it
#               fails if the decode is dropped, doubled, or applied at the wrong
#               end. It is also the fp16 proof — if the low-gain leg were
#               flushing, the two legs could not agree.
#
#  INTEGRAL     the engine's coefficients against a CPU projection of the very
#               snapshot they came from, decoded on the CPU side. Nothing here
#               carries a free gain, so a projector reading an unbound sampler
#               (the S7 trouble point: an unbound sampler reads ZERO in silence)
#               cannot pass by looking plausible.
#
# The receiver is still in the frame and still read, as the end-to-end witness
# that nonzero coefficients actually become light.
#
# WARMUP PINNED: ibl_crossfade_frames 0 (hard swap) and the fade weight is
# asserted at 1.0 before every read, so no read can catch a blend of two legs.
#
# THE FLOAT SURFACE — the recipe and its failure modes are documented in
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

# NAUTICAL TWILIGHT, the case that measured as exactly zero before the pre-scale.
SUN_ELEV_DEG = -12.0
SUN_AZIM_DEG = 0.0

BALL_POS = vec3(0, 2, 0)
CAM_EYE  = vec3(0, 2, -7.0)

# A DECLARED CADENCE rather than the sun-motion trigger: the sun never moves
# here, so an angle threshold would publish once and no property edit could ever
# reach a snapshot.
SNAPSHOT_INTERVAL = 0.05

# The round-trip partner's gain is deliberately NOT a power of two multiple of
# the shipped one. A x16 change only shifts an fp16 exponent — the mantissa, and
# therefore every decoded coefficient, comes back bit-identical and the check
# passes without ever exercising the quantization it exists to bound. 400/32 is
# 12.5x, so the two captures round differently and the agreement below is a
# measurement rather than an identity.
LEGS = [
    dict(key="night",    scale=None),   # the shipped gain
    dict(key="scale400", scale=400.0),  # 12.5x, the round-trip partner
]

# TOLERANCES, from the measurement.
#  the encode/decode pair is exact in float and rounds only in fp16, so a 12.5x
#  change in the gain must not move the DECODED coefficients by more than the
#  half-ulp of an fp16 mantissa accumulated through the projection.
ROUNDTRIP_TOL = 3.0e-2
#  engine coefficients vs a CPU projection of the same snapshot: what separates
#  them is float summation order, nothing structural.
COEFF_REL_TOL = 2.0e-2
#  the receiver must have received something. Not a calibration — a floor that
#  only a flush-to-zero can trip.
BALL_MIN      = 1.0e-9


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


def sh_reference(snap, capture_scale):
  """the nine L2 coefficients of the captured snapshot, integrated on the CPU.

  Same basis, same order and same grid as ProbeSHProjector's equirect kernel
  (probe_sh.h / probe_sh.cpp); the direction of a texel is the one
  sky.fxv2 ps_sky_equirect wrote there, x mirror included, and the capture
  PRE-SCALE is divided out here because the engine's coefficients are supposed
  to carry decoded radiance."""
  h, w, _ = snap.shape
  th = ((numpy.arange(h) + 0.5) / float(h)) * math.pi
  ph = ((numpy.arange(w) + 0.5) / float(w)) * 2.0 * math.pi - math.pi
  st = numpy.sin(th)[:, None]
  ct = numpy.cos(th)[:, None]
  dx = -(st * numpy.cos(ph)[None, :])
  dy = numpy.broadcast_to(ct, (h, w))
  dz = st * numpy.sin(ph)[None, :]
  dw = st * ((math.pi / float(h)) * (2.0 * math.pi / float(w)))
  L = snap[..., :3].astype(numpy.float64) * (dw / max(capture_scale, 1.0e-30))[..., None]
  basis = [
      numpy.full((h, w), 0.2820948),
      0.4886025 * dy,
      0.4886025 * dz,
      0.4886025 * dx,
      1.0925484 * dx * dy,
      1.0925484 * dy * dz,
      0.3153916 * (3.0 * dz * dz - 1.0),
      1.0925484 * dx * dz,
      0.5462742 * (dx * dx - dy * dy),
  ]
  return numpy.array([(L * b[..., None]).sum(axis=(0, 1)) for b in basis])


class FloatOutSGC(StandardSceneGraphComponent):
  """scenegraph that composites into an RGBA32F RtGroup (see the header)."""

  def _onGpuInit(self, ctx):
    rtg = lev2.RtGroup(ctx, WIDTH, HEIGHT)
    rtg.name = "nightsh_f32"
    rtg.createBuffer(tokens.RGBA32F, tokens.color)
    self.float_rtg = rtg
    self.sg_params.outputRTG = rtg
    super()._onGpuInit(ctx)


class NightSHApp(ComponentizedApplication):

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
    self._sh = {}
    self._scale = {}
    self.SGC = self.addComponent(
        "std_scenegraph", FloatOutSGC,
        eye=CAM_EYE, tgt=BALL_POS, up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          # NO SPECULAR and NO FLAT AMBIENT: the receiver must read the diffuse
          # ambient and nothing else, or another term would carry light the SH
          # probe never delivered.
          "SpecularIntensity": 0.0,
          "AmbientLevel":     vec3(0),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    # HARD SWAP (fleet rule for any gate reading lighting after an IBL publish):
    # the auto-sized fade window is hundreds of frames wide offscreen.
    self.atmo.ibl_crossfade_frames = 0
    self.atmo.ibl_snapshot_interval = SNAPSHOT_INTERVAL
    self._def_scale = float(self.atmo.ibl_capture_scale)
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    self.ball = SGC.createBallNode("recv", ctx=ctx, position=BALL_POS,
                                   color=vec4(1, 1, 1, 1), metallic=0.0,
                                   roughness=1.0, scale=1.6)

    # THE SUN IS DOWN and stays down, at zero intensity: this gate measures the
    # sky's own emission, and NO MOON is declared anywhere.
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
    print("[night-sh] %s" % txt, flush=True)

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
    print("[night-sh] captured %s %dx%d mean=%.6g min=%.6g max=%.6g" %
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

    if self._state == 0:                       # apply this leg's capture gain
      self.atmo.ibl_capture_scale = self._def_scale if leg["scale"] is None else leg["scale"]
      self._note("leg %s: capture_scale=%.6g" % (leg["key"], float(self.atmo.ibl_capture_scale)))
      self._gen_mark = int(self._pbr().sky_ibl_generation)
      self._restate(1)
      return

    if self._state == 1:                       # wait for TWO fresh publishes
      # two, not one: a cycle already in flight when the edit landed was fed the
      # PREVIOUS gain, so the first publish after an edit can still be stale.
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
          self._fail("leg %s: crossfade still running (fade weight %.6f) - the read "
                     "would blend two legs' probes" % (leg["key"], fw))
          return
        self._restate(3)
      return

    if self._state == 3:                       # read the probe, render, capture
      self.SGC.scenegraph.renderOnContext(ctx)
      if not self._realized:
        ctx.FBI.rtGroupInit(self.SGC.float_rtg)
        self._realized = True
        return
      p = self._pbr()
      if not bool(p.sky_sh_valid):
        self._fail("leg %s: the sky SH probe is not published - the diffuse ambient "
                   "is not coming from it and this gate has nothing to measure"
                   % leg["key"])
        return
      self._sh[leg["key"]] = numpy.array(
          [[c.x, c.y, c.z] for c in p.sky_sh_coefficients], dtype=numpy.float64)
      self._scale[leg["key"]] = float(self.atmo.ibl_capture_scale)
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

  ##############################################################

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    print("=== the night sky's coefficients ===", flush=True)
    for L in LEGS:
      key = L["key"]
      sh = self._sh[key]
      snap = self._shots["snap_" + key]
      l0 = sh[0]
      l1 = sh[1:4]
      print("  %-9s gain=%.1f  snapshot mean=%.6g max=%.6g" %
            (key, self._scale[key], float(snap.mean()), float(snap.max())), flush=True)
      print("           L0=<%.6g,%.6g,%.6g>  |L1|=%.6g" %
            (l0[0], l0[1], l0[2], float(numpy.linalg.norm(l1))), flush=True)
      check("L0_survives_%s" % key, bool((l0 > 0.0).all()),
            "L0=<%.6g,%.6g,%.6g> — every channel must be strictly positive; a zero "
            "here is the fp16 flush the capture pre-scale exists to prevent"
            % (l0[0], l0[1], l0[2]))
      check("L1_survives_%s" % key, float(numpy.linalg.norm(l1)) > 0.0,
            "|L1|=%.6g — the direction term must carry signal, not just the mean"
            % float(numpy.linalg.norm(l1)))

      want = sh_reference(snap, self._scale[key])
      dn = float(numpy.linalg.norm(want))
      rel = float(numpy.linalg.norm(sh - want)) / max(dn, 1e-30)
      print("           vs CPU projection: |engine|=%.6g |cpu|=%.6g rel=%.5f" %
            (float(numpy.linalg.norm(sh)), dn, rel), flush=True)
      check("coeffs_match_cpu_projection_%s" % key, rel < COEFF_REL_TOL,
            "rel=%.5f (tol %.3f) — a pure gain error here is the capture pre-scale "
            "decoding the wrong number of times" % (rel, COEFF_REL_TOL))

      ball = self._ballMean(self._shots["lit_" + key])
      print("           receiver mean=%.6g" % ball, flush=True)
      check("receiver_is_lit_%s" % key, ball > BALL_MIN,
            "mean=%.6g (min %.1e) — nonzero coefficients that light nothing are "
            "a broken reconstruction, not a dark night" % (ball, BALL_MIN))

    ##########################################################
    # THE ROUND TRIP. Two gains 16x apart, one sky. Nothing absolute is claimed:
    # what is claimed is that the encode and the decode cancel, exactly once
    # each, and that the dimmer capture did not lose the sky on the way.
    ##########################################################
    a = self._sh["night"]
    b = self._sh["scale400"]
    na = float(numpy.linalg.norm(a))
    rel = float(numpy.linalg.norm(a - b)) / max(na, 1e-30)
    print("=== pre-scale round trip (%.1f vs %.1f) ===" %
          (self._scale["night"], self._scale["scale400"]), flush=True)
    print("  |night|=%.6g |scale400|=%.6g  relative difference=%.5f" %
          (na, float(numpy.linalg.norm(b)), rel), flush=True)
    check("prescale_round_trip", rel < ROUNDTRIP_TOL,
          "rel=%.5f (tol %.3f) — a difference near %.1fx is the decode missing on "
          "one leg; a difference near 1 is the dim leg flushing to zero"
          % (rel, ROUNDTRIP_TOL, self._scale["scale400"] / max(self._scale["night"], 1.0)))

    ok = (len(failures) == 0)
    detail = ("L0_night=<%.6g,%.6g,%.6g> L0_scale400=<%.6g,%.6g,%.6g> "
              "roundtrip_rel=%.5f" %
              (a[0][0], a[0][1], a[0][2], b[0][0], b[0][1], b[0][2], rel))
    if failures:
      detail += " failed=" + ",".join(failures)
    verdict(ok, detail)                        # VERDICT BEFORE TEARDOWN
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = NightSHApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
