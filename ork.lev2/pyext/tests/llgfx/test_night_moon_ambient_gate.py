#!/usr/bin/env ork.python
################################################################################
# MOONLIGHT AMBIENT gate (procsky wave4 slice W4-S7).
#
# THE CLAIM UNDER TEST: a declared moon lights shadowed surfaces THROUGH THE
# CAPTURED-IBL PATH, and it does so in proportion to the moon light's own
# intensity — so a scene that scales its moon by lunar phase gets an ambient
# that waxes and wanes with it, for free.
#
# WHAT MAKES THIS NON-TRIVIAL. Before this slice the moon reached the sky as a
# DIRECTION only: the visible disc was drawn, and nothing else about the moon
# was known to the atmosphere. The sky-view LUT integrates the SUN, so with the
# sun down the sky it produced was the same whether the moon was overhead or on
# the other side of the planet. What is added is Rayleigh single-scatter of the
# moon's illuminance, read off the SkyBody==2 directional light (its colour
# times its intensity, which is where phase scaling lives).
#
# THE DECOMPOSITION, which is what makes this a measurement of the CAPTURE PATH
# rather than of the moon light itself. The same moon that brightens the sky is
# also an ordinary directional light hitting the same geometry directly, and
# that direct term would dominate any raw comparison. So every leg is captured
# TWICE, differing only in skyboxLevel:
#
#     skyboxLevel 1 -> env + direct        skyboxLevel 0 -> direct alone
#
# and their difference is the image-based term, isolated. skyboxLevel is the
# right knob and the only one: pbrEnvironmentLightingWithF0 scales both its
# channels by it while the analytic lobes are untouched (the argument is worked
# through in test_shadow_attribution_gate.py).
#
# THE THREE LEGS, one static scene, one warm process, sun parked below the
# horizon at zero intensity throughout:
#
#   moon_down   moon 20 degrees BELOW the horizon. The moon term is gated off
#               there, so this leg is the airglow + starlight floor and serves
#               as the zero of the differential.
#   moon_up     moon 45 degrees UP at unit illuminance.
#   moon_up_4x  the same moon at 4x the intensity.
#
# TWO ORACLES:
#   * DIRECTION — env(moon_up) > env(moon_down). A moon below the horizon
#     scatters nothing, and this is the whole claim in one inequality.
#   * PROPORTIONALITY — the moon's share of the env term, (env - env_down),
#     must grow 4x when the moon light does. Rayleigh single-scatter is exactly
#     linear in source illuminance, so this is a strong oracle and it needs no
#     absolute calibration: it is a ratio of differences.
#
# WARMUP PINNED: ibl_crossfade_frames 0 and the fade weight asserted at 1.0
# before every capture, so no capture blends two legs' maps.
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

SUN_ELEV_DEG = -12.0     # nautical twilight; the sun contributes no direct light
MOON_AZIM_DEG = 90.0     # off the sun's meridian, so the two are unambiguous
MOON_INTENSITY = 1.0     # a UNIT-illuminance moon, the anchor the shipped
                         # moon_rayleigh_strength default is derived against

BALL_POS = vec3(0, 2, 0)
CAM_EYE  = vec3(0, 2, -7.0)

SNAPSHOT_INTERVAL = 0.05   # declared cadence; see test_night_ambient_floor_gate

LEGS = [
    dict(key="moon_down",  elev=-20.0, intensity=MOON_INTENSITY),
    dict(key="moon_up",    elev= 45.0, intensity=MOON_INTENSITY),
    dict(key="moon_up_4x", elev= 45.0, intensity=MOON_INTENSITY * 4.0),
]

# the 4x oracle, allowed either way. Wide enough for fp16 quantization of the
# maps and for the horizon-airmass floor in the scatter term, tight enough that
# a constant (moon-independent) contribution could not pass it.
LINEARITY_TOL = 1.35


def celestial_dir(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the body (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class FloatOutSGC(StandardSceneGraphComponent):
  """scenegraph that composites into an RGBA32F RtGroup."""

  def _onGpuInit(self, ctx):
    rtg = lev2.RtGroup(ctx, WIDTH, HEIGHT)
    rtg.name = "moonambient_f32"
    rtg.createBuffer(tokens.RGBA32F, tokens.color)
    self.float_rtg = rtg
    self.sg_params.outputRTG = rtg
    super()._onGpuInit(ctx)


class MoonAmbientApp(ComponentizedApplication):

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
    self.atmo.ibl_crossfade_frames = 0
    self.atmo.ibl_snapshot_interval = SNAPSHOT_INTERVAL
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    self.ball = SGC.createBallNode("recv", ctx=ctx, position=BALL_POS,
                                   color=vec4(1, 1, 1, 1), metallic=0.0,
                                   roughness=1.0, scale=1.6)

    # THE SUN, down and dark. It is still DECLARED, and at the higher priority,
    # so the sky-view LUT's sun pick is unambiguous — the moon must never be
    # mistaken for it (the moon is picked by DECLARATION, never by rank).
    d = celestial_dir(0.0, SUN_ELEV_DEG)
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0
    sun.data.priority = 1.0
    sun.data.sky_body = 1
    sun.shadowCaster = False
    sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    # THE MOON. Declared by SkyBody, ranked below the sun.
    moon = lev2.DynamicDirectionalLight()
    moon.data.color = vec3(1, 1, 1)
    moon.data.intensity = MOON_INTENSITY
    moon.data.priority = 0.0
    moon.data.sky_body = 2
    moon.shadowCaster = False
    self.moon = moon
    self.moon_node = SGC.layer_fwd.createLightNode("moon", moon)
    self._aimMoon(LEGS[0]["elev"])

    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _aimMoon(self, elev_deg):
    m = celestial_dir(MOON_AZIM_DEG, elev_deg)
    # DirectionalLight::direction() is the TRAVEL direction, so aiming from the
    # body's position at the origin makes the direction TOWARD it its negation.
    self.moon.lookAt(vec3(m.x, m.y, m.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))

  def _onUpdate(self, updinfo):
    pass

  ##############################################################

  def _pbr(self):
    return self.SGC.pbr_common

  def _note(self, txt):
    print("[moon-ambient] %s" % txt, flush=True)

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
    print("[moon-ambient] captured %s %dx%d mean=%.6g max=%.6g" %
          (key, w, h, float(img.mean()), float(img.max())), flush=True)
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

    if self._state == 0:                       # apply this leg's moon state
      self.moon.data.intensity = leg["intensity"]
      self._aimMoon(leg["elev"])
      self._pbr().skyboxLevel = 1.0
      self._note("leg %s: moon elevation %.1f deg, intensity %.3f"
                 % (leg["key"], leg["elev"], leg["intensity"]))
      self._gen_mark = int(self._pbr().sky_ibl_generation)
      self._restate(1)
      return

    if self._state == 1:                       # wait for TWO fresh publishes
      # two, because a cycle already in flight when the edit landed carried the
      # PREVIOUS moon into its snapshot.
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
          self._fail("leg %s: crossfade still running (fade weight %.6f)" % (leg["key"], fw))
          return
        self._restate(3)
      return

    if self._state == 3:                       # env + direct
      self.SGC.scenegraph.renderOnContext(ctx)
      if not self._realized:
        ctx.FBI.rtGroupInit(self.SGC.float_rtg)
        self._realized = True
        return
      self._issueCapture(ctx, "both_" + leg["key"], self.SGC.float_rtg, "RGBA32F")
      self._restate(4)
      return

    if self._state == 4:                       # collect, switch to direct-only
      if self._collect() is None:
        return
      self._pbr().skyboxLevel = 0.0
      self._restate(5)
      return

    if self._state == 5:                       # settle the level change
      if (self._frame - self._state_frame) >= SETTLE_FRAMES:
        self._restate(6)
      return

    if self._state == 6:                       # direct alone
      self.SGC.scenegraph.renderOnContext(ctx)
      self._issueCapture(ctx, "direct_" + leg["key"], self.SGC.float_rtg, "RGBA32F")
      self._restate(7)
      return

    if self._collect() is None:                # 7: collect, advance
      return
    self._leg += 1
    self._restate(0)
    if self._leg >= len(LEGS):
      self._emitVerdict()

  ##############################################################

  def _ballMean(self, img):
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

    have = all(("both_" + L["key"]) in self._shots and ("direct_" + L["key"]) in self._shots
               for L in LEGS)
    check("captures_present", have)
    if not have:
      self._fail("missing captures")
      return

    env = {}
    print("=== decomposition, pre-tonemap linear radiance on the receiver ===", flush=True)
    for L in LEGS:
      k = L["key"]
      b = self._ballMean(self._shots["both_" + k])
      d = self._ballMean(self._shots["direct_" + k])
      env[k] = b - d
      print("  %-11s both=%.6g  direct=%.6g  env=%.6g" % (k, b, d, env[k]), flush=True)

    ##########################################################
    # DIRECTION: a moon above the horizon brightens the captured sky.
    ##########################################################
    check("env_floor_positive_with_moon_down", env["moon_down"] > 0.0,
          "env=%.6g" % env["moon_down"])
    check("moon_up_raises_captured_ambient", env["moon_up"] > env["moon_down"],
          "up=%.6g  down=%.6g" % (env["moon_up"], env["moon_down"]))

    ##########################################################
    # PROPORTIONALITY: the moon's SHARE of the env term is linear in its
    # illuminance, which is what makes phase scaling work.
    ##########################################################
    share_1x = env["moon_up"] - env["moon_down"]
    share_4x = env["moon_up_4x"] - env["moon_down"]
    ratio = share_4x / max(share_1x, 1.0e-30)
    print("=== proportionality ===", flush=True)
    print("  moon share at 1x=%.6g  at 4x=%.6g  ratio=%.4g (expected 4)"
          % (share_1x, share_4x, ratio), flush=True)
    check("moon_share_positive", share_1x > 0.0, "share=%.6g" % share_1x)
    check("moon_share_tracks_intensity",
          (ratio > 4.0 / LINEARITY_TOL) and (ratio < 4.0 * LINEARITY_TOL),
          "ratio=%.4g (allowed %.3g..%.3g)" % (ratio, 4.0 / LINEARITY_TOL, 4.0 * LINEARITY_TOL))

    ok = (len(failures) == 0)
    detail = ("env_down=%.6g env_up=%.6g env_up4=%.6g share1x=%.6g share4x=%.6g ratio=%.4g"
              % (env["moon_down"], env["moon_up"], env["moon_up_4x"],
                 share_1x, share_4x, ratio))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = MoonAmbientApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
