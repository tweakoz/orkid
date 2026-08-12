#!/usr/bin/env ork.python
################################################################################
# DAYLIGHT SKYLIGHT FRACTION gate (procsky wave4 slice W4-S7).
#
# THE CLAIM UNDER TEST: at midday a cast shadow is not black — it is lit by the
# rest of the sky, and by the RIGHT amount. This is the bright end of the night
# emission work: the same shared sample path now carries an always-on emission
# floor and the same captured maps now ride a pre-scale, and neither may move
# daylight, where the sun's own scattering is orders above both.
#
# THE OBSERVABLE, and why it is a ratio of the SAME pixels. A shadowed pixel and
# a lit pixel on a curved receiver have different surface normals, so comparing
# two different regions measures geometry as much as lighting. Instead each
# shadowed pixel is compared against ITSELF with the sun unoccluded:
#
#     skylight fraction = B / (B + D0)
#
# where B is the image-based (captured sky) term and D0 the direct sun the
# shadow removed, both read at the same pixels. That is exactly "how much of
# this surface's midday light came from the sky rather than the sun", which for
# a clear sky is a known physical number: sky irradiance on a sunlit horizontal
# surface runs roughly 10-25% of the total, the rest being the direct beam.
#
# THE CAPTURES, one static scene, one warm process, sun fixed throughout so a
# single published sky serves all of them (the harness is
# test_shadow_attribution_gate.py's, whose header works through why skyboxLevel
# is the knob that isolates the direct term):
#
#   envonly       sun intensity 0                -> B
#   sunonly       skyboxLevel 0, shadows on      -> D  (locates the umbra)
#   sunonly_nosh  skyboxLevel 0, shadows off     -> D0 (the beam that was removed)
#
# THE SUN'S INTENSITY IS DERIVED, not chosen. The band above is only meaningful
# if the direct sun and the sky come from the same star. The atmosphere presents
# its sky at _sunIlluminance x _skyExposure, attenuated by the medium, so the
# matching ground-level beam is _sunIlluminance x _skyExposure x the zenith
# transmittance. At the shipped defaults that is 1.0 x 4.0 x ~0.87 = 3.5, and
# that is what the directional light is set to. A scene free to pick any pair
# could land the ratio anywhere; this one is pinned to the engine's own numbers.
#
# WARMUP PINNED: ibl_crossfade_frames 0 and the fade weight asserted at 1.0
# before every capture.
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

# see the header: 1.0 (_sunIlluminance) x 4.0 (_skyExposure) x 0.87 (zenith
# transmittance of the shipped medium)
SUN_INTENSITY = 3.5
SUN_AZIM_DEG  = 0.0
SUN_ELEV_DEG  = 80.0      # midday, tilted just enough for the camera to see the shadow

RECEIVER_POS   = vec3(0, 0, 0)
RECEIVER_SCALE = 2.5
CASTER_SCALE   = 0.7
CASTER_LIFT    = 3.2

CAM_EYE = vec3(0, 3.5, -9.0)
CAM_TGT = vec3(0, 0.6, 0)

SURFACE_FLOOR    = 1.0e-4
DIRECT_HI        = 0.05
UMBRA_DIRECT_MAX = 1.0e-4
MIN_PIXELS       = 200

# THE PHYSICAL BAND. Clear-sky diffuse irradiance on a sunlit surface at midday.
SKYFRAC_LO = 0.10
SKYFRAC_HI = 0.25

LEGS = [
    dict(key="envonly",      sun=0.0,           skybox=1.0, caster=True),
    dict(key="sunonly",      sun=SUN_INTENSITY, skybox=0.0, caster=True),
    dict(key="sunonly_nosh", sun=SUN_INTENSITY, skybox=0.0, caster=False),
]


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class FloatOutSGC(StandardSceneGraphComponent):
  """scenegraph that composites into an RGBA32F RtGroup."""

  def _onGpuInit(self, ctx):
    rtg = lev2.RtGroup(ctx, WIDTH, HEIGHT)
    rtg.name = "skyfrac_f32"
    rtg.createBuffer(tokens.RGBA32F, tokens.color)
    self.float_rtg = rtg
    self.sg_params.outputRTG = rtg
    super()._onGpuInit(ctx)


class SkylightFractionApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._leg = 0
    self._state = 0
    self._state_frame = 0
    self._state_time = time.time()
    self._done = False
    self._exit_code = None
    self._realized = False
    self._inflight = None
    self._shots = {}
    self.SGC = self.addComponent(
        "std_scenegraph", FloatOutSGC,
        eye=CAM_EYE, tgt=CAM_TGT, up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          # no flat ambient: "the sky's share" must mean the captured IBL and
          # nothing else.
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
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    self.receiver = SGC.createBallNode("receiver", ctx=ctx, position=RECEIVER_POS,
                                       color=vec4(1, 1, 1, 1), metallic=0.0,
                                       roughness=0.85, scale=RECEIVER_SCALE)
    d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
    r_recv = float(min(SGC._ball_model.aabb_whd.x, SGC._ball_model.aabb_whd.y,
                       SGC._ball_model.aabb_whd.z)) * RECEIVER_SCALE
    cpos = vec3(d.x, d.y, d.z) * CASTER_LIFT + vec3(0, r_recv, 0)
    self.caster = SGC.createBallNode("caster", ctx=ctx, position=cpos,
                                     color=vec4(1, 1, 1, 1), metallic=0.0,
                                     roughness=0.85, scale=CASTER_SCALE)

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = SUN_INTENSITY
    sun.data.sky_body = 1
    sun.data.shadowBias = 0.05  # metres
    sun.data.shadowMapSize = 2048
    sun.data.shadowCascadeCount = 3
    sun.data.shadowMaxDistance = 250.0
    # PCF dither off: the same pixel is compared across captures, and a
    # per-fragment jitter that is not identical in all of them would land in
    # the ratio as if the shadow had moved.
    sun.data.pcfDither = 0.0
    sun.shadowCaster = True
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
    print("[skyfrac] %s" % txt, flush=True)

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
    print("[skyfrac] captured %s %dx%d mean=%.6g max=%.6g" %
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
      # ONE published cycle; the sun never moves afterwards, so every capture
      # shares one and the same sky.
      if bool(self._pbr().sky_ibl_ready) and (not bool(self._pbr().sky_ibl_inflight)):
        self._built = True
        self._restate(0)
      elif (time.time() - self._state_time) > WAIT_SECONDS:
        self._fail("the first procedural refilter never published")
      return
    if self._leg >= len(LEGS):
      return

    leg = LEGS[self._leg]

    if self._state == 0:
      self.sun.data.intensity = leg["sun"]
      self.sun.shadowCaster = leg["caster"]
      self._pbr().skyboxLevel = leg["skybox"]
      self._note("leg %s: sun %.2f, skyboxLevel %.2f, shadowCaster %s"
                 % (leg["key"], leg["sun"], leg["skybox"], leg["caster"]))
      self._restate(1)
      return

    if self._state == 1:
      if (self._frame - self._state_frame) >= SETTLE_FRAMES:
        fw = float(self._pbr().sky_ibl_fade_weight)
        if fw < 0.9999:
          self._fail("leg %s: crossfade still running (fade weight %.6f)" % (leg["key"], fw))
          return
        self._restate(2)
      return

    if self._state == 2:
      self.SGC.scenegraph.renderOnContext(ctx)
      if not self._realized:
        ctx.FBI.rtGroupInit(self.SGC.float_rtg)
        self._realized = True
        return
      self._issueCapture(ctx, leg["key"], self.SGC.float_rtg, "RGBA32F")
      self._restate(3)
      return

    if self._collect() is None:
      return
    self._leg += 1
    self._restate(0)
    if self._leg >= len(LEGS):
      self._emitVerdict()

  ##############################################################

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    have = all(L["key"] in self._shots for L in LEGS)
    check("captures_present", have)
    if not have:
      self._fail("missing captures")
      return

    B  = self._shots["envonly"]
    D  = self._shots["sunonly"]
    D0 = self._shots["sunonly_nosh"]

    lum = lambda im: im.mean(axis=2)
    bL, dL, d0L = lum(B), lum(D), lum(D0)

    ##########################################################
    # THE UMBRA — built from the direct-only captures alone, never from the
    # quantity under test: a surface pixel that received real beam with shadows
    # OFF and has essentially none of it left with shadows ON.
    ##########################################################
    surface = bL > SURFACE_FLOOR
    umbra = surface & (d0L > DIRECT_HI) & (dL < UMBRA_DIRECT_MAX)
    print("=== masks ===", flush=True)
    print("  surface=%d  umbra=%d  (of %d pixels)"
          % (int(surface.sum()), int(umbra.sum()), surface.size), flush=True)
    check("umbra_present", int(umbra.sum()) >= MIN_PIXELS,
          "%d px (min %d)" % (int(umbra.sum()), MIN_PIXELS))
    if int(umbra.sum()) < MIN_PIXELS:
      verdict(False, "umbra too small: %d px" % int(umbra.sum()))
      self._exit_code = 1
      self._done = True
      self.ezapp.signalExit()
      return

    sky    = float(bL[umbra].mean())
    beam   = float(d0L[umbra].mean())
    frac   = sky / max(sky + beam, 1.0e-30)

    print("=== midday light budget, same pixels, pre-tonemap linear radiance ===", flush=True)
    print("  captured sky term B=%.6g   removed sun beam D0=%.6g   sky fraction=%.4f"
          % (sky, beam, frac), flush=True)

    ##########################################################
    # THE SHADOW IS NOT BLACK — the floor half of the claim, and the one that
    # would have failed outright before the captured sky lit anything.
    ##########################################################
    check("shadow_is_skylit_not_black", float(bL[umbra].min()) > 0.0,
          "min sky term in the umbra=%.6g" % float(bL[umbra].min()))
    check("beam_is_real", beam > DIRECT_HI, "removed beam=%.6g" % beam)

    ##########################################################
    # AND IT IS THE RIGHT AMOUNT — the physical clear-sky band.
    ##########################################################
    check("skylight_fraction_in_physical_band",
          (frac > SKYFRAC_LO) and (frac < SKYFRAC_HI),
          "fraction=%.4f (band %.2f..%.2f)" % (frac, SKYFRAC_LO, SKYFRAC_HI))

    ok = (len(failures) == 0)
    detail = ("umbra_px=%d sky=%.6g beam=%.6g sky_fraction=%.4f"
              % (int(umbra.sum()), sky, beam, frac))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = SkylightFractionApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
