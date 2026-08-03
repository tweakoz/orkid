#!/usr/bin/env ork.python
################################################################################
# DIFFUSE IRRADIANCE CORRECTNESS gate (procsky wave4 slice W4-S8; converted from
# the W4-S3 harness that measured the path this one replaced).
#
# The procsky diffuse env is now SPHERICAL HARMONICS. One dynamic probe, fed by
# the equirect snapshot each IBL cycle freezes, projected onto the L2 basis on
# the GPU (ProbeSHProjector::projectEquirect) and reconstructed per fragment as
# a cosine-convolved irradiance (stdtools lib_env_sh, read by fwdtools
# lib_fwd_impl). The prefiltered equirect diffuse map — a GGX importance lobe
# standing in for a clamped cosine, read at a hardcoded coarse mip — no longer
# reaches a shaded fragment on the procedural path at all. This gate measures
# whether the number that arrives at a shaded fragment with world normal N is
# the sky's cosine-weighted irradiance AROUND N.
#
# TWO INDEPENDENT ORACLES, both against the sky the engine actually consumed:
#
#   the SHADED BALL   the end-to-end reading. Capture -> projection -> UBO ->
#                     reconstruction -> shading, measured off a lambertian
#                     receiver, scored against a cosine-weighted Riemann
#                     integral of the snapshot. This is the S3 experiment
#                     unchanged, which is what makes its numbers comparable:
#                     the same residual that read 0.124 (gradient) and 0.221
#                     (sunlow) through the prefiltered map reads ~0.018 / ~0.019
#                     through the probe.
#
#   the COEFFICIENTS  the projector's own integral, checked directly. The nine
#                     coefficients the engine read back are compared against a
#                     CPU projection of the same snapshot in the same basis,
#                     with the capture pre-scale divided out on the CPU side —
#                     so a decode applied twice, or not at all, or a projector
#                     that quietly integrated something else, fails here with
#                     nowhere to hide behind a free gain.
#
# WHAT IS MEASURED. Three sky RADIANCE FIELDS of deliberately different shape,
# each driven through the REAL capture -> convolution -> sample path in one warm
# process:
#
#   uniform   a thick, isotropic, high-albedo medium: near-constant radiance
#             over the whole sphere. CLOSED FORM: the cosine convolution of a
#             constant field is that same constant for EVERY normal, so the
#             measured response must be flat. This leg is also the CONTROL for
#             the orientation test below — a sky with no anisotropy cannot tell
#             any two orientations apart, and its numbers say so out loud.
#
#   gradient  the shipped earth medium with the sun near the ZENITH, which makes
#             the sky azimuthally symmetric: a single-axis (elevation) gradient
#             whose only structure is along world +Y.
#
#   sunlow    a forward-scattering medium with the sun LOW and on the world X
#             axis: radiance concentrated into one direction, chosen so the
#             field's structure is azimuthal where the gradient leg's is polar.
#
# Between them the two anisotropic legs cover both axes the response can be
# tested on, and each is judged only on the axis it was built for — the gradient
# sky has no azimuthal structure to offer and the sunlow sky's polar structure
# is a weak by-product. The uniform leg is deliberately blind to everything: it
# is the control that shows the instrument reports "no signal" when there is
# none, instead of reporting noise.
#
# THE REFERENCE is not a formula fitted to the sky we hoped for: it is a
# cosine-weighted RIEMANN INTEGRAL of the sky snapshot the engine actually
# convolved, read back as RGBA32F from sky_ibl_snapshot_rtgroup. Two conventions
# are baked into it and both are load-bearing: row v maps to polar angle v*pi
# from +Y (skyEquirectUV2Dir), and the snapshot is written MIRRORED IN X
# (sky.fxv2 ps_sky_equirect does rd.x = -rd.x, so the shared specular samplers
# read it at the bearing the baked .xir assets are authored for).
#
# THE ORIENTATION TEST. Comparing the measured response against the reference at
# the SAME normal only answers "does it match". This gate asks the stronger
# question — WHICH direction was integrated — in two layers.
#
#   The transform table (printed, not asserted) scores the measurement against
#   the reference evaluated at each of eight candidate rigid transforms of the
#   normal. It is the readable picture, and it is what localized the mirrored-U
#   defect this gate was built for: before envtools env_equirectangular2 undid
#   the mirror, the diffuse path integrated the sky around the X-MIRRORED
#   normal. The Y<->Z swap that a static reading of the shaders suggested was
#   RULED OUT by it — that transform scored worse than the identity in every
#   leg, which is why this gate scores a family instead of checking a guess.
#
#   The parity probes (asserted) compare each normal against its own mirror
#   image on the same ball in the same frame, which cancels the convolution
#   kernel's error instead of tolerating it. See _parity.
#
# See DELIVERED_ORIENTATION below for what is pinned and how it moves if the
# projection's convention ever moves again.
#
# HOW THE RESPONSE IS SAMPLED. One white lambertian ball (metallic 0, roughness
# 1) with the specular env level driven to ZERO and no flat ambient, so the only
# light in the frame is the diffuse env term. Per-normal sampling is FORWARD
# ONLY — a normal n is chosen, its surface point C + R*n is handed to the
# ENGINE's own camera.project(), and the pixel that comes back is read. No
# camera inverse, no assumed handedness, no assumed field of view. The ball
# radius R comes from the model's own AABB, and the gate CHECKS it by predicting
# the silhouette's pixel radius and comparing against the rendered ball's mask —
# a wrong R would silently mislabel every normal, so it fails loudly instead.
#
# THE FLOAT SURFACE. A pre-tonemap linear readback needs the scenegraph's own
# compositor and an RGBA32F outputRTG; the recipe and its failure modes are
# documented in test_env_hdr_range_gate.py, whose harness this gate reuses.
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
CAM_DIST      = 7.0
BALL_POS      = vec3(0, 2, 0)
BALL_SCALE    = 2.2

SETTLE_FRAMES = 24
WAIT_SECONDS  = 90.0

# normal sampling. FACING_MIN is a dot against the ball-to-eye axis: below it the
# surface is near the silhouette, where one pixel spans many degrees of normal
# and the mesh's own tessellation error dominates.
NORMAL_CANDIDATES = 1600
FACING_MIN        = 0.32
PARITY_FLOOR      = 0.12  # see _normalSet
FOOTPRINT         = 1     # half-width of the pixel box averaged per normal
MIN_SAMPLES       = 120

# reference integration: the snapshot is 512x256, so a full-resolution Riemann
# sum is 131072 texels per normal. Rows are decimated by this factor (columns
# are not) purely for cost; the sin(theta) weight is re-evaluated on the kept
# rows, so the quadrature stays consistent rather than merely approximate.
REF_ROW_STRIDE = 2

##############################################################################
# THE ORIENTATION THIS PATH DELIVERS — a MEASURED FACT, pinned.
#
# The diffuse ambient integrates the sky around the SHADED normal N, at TRUE
# world azimuth, with no mirror compensation anywhere. The SH projector
# generates each texel's direction as the one sky.fxv2 ps_sky_equirect WROTE
# there (its rd.x = -rd.x included), so the coefficients come out in world space
# and the reconstruction has nothing left to undo. Measured through the probe:
# X-parity +0.9995 on the sunlow leg, Y-parity +0.9997 on the gradient leg.
#
# The pin drives every assertion below: the residual check scores against this
# transform's reference, and each parity probe expects agreement on an axis this
# transform preserves and inversion on one it flips. A convention change in the
# projector that is not reflected here FAILS LOUDLY, which is the point.
##############################################################################
DELIVERED_ORIENTATION = "identity"

# Which way each parity probe should read, derived from the pin above: +1 where
# the delivered orientation preserves that axis, -1 where it flips it.
PARITY_EXPECT = {
    "identity": {"X": +1, "Y": +1},
    "mirrorX":  {"X": -1, "Y": +1},
}[DELIVERED_ORIENTATION]

# TOLERANCES — all of them set from measurement, see _emitVerdict's header.
FLAT_TOL          = 0.05   # uniform leg: relative RMS spread of the response
CONTROL_SWAP_TOL  = 0.10   # uniform leg: reference must be blind to the swap
POWER_MIN         = 0.25   # anisotropic legs: reference must SEE the swap
SHAPE_TOL         = 0.08   # residual about the delivered orientation (L2 truncation)
SILHOUETTE_TOL    = 0.08   # predicted vs rendered ball radius, relative
PARITY_MIN        = 0.85   # parity probe: agreement required, signed
PARITY_POWER_MIN  = 0.06   # below this the sky has no signal on that axis
# the engine's read-back coefficients against a CPU projection of the same
# snapshot: same basis, same grid, same decode. What separates them is fp16
# rounding in the source and float summation order, nothing structural.
COEFF_REL_TOL     = 0.02   # relative L2 distance over all nine coefficients
COEFF_COS_MIN     = 0.999  # direction agreement of the 27-vector


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


# Candidate rigid transforms of the world normal, scored in _emitVerdict. Each is
# an isometry, so it commutes with any rotationally symmetric convolution kernel:
# that is what lets this test name an ORIENTATION without knowing the kernel.
ORIENTATIONS = [
    ("identity",      lambda n: n),
    ("mirrorX",       lambda n: n * numpy.array([-1.0, 1.0, 1.0])),
    ("negY",          lambda n: n * numpy.array([1.0, -1.0, 1.0])),
    ("negZ",          lambda n: n * numpy.array([1.0, 1.0, -1.0])),
    ("mirrorXnegY",   lambda n: n * numpy.array([-1.0, -1.0, 1.0])),
    ("mirrorXnegZ",   lambda n: n * numpy.array([-1.0, 1.0, -1.0])),
    ("swapYZ",        lambda n: n[:, [0, 2, 1]]),
    ("mirrorXswapYZ", lambda n: n[:, [0, 2, 1]] * numpy.array([-1.0, 1.0, 1.0])),
]


LEGS = [
    # uniform: a thick isotropic medium over a white ground. Rayleigh is grey and
    # its scale height is pushed past the atmosphere thickness so density barely
    # falls off with altitude; mie is isotropic (g=0) and ozone is removed, so no
    # term in the medium prefers any direction.
    dict(key="uniform", azim=35.0, elev=62.0, park=24.0, exposure=40.0, judges=(),
         medium=dict(atmosphere_thickness=100.0,
                     rayleigh_scattering=vec3(0.02, 0.02, 0.02),
                     rayleigh_scale_height=200.0,
                     mie_scattering=0.02, mie_extinction=0.021,
                     mie_scale_height=200.0, mie_phase_g=0.0,
                     ozone_absorption=vec3(0, 0, 0),
                     ground_albedo=vec3(1, 1, 1))),
    # gradient: the shipped medium, sun near the zenith => azimuthally symmetric,
    # so the ONLY structure is elevation. 89 not 90: a sun exactly on the pole
    # makes the sun azimuth degenerate.
    dict(key="gradient", azim=35.0, elev=89.0, park=36.0, exposure=8.0,
         judges=("Y",), medium=None),
    # sunlow: forward-scattering haze with the sun low and AIMED DOWN +X, which
    # is the only leg that can tell an X mirror from the identity — the gradient
    # leg is azimuthally symmetric and provably cannot, and the uniform leg
    # cannot tell anything apart at all. Azimuth 90 rather than something
    # oblique because the probe measures the reference's X-ANTISYMMETRIC part,
    # and an oblique sun spends most of its contrast on the axis nobody is
    # asking about (measured at azimuth 35: X power 0.03, too weak to judge).
    #
    # The concentration is DELIBERATELY moderate. A first pass at g=0.85 with
    # rayleigh near zero produced a near-delta lobe (snapshot peak 19.8 against
    # a mean of 0.115) that NO orientation could track: every candidate scored a
    # residual above 0.6, because a clamped-cosine integral of a near-delta is a
    # sharp function and the delivered map is an 8x4 box-filtered GGX blob. That
    # is the approximation talking, not an orientation, and a leg that cannot
    # separate the two has no evidence to give.
    dict(key="sunlow", azim=90.0, elev=20.0, park=58.0, exposure=8.0, judges=("X",),
         medium=dict(rayleigh_scattering=vec3(0.004, 0.005, 0.010),
                     mie_scattering=0.014, mie_extinction=0.015,
                     mie_scale_height=4.0, mie_phase_g=0.70,
                     ozone_absorption=vec3(0, 0, 0),
                     ground_albedo=vec3(0.25, 0.25, 0.25))),
]


class FloatOutSGC(StandardSceneGraphComponent):
  """scenegraph that composites into an RGBA32F RtGroup (see the header)."""

  def _onGpuInit(self, ctx):
    rtg = lev2.RtGroup(ctx, WIDTH, HEIGHT)
    rtg.name = "envdiff_f32"
    rtg.createBuffer(tokens.RGBA32F, tokens.color)
    self.float_rtg = rtg
    self.sg_params.outputRTG = rtg
    super()._onGpuInit(ctx)


class DiffuseConvolutionApp(ComponentizedApplication):

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
    self._marks = {}
    self._sh = {}
    self._scale = {}
    self.SGC = self.addComponent(
        "std_scenegraph", FloatOutSGC,
        eye=vec3(0, 2, -CAM_DIST), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          # SPECULAR OFF: pbrEnvironmentLightingWithF0 scales its specular term
          # by SpecularLevel (fwdtools.i2), so zero here leaves the diffuse env
          # term as the ONLY light on the receiver — which is what makes the
          # measured number a reading of the diffuse ambient rather than a mixture.
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
    # HARD SWAP, no crossfade: the auto-sized fade window is hundreds of frames
    # wide at offscreen frame rates, so a capture a settle after the publish
    # would read mostly the PREVIOUS leg's sky (fleet rule for any gate that
    # reads lighting after an IBL publish).
    self.atmo.ibl_crossfade_frames = 0
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"
    # the shipped medium, captured BEFORE any leg edits it — the "gradient" leg
    # runs on these values and the others restore what they do not override.
    self._medium_defaults = {
        "atmosphere_thickness": self.atmo.atmosphere_thickness,
        "rayleigh_scattering": self.atmo.rayleigh_scattering,
        "rayleigh_scale_height": self.atmo.rayleigh_scale_height,
        "mie_scattering": self.atmo.mie_scattering,
        "mie_extinction": self.atmo.mie_extinction,
        "mie_scale_height": self.atmo.mie_scale_height,
        "mie_phase_g": self.atmo.mie_phase_g,
        "ozone_absorption": self.atmo.ozone_absorption,
        "ground_albedo": self.atmo.ground_albedo,
    }

    self.ball = SGC.createBallNode("recv_diffuse", ctx=ctx, position=BALL_POS,
                                   color=vec4(1, 1, 1, 1), metallic=0.0, roughness=1.0,
                                   scale=BALL_SCALE)
    # The receiver's world radius, from the model rather than from the framing.
    # aabb_whd carries HALF-extents, and XgmModel.boundingRadius is the AABB's
    # CORNER distance (sqrt(3) times this) — measured 1.71 against a sphere of
    # radius 0.99. Neither is documented as such, which is why the silhouette
    # check below is an assertion and not a comment: a wrong radius mislabels
    # every normal in this gate while still producing plausible-looking numbers
    # (it read 296 px predicted against 149 px rendered on the first attempt).
    whd = SGC._ball_model.aabb_whd
    self.ball_radius = float(min(whd.x, whd.y, whd.z)) * BALL_SCALE

    # DIRECTION SOURCE ONLY: intensity 0 keeps every photon in the frame
    # image-based while the prologue still bakes the LUTs (and the snapshot)
    # with this sun direction.
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(LEGS[0]["azim"], LEGS[0]["elev"])
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass

  ##############################################################

  def _aimSun(self, azimuth_deg, elevation_deg):
    # DirectionalLight::direction() is the light's TRAVEL direction, so aiming it
    # FROM the sun's position at the origin makes dir_to_sun the negation — which
    # is what the compositor prologue's LUT step reads.
    d = dir_to_sun(azimuth_deg, elevation_deg)
    self.sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))

  def _applyMedium(self, medium):
    """restore the shipped medium, then overlay this leg's overrides. Every field
    is written every leg: the atmosphere object is shared across legs and a knob
    left behind by an earlier one would silently ride into the next."""
    for k, v in self._medium_defaults.items():
      setattr(self.atmo, k, v)
    for k, v in (medium or {}).items():
      setattr(self.atmo, k, v)

  def _pbr(self):
    return self.SGC.pbr_common

  def _note(self, txt):
    print("[env-diff] %s" % txt, flush=True)

  def _fail(self, why):
    verdict(False, why)
    self._exit_code = 1
    self._done = True
    self.ezapp.signalExit()

  ##############################################################
  # capture plumbing
  ##############################################################

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
    self._shots[key] = img
    self._inflight = None
    print("[env-diff] captured %s %dx%d mean=%.6g max=%.6g" %
          (key, w, h, float(img.mean()), float(img.max())), flush=True)
    return key

  ##############################################################
  # per-leg state machine (the sun walk is how an IBL publish is triggered at
  # all: the shipped feed refilters on sun ANGLE, so each leg parks the sun and
  # walks it back to the elevation it is measured at)
  ##############################################################

  def _restate(self, s):
    self._state = s
    self._state_frame = self._frame
    self._state_time = time.time()

  def _published(self, key, mark, count):
    want = self._marks[mark + key] + count
    gen = int(self._pbr().sky_ibl_generation)
    if (gen >= want) and (not bool(self._pbr().sky_ibl_inflight)):
      return True
    if (time.time() - self._state_time) > WAIT_SECONDS:
      self._fail("leg %s: generation never reached %d (%.1f s, %d frames, gen=%d inflight=%s)"
                 % (key, want, time.time() - self._state_time,
                    self._frame - self._state_frame, gen,
                    bool(self._pbr().sky_ibl_inflight)))
    return False

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      # PRIME: the first-ever refilter's snapshot is the keyframe every later
      # sun-move trigger is measured against, so no leg may park the sun while
      # it is still running (see test_env_hdr_range_gate.py).
      if bool(self._pbr().sky_ibl_ready) and (not bool(self._pbr().sky_ibl_inflight)):
        self._built = True
        self._restate(0)
      elif (time.time() - self._state_time) > WAIT_SECONDS:
        self._fail("the first procedural refilter never published")
      return
    if self._leg >= len(LEGS):
      return

    leg = LEGS[self._leg]
    key = leg["key"]

    if self._state == 0:
      self._marks["gen0_" + key] = int(self._pbr().sky_ibl_generation)
      self._applyMedium(leg["medium"])
      self.atmo.sky_exposure = leg["exposure"]
      self._aimSun(leg["azim"], leg["park"])
      self._note("leg %s: medium hash %016x, exposure %.1f, sun parked at %.1f deg (gen %d)"
                 % (key, int(self.atmo.medium_hash), leg["exposure"], leg["park"],
                    self._marks["gen0_" + key]))
      self._restate(1)
      return

    if self._state == 1:                      # the park publish
      if not self._published(key, "gen0_", 1):
        return
      self._aimSun(leg["azim"], leg["elev"])
      self._marks["gen1_" + key] = int(self._pbr().sky_ibl_generation)
      self._restate(2)
      return

    if self._state == 2:                      # the return publish (measured on)
      if not self._published(key, "gen1_", 1):
        return
      self._restate(3)
      return

    if self._state == 3:                      # settle
      fade = float(self._pbr().sky_ibl_fade_weight)
      if fade < 0.9999:
        if (time.time() - self._state_time) > WAIT_SECONDS:
          self._fail("leg %s: crossfade never settled (weight=%.4f)" % (key, fade))
        return
      if (self._frame - self._state_frame) >= SETTLE_FRAMES:
        self._restate(4)
      return

    if self._state == 4:                      # render float + capture
      self.SGC.scenegraph.renderOnContext(ctx)
      if not self._realized:
        # REALIZE the target: captureAsFormat asserts natively on an unbuilt
        # RtBuffer impl rather than raising.
        ctx.FBI.rtGroupInit(self.SGC.float_rtg)
        self._realized = True
        return
      p = self._pbr()
      self._note("%s: fade=%.4f ambient=%s diffuseLevel=%.3f specularLevel=%.3f skyboxLevel=%.3f"
                 % (key, float(p.sky_ibl_fade_weight), repr(p.ambientLevel),
                    float(p.diffuseLevel), float(p.specularLevel), float(p.skyboxLevel)))
      # THE SOURCE OF THE LIGHT IN THIS FRAME, asserted rather than assumed: a
      # leg that captured while the probe was absent would be measuring the
      # authored-envmap fallback and reporting it as the SH path.
      if not bool(p.sky_sh_valid):
        self._fail("leg %s: the sky SH probe is not published at capture time "
                   "(the diffuse ambient would be coming from the authored map)" % key)
        return
      self._sh[key] = [numpy.array([c.x, c.y, c.z]) for c in p.sky_sh_coefficients]
      self._scale[key] = float(self.atmo.ibl_capture_scale)
      self._issueCapture(ctx, "lit_" + key, self.SGC.float_rtg, "RGBA32F")
      self._restate(5)
      return

    if self._state == 5:                      # collect lit, capture the source
      if self._collect() is None:
        return
      rtg = self._pbr().sky_ibl_snapshot_rtgroup
      if rtg is None:
        self._fail("leg %s: no equirect snapshot RTG after a published cycle" % key)
        return
      self._issueCapture(ctx, "snap_" + key, rtg, "RGBA32F")
      self._restate(6)
      return

    if self._collect() is None:               # 6: collect the snapshot, advance
      return
    self._leg += 1
    self._restate(0)
    if self._leg >= len(LEGS):
      self._emitVerdict()

  ##############################################################
  # observables
  ##############################################################

  def _pixelOf(self, wpos, w, h):
    ndc = self.SGC.camera.project(float(w) / float(h), wpos)
    return ((ndc.x * 0.5 + 0.5) * w, (ndc.y * 0.5 + 0.5) * h)

  def _normalSet(self):
    """Fibonacci-distributed normals on the ball, kept where the surface faces
    the camera squarely enough that one pixel is one normal — and arranged into
    MIRROR-CLOSED quartets so the parity probes below have exact partners.

    The set is emitted as four equal blocks of B normals in a fixed sign order:

        block 0: (+x, +y, z)    block 1: (-x, +y, z)
        block 2: (+x, -y, z)    block 3: (-x, -y, z)

    so index k pairs with B+k across X and with 2B+k across Y. Both flips are
    VISIBILITY PRESERVING here and that is not luck: the camera sits on the
    ball's own -Z axis, so facing depends on z alone and the quartet is either
    entirely visible or entirely rejected. The gate asserts that camera geometry
    rather than trusting it, because a camera moved off-axis would silently turn
    every parity pair into a comparison of a visible point against a point
    behind the horizon."""
    eye = self.SGC.camera.eye
    c = numpy.array([BALL_POS.x, BALL_POS.y, BALL_POS.z])
    u = numpy.array([eye.x, eye.y, eye.z]) - c
    u = u / numpy.linalg.norm(u)
    if (abs(u[0]) > 1.0e-5) or (abs(u[1]) > 1.0e-5):
      self._fail("camera is off the ball's Z axis (u=%s): the parity probes' "
                 "visibility-preserving property does not hold" % (u,))
      return numpy.zeros((0, 3)), 0
    i = numpy.arange(NORMAL_CANDIDATES) + 0.5
    y = 1.0 - 2.0 * i / NORMAL_CANDIDATES
    r = numpy.sqrt(numpy.maximum(1.0 - y * y, 0.0))
    ga = math.pi * (3.0 - math.sqrt(5.0))
    n = numpy.stack([r * numpy.cos(ga * i), y, r * numpy.sin(ga * i)], axis=1)
    keep = (n @ u) > FACING_MIN
    # PARITY_FLOOR keeps the quartet's four members apart: a normal already near
    # x=0 or y=0 barely moves when that component is flipped, so it contributes
    # nothing but noise to the difference the probe measures.
    keep &= (n[:, 0] > PARITY_FLOOR) & (n[:, 1] > PARITY_FLOOR)
    base = n[keep]
    quart = numpy.concatenate([base * [1, 1, 1], base * [-1, 1, 1],
                               base * [1, -1, 1], base * [-1, -1, 1]])
    return quart, len(base)

  def _sampleBall(self, img, normals):
    """measured radiance per normal, and the pixel each one landed on. FORWARD
    projection only — the engine's camera is the single source of truth for
    where a surface point lands."""
    h, w, _ = img.shape
    c = numpy.array([BALL_POS.x, BALL_POS.y, BALL_POS.z])
    vals = numpy.zeros(len(normals))
    ok = numpy.zeros(len(normals), dtype=bool)
    for k, n in enumerate(normals):
      p = c + self.ball_radius * n
      px, py = self._pixelOf(vec3(float(p[0]), float(p[1]), float(p[2])), w, h)
      ix, iy = int(round(px)), int(round(py))
      if ix < FOOTPRINT or iy < FOOTPRINT or ix >= w - FOOTPRINT or iy >= h - FOOTPRINT:
        continue
      box = img[iy - FOOTPRINT:iy + FOOTPRINT + 1, ix - FOOTPRINT:ix + FOOTPRINT + 1, :3]
      # the ball is the only lit thing in the frame, so a zero anywhere in the
      # footprint means the box straddled the silhouette: reject rather than
      # average background into the reading.
      if float(box.min()) <= 0.0:
        continue
      vals[k] = float(box.mean())
      ok[k] = True
    return vals, ok

  def _silhouetteCheck(self, img):
    """predicted vs rendered ball radius in pixels. A sphere's silhouette
    normals satisfy n.u == R/d (u = the unit ball-to-eye axis, d their
    distance), so the silhouette's pixel radius is predictable from R alone —
    and R is what every normal label in this gate depends on."""
    h, w, _ = img.shape
    eye = self.SGC.camera.eye
    c = numpy.array([BALL_POS.x, BALL_POS.y, BALL_POS.z])
    e = numpy.array([eye.x, eye.y, eye.z])
    d = float(numpy.linalg.norm(e - c))
    u = (e - c) / d
    R = self.ball_radius
    ct = R / d
    st = math.sqrt(max(1.0 - ct * ct, 0.0))
    # any unit vector perpendicular to u spans the silhouette cone
    a = numpy.array([0.0, 1.0, 0.0])
    if abs(float(a @ u)) > 0.9:
      a = numpy.array([1.0, 0.0, 0.0])
    t = numpy.cross(u, a)
    t = t / numpy.linalg.norm(t)
    n_sil = ct * u + st * t
    p_sil = c + R * n_sil
    cx, cy = self._pixelOf(vec3(float(c[0]), float(c[1]), float(c[2])), w, h)
    sx, sy = self._pixelOf(vec3(float(p_sil[0]), float(p_sil[1]), float(p_sil[2])), w, h)
    predicted = math.hypot(sx - cx, sy - cy)
    mask = img[..., :3].max(axis=2) > 0.0
    measured = math.sqrt(float(mask.sum()) / math.pi)
    return predicted, measured

  def _snapshotField(self, snap):
    """(directions, weights) for the sky snapshot, in WORLD space.

    Two conventions, both from the shaders that wrote and read this image:
      * skyEquirectUV2Dir (skytools.i2) — u sweeps azimuth from the -X meridian,
        v maps to polar angle v*pi measured from +Y.
      * ps_sky_equirect (sky.fxv2) writes MIRRORED IN X (rd.x = -rd.x) so the
        shared specular samplers read it at the bearing the baked .xir assets
        are authored for. Undone here, because this gate integrates in WORLD
        directions.
    The weight carries the solid angle sin(theta) and the radiance itself."""
    h, w, _ = snap.shape
    rows = numpy.arange(0, h, REF_ROW_STRIDE)
    lum = snap[rows][..., :3].astype(numpy.float64).mean(axis=2)
    th = (rows + 0.5) / float(h) * math.pi
    ph = (numpy.arange(w) + 0.5) / float(w) * 2.0 * math.pi - math.pi
    st = numpy.sin(th)[:, None]
    ct = numpy.cos(th)[:, None]
    dx = -(st * numpy.cos(ph)[None, :])          # the X mirror, undone
    dy = numpy.broadcast_to(ct, (len(rows), w))
    dz = st * numpy.sin(ph)[None, :]
    dirs = numpy.stack([dx.ravel(), dy.ravel(), dz.ravel()], axis=1)
    wgt = (lum * st).ravel()
    return dirs, wgt

  @staticmethod
  def _shReference(snap, capture_scale):
    """the nine L2 coefficients of the captured snapshot, integrated on the CPU.

    Deliberately NOT a decimated integral: this is the projector's own claim
    under test, so it walks every texel of the source exactly as the kernel
    does, in the same basis and the same order of bands (probe_sh.h). The
    direction of a texel is the one ps_sky_equirect wrote there, x mirror
    included, and the CAPTURE PRE-SCALE is divided out here — the engine's
    coefficients are supposed to carry decoded radiance, so a decode that
    double-applied or never happened shows up as a pure gain error against
    this."""
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
    return [(L * b[..., None]).sum(axis=(0, 1)) for b in basis]

  def _refIrradiance(self, dirs, wgt, normals):
    """cosine-weighted irradiance of the measured sky field, per normal. Scale is
    irrelevant everywhere it is used (every comparison fits a free gain), so the
    dtheta*dphi constant is deliberately not carried."""
    out = numpy.zeros(len(normals))
    for k in range(0, len(normals), 32):
      chunk = normals[k:k + 32]
      c = numpy.maximum(dirs @ chunk.T, 0.0)     # (texels, chunk)
      out[k:k + 32] = wgt @ c
    return out

  @staticmethod
  def _parity(meas, ref, ok, ia, ib):
    """SIGNED parity probe across one axis.

    The transform table below scores whole-shape residuals, and those residuals
    carry a floor this gate is not entitled to remove: the reconstruction is a
    truncated L2 series, so even a perfectly oriented path cannot reproduce a
    sky's fine structure. The floor is far smaller through the probe than it was
    through the prefiltered map (~0.02 against ~0.10-0.13), but it is still a
    floor, and it is still not the axis-resolved question.

    This probe removes it instead of tolerating it. Every normal is compared
    against its own mirror image across the axis, on the same ball, in the same
    frame, under the same reconstruction: whatever the truncation gets wrong it
    gets wrong to BOTH members of the pair, so it cancels in the
    difference. What survives is the sky's antisymmetric part, and the only
    question left is its SIGN — agreement with the reference means the shaded
    normal is the direction that was integrated, opposition means that axis is
    flipped. Returned as a cosine similarity in [-1, +1], with the reference's
    own antisymmetric strength ("power") so a sky that has no signal on this
    axis reports that fact instead of a coin flip."""
    both = ok[ia] & ok[ib]
    if both.sum() < 8:
      return 0.0, 0.0, int(both.sum())
    dm = meas[ia][both] - meas[ib][both]
    de = ref[ia][both] - ref[ib][both]
    nm = float(numpy.linalg.norm(dm))
    ne = float(numpy.linalg.norm(de))
    power = ne / max(float(numpy.linalg.norm(ref[ia][both])), 1e-30)
    if nm <= 0.0 or ne <= 0.0:
      return 0.0, power, int(both.sum())
    return float(dm @ de) / (nm * ne), power, int(both.sum())

  @staticmethod
  def _fit(meas, ref):
    """best-fit gain and the residual it leaves, relative. Both signals carry an
    arbitrary common scale (albedo, DiffuseLevel, sky exposure), so only the
    SHAPE is under test."""
    den = float(ref @ ref)
    if den <= 0.0:
      return 0.0, float("inf"), 0.0
    a = float(meas @ ref) / den
    resid = meas - a * ref
    denom = a * float(numpy.linalg.norm(ref))
    rel = float(numpy.linalg.norm(resid)) / denom if denom > 0 else float("inf")
    mm = meas - meas.mean()
    rr = ref - ref.mean()
    dn = float(numpy.linalg.norm(mm) * numpy.linalg.norm(rr))
    corr = float(mm @ rr) / dn if dn > 0 else 0.0
    return a, rel, corr

  ##############################################################

  def _emitVerdict(self):
    ##########################################################
    # TOLERANCES, and why each one is where it is. Every number below was set
    # from the measurements this gate prints, not from theory:
    #
    #  SHAPE_TOL (0.08) is the residual the delivered orientation is allowed.
    #    Measured 0.018 (gradient) and 0.019 (sunlow) through the SH probe,
    #    against 0.124 and 0.221 through the prefiltered map this replaced — so
    #    the bar sits about 4x above the worse of them, which is margin for a
    #    sky whose shape changes (a below-horizon fill is coming) and not for a
    #    path that has stopped tracking the sky. ONE deliberate approximation
    #    still lives inside it: the series is truncated at L2, so a sky with
    #    structure finer than a quadratic cannot be reproduced exactly. That is
    #    the design, not a defect, and it is small — an order of magnitude below
    #    what the GGX-lobe map cost.
    #
    #  PARITY_MIN (0.85) is the agreement the parity probes must reach on their
    #    design axis, after the expected sign is applied. What the probe resolves
    #    is a sign, and the populations are now far apart AND near saturation:
    #    measured +0.9995 (X, sunlow) and +0.9997 (Y, gradient) where the axis is
    #    preserved. A mirrored axis reads the same magnitude negative, so any bar
    #    in (0,1) separates them; this one sits where a path that merely got
    #    noisy also fails.
    #
    #  FLAT_TOL / CONTROL_SWAP_TOL / POWER_MIN / PARITY_POWER_MIN are self-checks
    #    on the EXPERIMENT rather than on the engine: a uniform sky must produce
    #    a flat response and must be blind to the orientation test, and the
    #    anisotropic skies must actually be anisotropic along the axis each one
    #    is asked about. A gate that reported an orientation from skies with no
    #    directional structure would be reporting noise.
    ##########################################################
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

    normals, B = self._normalSet()
    if B == 0:
      return                                 # _normalSet already emitted a verdict
    # quartet layout (see _normalSet): index k pairs with B+k across X and with
    # 2B+k across Y; the second pair of each axis reuses the other Y/X sign, so
    # every normal contributes to both probes.
    ix_a = numpy.concatenate([numpy.arange(B), numpy.arange(2 * B, 3 * B)])
    ix_b = numpy.concatenate([numpy.arange(B, 2 * B), numpy.arange(3 * B, 4 * B)])
    iy_a = numpy.concatenate([numpy.arange(B), numpy.arange(B, 2 * B)])
    iy_b = numpy.concatenate([numpy.arange(2 * B, 3 * B), numpy.arange(3 * B, 4 * B)])
    PAIRS = (("X", ix_a, ix_b), ("Y", iy_a, iy_b))

    pred, meas_r = self._silhouetteCheck(self._shots["lit_" + LEGS[0]["key"]])
    print("=== receiver geometry ===", flush=True)
    print("  ball radius(world)=%.4f  silhouette px predicted=%.2f rendered=%.2f  normals offered=%d"
          % (self.ball_radius, pred, meas_r, len(normals)), flush=True)
    check("silhouette_matches_model_radius",
          abs(pred - meas_r) / max(meas_r, 1.0) < SILHOUETTE_TOL,
          "predicted=%.2f rendered=%.2f rel=%.4f (tol %.2f)"
          % (pred, meas_r, abs(pred - meas_r) / max(meas_r, 1.0), SILHOUETTE_TOL))

    results = {}
    for L in LEGS:
      key = L["key"]
      lit = self._shots["lit_" + key]
      snap = self._shots["snap_" + key]
      vals, ok = self._sampleBall(lit, normals)
      dirs, wgt = self._snapshotField(snap)
      n_ok = normals[ok]
      m = vals[ok]
      scores = []
      for name, xf in ORIENTATIONS:
        ref = self._refIrradiance(dirs, wgt, xf(n_ok))
        a, rel, corr = self._fit(m, ref)
        scores.append((name, a, rel, corr))
      ref_id = self._refIrradiance(dirs, wgt, n_ok)
      swap_xf = dict(ORIENTATIONS)["swapYZ"]
      ref_sw = self._refIrradiance(dirs, wgt, swap_xf(n_ok))
      swap_contrast = (float(numpy.linalg.norm(ref_id - ref_sw))
                       / max(float(numpy.linalg.norm(ref_id)), 1e-30))
      # parity probes run over the FULL quartet indexing, so they need the
      # unfiltered arrays plus the validity mask rather than the packed ones.
      ref_full = self._refIrradiance(dirs, wgt, normals)
      parity = {}
      for axis, ia, ib in PAIRS:
        parity[axis] = self._parity(vals, ref_full, ok, ia, ib)
      results[key] = dict(n=len(m), m=m, scores=scores, swap_contrast=swap_contrast,
                          parity=parity, sky_mean=float(snap[..., :3].mean()))

      print("=== leg %s ===" % key, flush=True)
      print("  samples=%d  response mean=%.6g min=%.6g max=%.6g  sky mean=%.6g  "
            "reference swap-contrast=%.4f"
            % (len(m), float(m.mean()) if len(m) else 0.0,
               float(m.min()) if len(m) else 0.0, float(m.max()) if len(m) else 0.0,
               results[key]["sky_mean"], swap_contrast), flush=True)
      for name, a, rel, corr in scores:
        print("    orientation %-14s gain=%.6g  rel_rms=%.4f  corr=%+.4f"
              % (name, a, rel, corr), flush=True)
      for axis, ia, ib in PAIRS:
        agree, power, npair = parity[axis]
        print("    parity %s   agreement=%+.4f  reference power=%.4f  pairs=%d"
              % (axis, agree, power, npair), flush=True)
      check("samples_%s" % key, len(m) >= MIN_SAMPLES,
            "n=%d (min %d)" % (len(m), MIN_SAMPLES))

    ##########################################################
    # THE PROJECTOR'S OWN INTEGRAL — the engine's read-back coefficients
    # against a CPU projection of the same snapshot (see _shReference). This is
    # the only check here that carries no free gain, so it is where the capture
    # pre-scale's decode is actually pinned: a factor applied twice, or dropped,
    # moves every coefficient by that factor and lands as a relative error of
    # (scale - 1), which is 31 at the shipped gain of 32.
    ##########################################################
    print("=== SH coefficients (engine vs CPU projection of the same snapshot) ===",
          flush=True)
    for L in LEGS:
      key = L["key"]
      got = numpy.array(self._sh[key]).reshape(-1)
      want = numpy.array(self._shReference(self._shots["snap_" + key],
                                           self._scale[key])).reshape(-1)
      dn = float(numpy.linalg.norm(want))
      rel = float(numpy.linalg.norm(got - want)) / max(dn, 1e-30)
      cos = (float(got @ want) / max(float(numpy.linalg.norm(got)) * dn, 1e-30))
      print("  %-9s capture_scale=%.1f  |engine|=%.6g |cpu|=%.6g  rel=%.5f  cos=%+.6f"
            % (key, self._scale[key], float(numpy.linalg.norm(got)), dn, rel, cos),
            flush=True)
      print("           L0 engine=<%.6g,%.6g,%.6g>  cpu=<%.6g,%.6g,%.6g>"
            % (got[0], got[1], got[2], want[0], want[1], want[2]), flush=True)
      check("sh_coeffs_match_cpu_projection_%s" % key, rel < COEFF_REL_TOL,
            "rel=%.5f (tol %.3f) — a pure gain error here is the capture "
            "pre-scale decoding the wrong number of times" % (rel, COEFF_REL_TOL))
      check("sh_coeffs_direction_%s" % key, cos > COEFF_COS_MIN,
            "cos=%+.6f (min %.4f)" % (cos, COEFF_COS_MIN))

    ##########################################################
    # the uniform leg is the experiment's own control, twice over
    ##########################################################
    u = results["uniform"]
    flat = (float(numpy.std(u["m"]) / max(numpy.mean(u["m"]), 1e-30))
            if len(u["m"]) else float("inf"))
    print("=== control (uniform sky) ===", flush=True)
    print("  response relative spread=%.4f   reference swap-contrast=%.4f"
          % (flat, u["swap_contrast"]), flush=True)
    check("uniform_response_is_flat", flat < FLAT_TOL,
          "spread=%.4f (tol %.2f) — closed form: the cosine convolution of a "
          "constant field is that constant for every normal" % (flat, FLAT_TOL))
    check("control_is_orientation_blind", u["swap_contrast"] < CONTROL_SWAP_TOL,
          "swap_contrast=%.4f (tol %.2f)" % (u["swap_contrast"], CONTROL_SWAP_TOL))

    ##########################################################
    # the orientation verdict, on the anisotropic legs only
    ##########################################################
    print("=== orientation ===", flush=True)
    winners = []
    for key in ("gradient", "sunlow"):
      r = results[key]
      check("discriminator_power_%s" % key, r["swap_contrast"] > POWER_MIN,
            "swap_contrast=%.4f (min %.2f)" % (r["swap_contrast"], POWER_MIN))
      by_rel = sorted(r["scores"], key=lambda s: s[2])
      best = by_rel[0]
      ident = [s for s in r["scores"] if s[0] == "identity"][0]
      margin = ident[2] / max(best[2], 1e-30)
      winners.append(best[0])
      # The table's WINNER is reported, never asserted: its residuals sit ON the
      # kernel/mip floor described above, so a single-axis orientation error
      # moves them by about as much as the floor already is and any margin bar
      # one might set lands inside the noise (measured 1.301 against a 1.30 bar
      # — a coin toss dressed as a threshold). What IS asserted is the residual
      # against the DELIVERED orientation, which measures the convolution KERNEL
      # rather than its aim, and the identity residual is printed beside it as
      # the correctness number this slice is judged on.
      deliv = [s for s in r["scores"] if s[0] == DELIVERED_ORIENTATION][0]
      print("  %-9s best=%-14s rel_rms=%.4f   delivered(%s) rel_rms=%.4f   "
            "identity rel_rms=%.4f   margin=%.3fx"
            % (key, best[0], best[2], DELIVERED_ORIENTATION, deliv[2],
               ident[2], margin), flush=True)
      check("kernel_tracks_cosine_reference_%s" % key, deliv[2] < SHAPE_TOL,
            "rel_rms=%.4f about the %s normal (tol %.2f); about the shaded "
            "normal it is %.4f" % (deliv[2], DELIVERED_ORIENTATION, SHAPE_TOL,
                                   ident[2]))

    ##########################################################
    # PARITY — the axis-resolved verdict, with the truncation cancelled out.
    #
    # Each axis is judged on the leg BUILT to carry it and nowhere else: the
    # gradient leg is azimuthally symmetric, so Y is the only axis it has an
    # opinion about, and the sunlow leg puts the sun on the X axis for exactly
    # the same reason. Every (leg, axis) number is printed regardless, so the
    # off-design ones stay visible as evidence — but they are not assertions,
    # because a probe reads the SIGN of an inversion cleanly while its MAGNITUDE
    # is bounded by how much antisymmetric signal that axis carries at all. Off
    # its design axis that bound is tight: through the prefiltered map the sunlow
    # leg's Y probe read +0.36 while the gradient leg's read +0.84 on the same
    # engine and the same frame budget. Asserting the weaker one would be
    # asserting the sky's shape, not the orientation.
    #
    # The DESIGNED axis is still required to carry real signal (PARITY_POWER_MIN)
    # — a leg whose sky quietly stopped being anisotropic would otherwise report
    # a coin flip as a pass.
    ##########################################################
    print("=== parity (per axis, truncation-cancelled) ===", flush=True)
    for L in LEGS:
      key = L["key"]
      for axis in ("X", "Y"):
        agree, power, npair = results[key]["parity"][axis]
        judged = axis in L["judges"]
        print("  %-9s axis %s   agreement=%+.4f  power=%.4f  pairs=%d  %s"
              % (key, axis, agree, power, npair,
                 "JUDGED" if judged else "(off design axis, evidence only)"),
              flush=True)
        if not judged:
          continue
        want = PARITY_EXPECT[axis]
        check("parity_power_%s_%s" % (axis, key), power > PARITY_POWER_MIN,
              "power=%.4f (min %.2f)" % (power, PARITY_POWER_MIN))
        check("parity_%s_%s" % (axis, key), (agree * want) > PARITY_MIN,
              "agreement=%+.4f, expected sign %+d for DELIVERED_ORIENTATION=%s "
              "(bar %.2f on the signed value) — %s"
              % (agree, want, DELIVERED_ORIENTATION, PARITY_MIN,
                 "the sky is integrated around the shaded normal on this axis"
                 if want > 0 else
                 "PINNED DEFECT: the sky is integrated around the %s-mirrored "
                 "normal on this axis" % axis))

    ok = (len(failures) == 0)
    detail = ("uniform_flat=%.4f uniform_swapcontrast=%.4f "
              "gradient_best=%s gradient_rel=%.4f gradient_ident=%.4f "
              "sunlow_best=%s sunlow_rel=%.4f sunlow_ident=%.4f "
              "parityX=%+.4f/%.4f parityY=%+.4f/%.4f"
              % (flat, u["swap_contrast"],
                 winners[0],
                 sorted(results["gradient"]["scores"], key=lambda s: s[2])[0][2],
                 [s for s in results["gradient"]["scores"] if s[0] == "identity"][0][2],
                 winners[1],
                 sorted(results["sunlow"]["scores"], key=lambda s: s[2])[0][2],
                 [s for s in results["sunlow"]["scores"] if s[0] == "identity"][0][2],
                 results["sunlow"]["parity"]["X"][0], results["sunlow"]["parity"]["X"][1],
                 results["gradient"]["parity"]["Y"][0], results["gradient"]["parity"]["Y"][1]))
    if failures:
      detail += " failed=" + ",".join(failures)
    verdict(ok, detail)                      # VERDICT BEFORE TEARDOWN
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = DiffuseConvolutionApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
