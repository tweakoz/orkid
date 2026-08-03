#!/usr/bin/env ork.python
################################################################################
# SHADOW ATTRIBUTION probe (procsky wave4 slice W4-S3, part B).
#
# THE CLAIM UNDER TEST, in one line: a sun shadow must remove the SUN and
# nothing else. Where a fragment is fully shadowed, the light it still carries
# must be exactly the light it would carry with the sun switched off — the
# image-based (env/IBL) contribution, intact and unattenuated.
#
# Static reading says this holds. In fwdtools.i2 the shadow factor is built once
# (_sun_shadow_factor x _sun_cookie_factor) and multiplied into sun_lighting's
# diffuse and specular only; pbrEnvironmentLightingWithF0 never sees it, and the
# final composite sums env_lighting + point + spot + sun with no shadow term of
# its own. This gate is the NUMERIC confirmation of that reading, because a
# reading of shader source is not a measurement of a shipped binary.
#
# HOW IT IS MEASURED — four captures of ONE static scene in one warm process,
# differing only in which light path is switched on:
#
#   envonly       sun intensity 0.               -> B, the image-based term alone
#   sunonly       sun on, skyboxLevel 0.         -> D, the direct term alone
#   sunonly_nosh  sunonly with shadowCaster off. -> D0, direct with NO occlusion
#   both          everything on.                 -> A, the shipped frame
#
# skyboxLevel is the knob that isolates the direct term, and it has to be that
# one: pbrEnvironmentLightingWithF0 scales BOTH its returned channels by
# SkyboxLevel, while DiffuseLevel and SpecularLevel also multiply the sun's own
# diffuse and specular lobes — zeroing those would have removed the very term
# this gate is trying to look at. Nothing moves between captures: the sun never
# changes DIRECTION (so no IBL refilter is triggered and every capture shares
# one published sky), the geometry is fixed, and the four frames are therefore
# comparable PIXEL BY PIXEL.
#
# THE MASKS ARE INDEPENDENT OF THE VERDICT. A cast-shadow pixel is defined
# without reference to A or B: it is a surface pixel that received real direct
# light with shadows OFF (D0 high) and lost it with shadows ON (D collapsed).
# That is a statement about the shadow map alone, so using it to select where to
# compare A against B is not circular. It also separates a genuine cast shadow
# from a fragment that is merely facing away from the sun, where the direct term
# is zero for reasons that have nothing to do with shadowing.
#
# THE VERDICT then has two halves, and both are needed:
#   * in the cast shadow, A == B (the shadow removed the direct term exactly,
#     and removed nothing else — a shadow that also attenuated the env would
#     show up here as A < B);
#   * in the lit region, A == B + D (direct and image-based light superpose, no
#     cross-term), and D is large — which is what makes the first half a real
#     measurement rather than a comparison of two zeros.
#
# READ-ONLY BY MANDATE: this probe measures the shipped shadow path and does not
# touch it. Everything it changes is a light level or an intensity, through the
# same public properties any application uses.
#
# THE FLOAT SURFACE. Pre-tonemap linear readback through the scenegraph's own
# compositor into an RGBA32F outputRTG; the recipe and its three failure modes
# are documented in test_env_hdr_range_gate.py, whose harness this gate reuses.
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
WAIT_SECONDS  = 90.0

SUN_INTENSITY = 6.0
SUN_AZIM_DEG  = 0.0
SUN_ELEV_DEG  = 55.0

RECEIVER_POS   = vec3(0, 0, 0)
RECEIVER_SCALE = 2.5
CASTER_SCALE   = 0.7
CASTER_LIFT    = 3.2      # along the direction of the sun, from the receiver's top

CAM_EYE = vec3(0, 3.5, -9.0)
CAM_TGT = vec3(0, 0.6, 0)

# masks, all in linear radiance on the pre-tonemap surface
SURFACE_FLOOR = 1.0e-4    # above this the pixel is lit geometry, not background
DIRECT_HI     = 0.05      # direct radiance that counts as "really lit by the sun"
SHADOW_FRAC   = 0.05      # a cast shadow keeps under this fraction of its direct light
# UMBRA: a cast-shadow pixel whose surviving direct radiance is below this in
# ABSOLUTE terms. The distinction matters — SHADOW_FRAC admits the penumbra,
# where PCF leaves a few parts in a thousand of the sun standing, and "the
# shadowed fragment reads the env-only value" is only literally true where
# nothing of the sun is left. Both regions are measured, for different claims.
UMBRA_DIRECT_MAX = 1.0e-4
MIN_PIXELS    = 200       # per mask, or the probe had nothing to measure

# TOLERANCES — set from measurement, see _emitVerdict.
ENV_UNTOUCHED_TOL = 1.0e-5   # |(A-D)-B|/B over the whole cast shadow
UMBRA_TOL         = 5.0e-3   # |A-B|/B in the umbra
SUPERPOS_TOL      = 1.0e-5   # |A-(B+D)|/A in the lit region


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class FloatOutSGC(StandardSceneGraphComponent):
  """scenegraph that composites into an RGBA32F RtGroup (see the header)."""

  def _onGpuInit(self, ctx):
    rtg = lev2.RtGroup(ctx, WIDTH, HEIGHT)
    rtg.name = "shadowattrib_f32"
    rtg.createBuffer(tokens.RGBA32F, tokens.color)
    self.float_rtg = rtg
    self.sg_params.outputRTG = rtg
    super()._onGpuInit(ctx)


# The four captures. Each entry is (key, sun intensity, skyboxLevel, shadowCaster).
LEGS = [
    dict(key="envonly",      sun=0.0,           skybox=1.0, caster=True),
    dict(key="sunonly",      sun=SUN_INTENSITY, skybox=0.0, caster=True),
    dict(key="sunonly_nosh", sun=SUN_INTENSITY, skybox=0.0, caster=False),
    dict(key="both",         sun=SUN_INTENSITY, skybox=1.0, caster=True),
]


class ShadowAttributionApp(ComponentizedApplication):

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
          # no flat ambient: "the env term" must mean the IBL and nothing else,
          # or the attribution check would pass on a constant that no shadow
          # could ever have touched.
          "AmbientLevel":     vec3(0),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    # NO SKY ON SCREEN: the background must stay black so a surface pixel is
    # identifiable by radiance alone.
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    # HARD SWAP, no crossfade (fleet rule for any gate reading lighting after an
    # IBL publish): the auto-sized fade window is hundreds of frames wide at
    # offscreen frame rates.
    self.atmo.ibl_crossfade_frames = 0
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    self.receiver = SGC.createBallNode("receiver", ctx=ctx, position=RECEIVER_POS,
                                       color=vec4(1, 1, 1, 1), metallic=0.0,
                                       roughness=0.85, scale=RECEIVER_SCALE)
    # the occluder rides on SGC.fwd_layers (createBallNode's own layer set), which
    # includes the depth prepass — a drawable that does not play the prepass role
    # casts no sun shadow at all.
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
    sun.data.shadowBias = 2e-4
    sun.data.shadowMapSize = 2048
    sun.data.shadowCascadeCount = 3
    sun.data.shadowMaxDistance = 250.0
    # PCF DITHER OFF. The dither is a per-fragment jitter of the shadow lookup,
    # and this gate compares the SAME pixel across four captures — a jitter that
    # is not identical in all four would land in the attribution residual as if
    # the shadow had touched the env term.
    sun.data.pcfDither = 0.0
    sun.shadowCaster = True
    # DirectionalLight::direction() is the light's TRAVEL direction, so aiming it
    # from the sun's position at the origin makes dir_to_sun its negation.
    sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass   # fully static: the only differences between captures are the ones we make

  ##############################################################

  def _pbr(self):
    return self.SGC.pbr_common

  def _note(self, txt):
    print("[shadow-attrib] %s" % txt, flush=True)

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
    print("[shadow-attrib] captured %s %dx%d mean=%.6g max=%.6g" %
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
      # PRIME: one published procedural refilter before anything is measured.
      # The sun never moves afterwards, so this is the ONLY cycle that ever
      # runs and all four captures share one and the same sky.
      if bool(self._pbr().sky_ibl_ready) and (not bool(self._pbr().sky_ibl_inflight)):
        self._built = True
        self._restate(0)
      elif (time.time() - self._state_time) > WAIT_SECONDS:
        self._fail("the first procedural refilter never published")
      return
    if self._leg >= len(LEGS):
      return

    leg = LEGS[self._leg]

    if self._state == 0:                       # apply this leg's light state
      self.sun.data.intensity = leg["sun"]
      self.sun.shadowCaster = leg["caster"]
      self._pbr().skyboxLevel = leg["skybox"]
      self._note("leg %s: sun intensity %.2f, skyboxLevel %.2f, shadowCaster %s"
                 % (leg["key"], leg["sun"], leg["skybox"], leg["caster"]))
      self._restate(1)
      return

    if self._state == 1:                       # settle
      if (self._frame - self._state_frame) >= SETTLE_FRAMES:
        self._restate(2)
      return

    if self._state == 2:                       # render float + capture
      self.SGC.scenegraph.renderOnContext(ctx)
      if not self._realized:
        # REALIZE the target: captureAsFormat asserts natively on an unbuilt
        # RtBuffer impl rather than raising.
        ctx.FBI.rtGroupInit(self.SGC.float_rtg)
        self._realized = True
        return
      self._issueCapture(ctx, leg["key"], self.SGC.float_rtg, "RGBA32F")
      self._restate(3)
      return

    if self._collect() is None:                # 3: collect, advance
      return
    self._leg += 1
    self._restate(0)
    if self._leg >= len(LEGS):
      self._emitVerdict()

  ##############################################################

  def _emitVerdict(self):
    ##########################################################
    # TOLERANCES. Both are relative residuals between captures of a STATIC scene
    # on a float surface, so the honest expectation is float rounding, not a
    # perceptual bound — the arithmetic differs only in which terms the shader
    # summed. They are set at 2e-3, which is loose enough to absorb the
    # accumulation order changing between a frame that adds an env term and one
    # that does not, and still two orders of magnitude tighter than any real
    # attribution defect: a shadow that also attenuated the env term would push
    # the first residual to the size of the shadow factor itself.
    ##########################################################
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

    A  = self._shots["both"]           # env + direct, shadowed
    B  = self._shots["envonly"]        # env alone
    D  = self._shots["sunonly"]        # direct alone, shadowed
    D0 = self._shots["sunonly_nosh"]   # direct alone, no occlusion

    lum = lambda im: im.mean(axis=2)
    bL, dL, d0L = lum(B), lum(D), lum(D0)

    ##########################################################
    # masks — built from B/D/D0 only, never from A (see the header)
    ##########################################################
    surface = bL > SURFACE_FLOOR
    castshadow = surface & (d0L > DIRECT_HI) & (dL < SHADOW_FRAC * d0L)
    lit = surface & (dL > DIRECT_HI)

    print("=== masks ===", flush=True)
    print("  surface=%d  cast_shadow=%d  lit=%d  (of %d pixels)"
          % (int(surface.sum()), int(castshadow.sum()), int(lit.sum()), surface.size),
          flush=True)
    check("cast_shadow_present", int(castshadow.sum()) >= MIN_PIXELS,
          "%d px (min %d)" % (int(castshadow.sum()), MIN_PIXELS))
    check("lit_region_present", int(lit.sum()) >= MIN_PIXELS,
          "%d px (min %d)" % (int(lit.sum()), MIN_PIXELS))
    if int(castshadow.sum()) < MIN_PIXELS or int(lit.sum()) < MIN_PIXELS:
      verdict(False, "masks too small: cast_shadow=%d lit=%d"
              % (int(castshadow.sum()), int(lit.sum())))
      self._exit_code = 1
      self._done = True
      self.ezapp.signalExit()
      return

    ##########################################################
    # what the shadow actually removed — makes the attribution check a real
    # measurement instead of a comparison of two zeros
    ##########################################################
    removed = float(d0L[castshadow].mean())
    residual = float(dL[castshadow].mean())
    print("=== what the shadow removed (cast-shadow region) ===", flush=True)
    print("  direct radiance with shadows OFF=%.6g   with shadows ON=%.6g   "
          "removed fraction=%.4f" % (removed, residual, 1.0 - residual / max(removed, 1e-30)),
          flush=True)
    check("shadow_removed_real_direct_light", removed > DIRECT_HI,
          "direct-with-shadows-off mean=%.6g (min %.3g)" % (removed, DIRECT_HI))

    ##########################################################
    # THE ATTRIBUTION VERDICT, in the form that survives a penumbra.
    #
    # The literal claim — "a shadowed fragment reads the env-only value" — is
    # only exactly true where the shadow is FULL. PCF leaves a few parts in a
    # thousand of the sun standing along the shadow's edge, and a check written
    # as A == B over the whole cast shadow would fail on that surviving direct
    # light while learning nothing about attribution (measured: |A-B|/B up to
    # 3.0e-1 at the edge, all of it the penumbra's own sunlight).
    #
    # The claim that holds EVERYWHERE, penumbra included, is the one actually
    # under test: the shadow scaled the direct term and left the env term alone.
    # Subtract each pixel's own direct term from the shipped frame and what
    # remains must be the env-only frame, whatever fraction of the sun the
    # shadow happened to leave. That is asserted over the whole cast shadow; the
    # literal form is asserted separately over the umbra, where it is meaningful.
    ##########################################################
    a_s = lum(A)[castshadow]
    b_s = bL[castshadow]
    d_s = dL[castshadow]
    rel_env = numpy.abs((a_s - d_s) - b_s) / numpy.maximum(b_s, 1e-30)
    rel_s = numpy.abs(a_s - b_s) / numpy.maximum(b_s, 1e-30)
    umbra = castshadow & (dL < UMBRA_DIRECT_MAX)
    print("=== attribution (cast-shadow region) ===", flush=True)
    print("  env-only mean=%.6g min=%.6g   shipped mean=%.6g   "
          "|A-B|/B mean=%.3e max=%.3e   |(A-D)-B|/B mean=%.3e max=%.3e"
          % (float(b_s.mean()), float(b_s.min()), float(a_s.mean()),
             float(rel_s.mean()), float(rel_s.max()),
             float(rel_env.mean()), float(rel_env.max())), flush=True)
    check("ambient_intact_in_shadow", float(b_s.min()) > 0.0,
          "env-only min=%.6g inside the shadow (a fully black shadow would make "
          "the comparison vacuous)" % float(b_s.min()))
    check("shadow_never_touched_the_env_term", float(rel_env.max()) < ENV_UNTOUCHED_TOL,
          "max |(A-D)-B|/B = %.3e (tol %.1e) over %d shadowed px — a shadow that "
          "also attenuated the env would show here at the size of the shadow "
          "factor" % (float(rel_env.max()), ENV_UNTOUCHED_TOL, int(castshadow.sum())))

    n_umbra = int(umbra.sum())
    print("  umbra (surviving direct < %.1e): %d px" % (UMBRA_DIRECT_MAX, n_umbra),
          flush=True)
    check("umbra_present", n_umbra >= MIN_PIXELS, "%d px (min %d)" % (n_umbra, MIN_PIXELS))
    if n_umbra >= MIN_PIXELS:
      rel_u = numpy.abs(lum(A)[umbra] - bL[umbra]) / numpy.maximum(bL[umbra], 1e-30)
      print("  umbra |A-B|/B mean=%.3e max=%.3e" % (float(rel_u.mean()), float(rel_u.max())),
            flush=True)
      check("umbra_reads_the_env_only_value", float(rel_u.max()) < UMBRA_TOL,
            "max |A-B|/B = %.3e (tol %.1e) over %d fully shadowed px"
            % (float(rel_u.max()), UMBRA_TOL, n_umbra))

    ##########################################################
    # SUPERPOSITION in the lit region: direct and image-based light add
    ##########################################################
    a_l = lum(A)[lit]
    sum_l = bL[lit] + dL[lit]
    rel_l = numpy.abs(a_l - sum_l) / numpy.maximum(a_l, 1e-30)
    print("=== superposition (lit region) ===", flush=True)
    print("  env=%.6g  direct=%.6g  shipped=%.6g  |A-(B+D)|/A mean=%.3e max=%.3e"
          % (float(bL[lit].mean()), float(dL[lit].mean()), float(a_l.mean()),
             float(rel_l.mean()), float(rel_l.max())), flush=True)
    check("direct_and_env_superpose", float(rel_l.max()) < SUPERPOS_TOL,
          "max |A-(B+D)|/A = %.3e (tol %.1e) over %d lit px"
          % (float(rel_l.max()), SUPERPOS_TOL, int(lit.sum())))

    ok = (len(failures) == 0)
    detail = ("shadow_px=%d lit_px=%d removed=%.6g residual=%.6g "
              "env_in_shadow=%.6g attrib_max=%.3e superpos_max=%.3e"
              % (int(castshadow.sum()), int(lit.sum()), removed, residual,
                 float(b_s.mean()), float(rel_s.max()), float(rel_l.max())))
    if failures:
      detail += " failed=" + ",".join(failures)
    verdict(ok, detail)                        # VERDICT BEFORE TEARDOWN
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = ShadowAttributionApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
