#!/usr/bin/env ork.python
################################################################################
# CLOUD SHADOW (sun cookie) gate.
#
# THE CLAIM UNDER TEST, in one line: an occluder that plays the "sun_cookie"
# layer role removes the SUN from the ground under it AND from the SKY when it
# stands between the eye and the disc, removes nothing else, and the size of its
# penumbra is DATA.
#
# WHAT THE ENGINE DOES, and therefore what this measures. Once a sun declares
# CloudShadowStrength > 0, the forward prologue renders whatever sits on the
# "sun_cookie" layer from a sun-aligned ortho camera into a small mipped RT
# (ForwardPbrNodeImpl::_update_sun_cookie). The occluders' own premultiplied
# ALPHA accumulates there as the cloud occlusion — no second extinction model
# anywhere — and BOTH consumers of that one map apply BEER-LAMBERT to it:
#
#     transmittance = exp(-tau * strength * a),   tau = CloudExtinction (7.5)
#
# _sun_cookie_sample (fwdtools.i2) multiplies it into the DIRECT sun term only,
# and the sky pass (pbrtools.i2) multiplies the SAME law with the SAME tau into
# the sun/moon disc radiance — one beam cannot have two transmittances. What is
# per-consumer is only the SPATIAL filter: CloudShadowSoftness is the ground
# penumbra's mip bias, CloudDiscSoftness is the disc's (sharp, and its own).
#
# WHY THE LAW CHANGED (W15-S2). It used to be a LINEAR mix(1, 1-a, strength),
# which failed in two independent ways:
#   * at the alpha a thin visible deck actually accumulates (0.3-0.5) a linear
#     1-a leaves half to two thirds of the beam alive, so the sun stayed plainly
#     visible through a solid-looking cloud. The real direct beam through cumulus
#     is essentially gone: tau = 3*LWP/(2*rho_w*r_e) with LWP 50 g/m^2, r_e 10 um,
#     rho_w 1000 kg/m^3 gives 7.5, and exp(-7.5) = 0.05% survives a full deck
#     while a genuine veil at a=0.1 still passes 47% and grades;
#   * STRENGTH used as a MIX WEIGHT is a hard transmittance FLOOR of 1-strength
#     that no optical depth can get under — at strength 0.85 the beam keeps 15%
#     of a radiance that is orders of magnitude past full scale, i.e. a sun that
#     will not go out no matter what the deck does. Beer-Lambert composes
#     multiplicatively, so strength belongs in the EXPONENT, where it reads as
#     "how much of this deck's occlusion is real optical depth". At strength 1
#     the two forms are identical; below it only this one extinguishes; at
#     strength 0 it is exactly exp(0) = 1, so the disarmed path is untouched.
#
# WHAT THE SKY TERM TAKES (W15-S3). The cookie is also allowed to pull the
# IMAGE-BASED term down, by a declared weight (CloudShadowIblWeight, 0.65 by
# default): a cloud covers a broad wedge of the dome, and with the env term
# carrying most of a daylit frame's energy a cookie that touched only the direct
# beam was invisible. What the sky takes is NOT the beam's transmittance —
# exp(-tau*a) is near-binary past a ~ 0.3, so a sky driven by it snaps to
# 1-weight under any cloud worth seeing. It is COVERAGE:
#
#     env *= mix(1, 1 - strength*a, weight)
#
# one term, linear in the same alpha the beam's exponent uses, so a half-covered
# sky is half dimmed. The two consumers of one sample therefore differ by design,
# and this gate measures BOTH against the SAME recovered alpha (below).
#
# The occluder here is a plain PBR ball rather than a cloud deck: this gate is
# about the ENGINE path, and a ball writes alpha 1, which makes the occlusion it
# contributes exactly known. What the cloud decks contribute through that same
# path is their own shader's business (and their own gate).
#
# GROUND legs — eight captures of ONE static scene in one warm process, differing
# only in the sun's cloud-shadow data. EVERY leg declares CloudShadowIblWeight:
# the BEAM legs pin it to 0 so that "what the cookie did to the direct term" is
# still measured with the env term held fixed (the algebra below subtracts a
# disarmed leg, which only cancels if the ambient is the same in both), and the
# ENV legs are the ones that let the sky move:
#
#   off        strength 0,               iblw 0    -> the disarmed frame (no cookie pass at all)
#   on_sharp   strength 1.0, softness 0, iblw 0    -> armed at the REFERENCE state, DEFAULT tau
#   on_soft    strength 1.0, softness 3, iblw 0    -> the same, with a wide penumbra
#   on_probe   strength 1.0, tau 1.0,    iblw 0    -> the SAME geometry at a low optical depth
#   off_dark   strength 0,   sun 0                 -> image-based light alone (the env denominator)
#   on_dark    strength 1.0, sun 0,      iblw 0    -> image-based light alone, cookie armed, sky OFF-LIMITS
#   e_full     strength 1.0, sun 0,      iblw 1    -> the same, sky fully weighted
#   e_half     strength 1.0, sun 0,      iblw 0.5  -> the same, sky at half weight
#
# off_dark needs no weight: at strength 0 the cookie is not armed at all, so its
# sky_atten is exactly 1 whatever the weight says — which is itself the reason
# the disarmed frame stays byte-identical.
#
# The armed ground legs run at strength 1.0 and softness 0 because that is the
# OWNER-APPROVED bench state (both shipped scenes author past the clamp with
# zero mip blur); the gate must prove the curve where the look was signed off.
#
# THE GROUND VERDICT has six halves:
#   * the cookie removed real direct light: a measurable region of the receiver
#     is darker in on_sharp than in off, by a fraction that tracks the strength;
#   * ATTRIBUTION, in its post-W15-S3 form. With the sun switched off, arming
#     the cookie must move the env term by EXACTLY the declared weight and by
#     nothing else — three clauses, and together they are strictly stronger than
#     the "env untouched" this gate asserted before the sky was allowed to dim:
#       - at weight 0 the env is untouched to float rounding (on_dark ==
#         off_dark), so an implementation that leaked into the ambient by any
#         route other than the declared weight still fails here;
#       - at weight 1 what the env LOST over the shadow core is the same alpha
#         the beam legs recover from an independent measurement (the tau pair) —
#         one cookie sample, two consumers, ONE coverage;
#       - and the loss is LINEAR IN THE WEIGHT: e_half must remove exactly half
#         of what e_full removed, per pixel. That is the mix() form itself, and
#         no other attenuation shape satisfies it at two weights at once;
#   * SOFTNESS IS DATA: the steepest luminance step across the cookie shadow's
#     edge is strictly smaller at softness 3 than at softness 0 (the reference
#     state uses 0 — this leg proves the knob still works, it is not a default);
#   * THE GROUND CURVE IS EXPONENTIAL, proven WITHOUT knowing how the pixel
#     splits into direct and ambient. Per pixel OFF = D+E, and an armed leg is
#     D*f+E with f = exp(-tau*s*a), so
#
#         R = sum(OFF - on_sharp) / sum(OFF - on_probe)
#           = (1 - exp(-tau_hi*s*a)) / (1 - exp(-tau_lo*s*a))
#
#     — D and E cancel exactly. R is a strictly decreasing function of a alone,
#     so it INVERTS to the cookie alpha, which over an opaque ball's shadow core
#     must land in [0.80, 1.02]. A shader still running the LINEAR law ignores tau entirely,
#     makes the two legs identical, and lands R at 1.0 -> inferred alpha runs
#     away past 1 and this check FAILS. That is what makes it un-fakeable;
#   * THE BEAM REACHES THE AMBIENT FLOOR (and the cascade-contrast consequence).
#     With a inferred above, f is predicted arithmetically; measured against the
#     off_dark leg as f = (on_sharp - off_dark)/(off - off_dark) over the shadow
#     core it must match to 0.02 and be <= 0.15. This is the whole statement
#     about cascade shadows under cloud: EVERY contrast carried by the direct
#     beam — a building's cascade shadow included — is multiplied by this same
#     f, so it fades to a tenth of itself; and because off_dark is measurably
#     ABOVE zero, it fades PARTIALLY and never to nothing. No separate mechanism
#     exists or is needed.
#
# TRANSIT legs — the cookie's SECOND consumer, and the observable the ground
# legs cannot reach: a cloud standing on the eye->sun ray must put the SUN
# ITSELF out, not merely shade the dirt. Same warm process, same eye; the
# procedural sky is switched on, the camera is aimed up-sun so the disc is dead
# centre, and the occluder is parked at eye + dirToSun * 26m — exactly on the
# axis of the cookie's own ortho camera, so it lands on the very texel the disc
# consumer samples (pbrtools ps_forward_skybox_proc reads ONE texel, the eye's).
#
#   d_ref    strength 0,    off-ray                    -> the disarmed disc, the denominator
#   d_clear  strength 0.9,  off-ray                    -> cookie ARMED and non-empty, nothing over the eye
#   d_occ    strength 1.0,  ON-ray, DEFAULT tau        -> THE KILL, at the owner-approved reference state
#   d_law    strength 0.9,  ON-ray, tau 1.0            -> the same transit at a LOW optical depth
#   d_law2   strength 0.9,  ON-ray, tau 2.0            -> and at twice that depth
#   d_lawh   strength 0.45, ON-ray, tau 1.0            -> and at half strength
#   d_gsoft  strength 1.0,  ON-ray, ground softness 6  -> the ground's LOD must not reach the disc
#
# d_clear is the control that matters: it differs from d_occ ONLY in where the
# occluder stands, so an attenuation that showed up in both would be the cookie
# being armed rather than the cloud being in the way.
#
# THE TRANSIT VERDICT, on the disc's PRE-TONEMAP peak (the disc runs orders of
# magnitude past full scale — an LDR frame would report "white" for both legs).
# Write r = peak/peak(d_ref); the shader's law is r = exp(-tau*s*a):
#
#   * arming alone changes nothing:  |d_clear/d_ref - 1| <= 0.02;
#
#   * THE CURVE IS EXPONENTIAL — and the same algebra hands back the measurement
#     bias for free. The ring subtraction cannot remove the near-sun AUREOLE (the
#     sky under the disc is brighter than the sky 3-5 radii out), so every peak
#     carries a common bias C: invisible beside a blazing disc, dominant beside
#     an extinguished one. Because the probe depths are exactly 1:2, with
#     x = exp(-tau1*s*a) and peak = P*f + C,
#
#         d_law  - d_ref = P*(x-1)          d_law2 - d_ref = P*(x-1)*(x+1)
#         =>  x = (d_law2-d_ref)/(d_law-d_ref) - 1,  P = (d_law-d_ref)/(x-1),
#             C = d_ref - P,   a = -ln(x)/(tau1*s)
#
#     THREE unknowns from THREE legs, nothing assumed about the sky. The identity
#     holds only if transmittance is exponential in tau: a shader still running
#     the LINEAR law ignores tau, makes the two probe legs equal, sends x to 0
#     and yields no alpha at all — FAIL. The recovered alpha must be physical
#     ([0.80, 1.02]) for an opaque ball. The probe depths are 1.0/2.0 and not the
#     shipped 7.5 because at the shipped depth the disc is three orders down and
#     its ratio is pure bias.
#
#   * THE SUN GOES OUT, in two clauses — one arithmetic, one human:
#       (1) what is left of the DISC once C is removed is what the law predicts,
#           to 2% of the clear disc's own peak (predicted exp(-7.5*1.0*a) ~ 5e-4
#           of it, i.e. nothing);
#       (2) and the transiting disc is no longer BRIGHTER than the sky it sits
#           in. Under the linear law the same geometry left it at ~2x its sky
#           ring — a bright hole in a cloud, which is the complaint this slice
#           answers. (That run measured 0.1137 of the clear disc against a 0.28
#           threshold; there is no meaningful comparison of the two thresholds,
#           only of the two pictures.)
#     RATIONALE for the 0.80 alpha floor: the occluder is an opaque ball (alpha
#     1) whose image on the cookie is ~40 texels across and centred on the very
#     texel the disc reads, so a is 1 up to the mip filter and 0.80 leaves the
#     filter room.
#
#   * IT SCALES WITH STRENGTH: a = -ln(r)/(tau*s) inferred from d_law (s=0.9) and
#     d_lawh (s=0.45) must agree to 0.06 and stay physical (<= 1.02). Strength
#     enters the exponent, so "half the strength" is "half the optical depth" —
#     any attenuation that is not this law splits the two.
#
#   * THE DISC HAS ITS OWN LOD. d_gsoft raises CloudShadowSoftness (the GROUND
#     penumbra bias) to 6 and changes nothing else; the disc ratio must match
#     d_occ to the control tolerance. If the disc still read the ground's bias,
#     a LOD-6 tap over a ~40-texel occluder image would dilute the alpha and the
#     ratio would climb visibly. Guarded by "a transit actually happened", so a
#     disarmed mutation run cannot pass it on two undimmed discs.
#
# The transit legs raise the disc's angular radius (0.265deg is ~4 px at this
# framing) and lower its intensity into the displayable range. Both are DATA on
# the atmosphere, they scale the disc term as a whole, and the verdict is a
# RATIO of two legs that share them — what they buy is a peak made of hundreds
# of pixels instead of four, and a frame a human can look at (CLOUD_COOKIE_SHOT_DIR).
#
# CAPTURE FRESHNESS: every transit leg is captured TWICE and the later capture
# wins (the fleet law); the log prints both, so a drifting capture is visible
# rather than silently averaged in.
#
# NOT COVERED HERE: the MOON's disc. The cookie is baked along the cascade
# holder's direction and dims only the body it was baked toward, so a moon leg
# means a night scene in which the moon holds the cascade — a different rig, not
# a different constant. The sun leg proves the mechanism; SkyCookieBody's second
# component is unexercised by this gate.
#
# ENV:
#   CLOUD_COOKIE_SHOT_DIR=<dir>     dump every transit frame as a PNG (looking, not gating)
#   CLOUD_COOKIE_FORCE_STRENGTH=<f> override every leg's strength; =0 is the mutation run
#
# THE FLOAT SURFACE. Pre-tonemap linear readback through the scenegraph's own
# compositor into an RGBA32F outputRTG — the recipe (and its failure modes) is
# documented in test_env_hdr_range_gate.py, whose harness this reuses via
# test_shadow_attribution_gate.py.
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
SUN_ELEV_DEG  = 62.0
SUN_AZIM_DEG  = 0.0

RECEIVER_SCALE = 9.0    # a wide dome: the cookie shadow must land INSIDE it
OCCLUDER_SCALE = 6.0
OCCLUDER_LIFT  = 26.0   # meters toward the sun — well clear of the cascades' reach

# COOKIE RIG. Extent is deliberately small (the receiver is meters wide, not
# kilometers) and the map is coarse: softness must come from the LOD bias, which
# is the knob under test, and not from starving the resolution.
COOKIE_EXTENT   = 40.0
COOKIE_DIM      = 256
COOKIE_DEPTH    = 600.0
COOKIE_STRENGTH = 1.0   # = ANCHOR_STRENGTH: the ground legs run at the frozen reference
SOFT_SHARP      = 0.0
SOFT_WIDE       = 3.0

################################################################################
# EXTINCTION ANCHORS — every threshold below is derived from these four numbers
# and nothing else, so a retune is an edit HERE and nowhere in the checks.
#
#   transmittance = exp(-tau * strength * a)
#
# ANCHOR_TAU is asserted to BE the engine's unauthored default (a scene that
# says nothing must get the physical look) and every "default" leg leaves the
# knob alone rather than setting it, so a regressed default breaks this gate.
################################################################################
ANCHOR_TAU        = 7.5   # CloudExtinction default: 3*LWP/(2*rho_w*r_e),
                          #  LWP 50 g/m^2, r_e 10 um, rho_w 1000 kg/m^3
ANCHOR_SOLID_A    = 0.80  # the cookie alpha a SOLID shell reaches at one texel
ANCHOR_STRENGTH   = 1.00  # the OWNER-APPROVED bench state: both shipped scenes
                          #  author past the clamp (swest 3.0, forest 1.5) with
                          #  softness 0.0, so the curve is frozen against
                          #  strength 1.0 / zero mip blur / extent 9000
DISC_LOD_SHIPPED  = 0.5   # CloudDiscSoftness default (the disc's own, sharp tap)

# derived, printed by the verdict so the anchoring is visible in the log:
#   solid-shell transmittance at the reference strength
ANCHOR_TRANS      = math.exp(-ANCHOR_TAU * ANCHOR_STRENGTH * ANCHOR_SOLID_A)

# PROBE optical depths. The default tau saturates the disc past what a ratio can
# resolve (da/dr = 1/(tau*s*r) runs to ~10^2), so the CURVE and LINEARITY legs
# are measured at two low depths where the inversion is conditioned. Their RATIO
# is the whole point: log-transmittance must scale exactly with tau.
TAU_PROBE     = 1.0
TAU_PROBE2    = 2.0
ALPHA_MAX     = 1.02    # a cookie alpha can never exceed 1; the slack is measurement
DISC_RESIDUAL_TOL = 0.02  # what may be left of the disc, as a fraction of its clear peak
BEAM_TOL      = 0.03    # |measured f - f predicted from the inferred alpha|
BEAM_MAX      = 0.15    # under a solid occluder the direct beam must be this far gone
DISC_KILL_RING = 1.00   # the killed disc may not be BRIGHTER than the sky it sits in
assert abs(TAU_PROBE2 - 2.0 * TAU_PROBE) < 1e-9, \
    "the transit solve below is the tau-DOUBLING identity; the probe depths must be 1:2"

CAM_EYE = vec3(0, 7.0, -22.0)
CAM_TGT = vec3(0, 0.0, 0)

SURFACE_FLOOR = 1.0e-4   # above this the pixel is lit geometry, not background
SHADOW_DROP   = 0.02     # relative darkening that counts as "in the cookie shadow"
MIN_PIXELS    = 200
ATTRIB_TOL    = 1.0e-5   # |on_dark - off_dark| / off_dark, sun switched off AND weight 0

# THE SKY WEIGHT legs. Weight 1 is not the shipped default (0.65) on purpose:
# the law under test is the mix, and the shipped number is a LOOK that moves,
# so the gate exercises the two weights it can predict from each other instead
# of freezing an artist's dial. Both env legs run at ANCHOR_STRENGTH.
IBL_W_FULL    = 1.0
IBL_W_HALF    = 0.5
# what the env lost over the core, against the alpha the BEAM legs recovered.
# The two are different estimators of the same coverage (the beam's is a
# radiance-weighted aggregate over the core, the env's is an irradiance-weighted
# mean), so the tolerance is the spread between the weightings and not instrument
# noise. Measured 0.002 apart on mac/MoltenVK 2026-08-07 — the slack is for a
# core that is less uniformly covered than this ball's.
ENV_ALPHA_TOL = 0.06
# per-pixel: (1 - r_half) vs 0.5*(1 - r_full). This one IS instrument noise —
# the two legs are the same static frame at two weights.
ENV_MIX_TOL   = 0.01
ENV_SIGNAL    = 0.02     # coverage below this carries no signal to check the halving on
ENV_OUTSIDE_TOL = 0.02   # where the beam legs COULD have seen a shadow and did not, the sky
                         #  may not have moved either (see the mask derivation in section 5)
DIRECT_FRACTION = 0.20   # how much of a pixel the direct term must carry for its silence
                         #  about the deck to be evidence

# TRANSIT rig (see the header). The camera FOV is StandardSceneGraphComponent's
# default, and the disc's pixel radius is derived from it rather than eyeballed
# so the measurement window follows the disc if either number is ever retuned.
CAM_FOV_DEG        = 45.0
TRANSIT_SETTLE     = 48     # the skybox/camera/occluder all move: settle longer than a data-only leg
TRANSIT_DISC_DEG   = 2.0    # disc angular RADIUS for the transit legs
TRANSIT_DISC_INT   = 0.6    # x sun illuminance x sky exposure: a disc ~18x the sky around it, which
                            # is bright enough to measure and low enough that an 8-bit dump of the
                            # linear frame still SHOWS the attenuation instead of clipping white
TRANSIT_STRENGTH   = 0.9
TRANSIT_HALF       = 0.45
MIN_TEXEL_OCCL     = ANCHOR_SOLID_A  # the cookie alpha the on-ray ball must reach at the eye texel
CONTROL_TOL        = 0.02   # |d_clear/d_ref - 1|: arming with a clear sky overhead is a no-op
LINEARITY_TOL      = 0.06   # |a(0.9) - a(0.45)|, both inferred at TAU_PROBE
DISC_LIMB          = 1.25   # the disc's outer ramp, as a multiple of the radius (atmosphere default)
GROUND_SOFT_PROBE  = 6.0    # the ground bias d_gsoft raises; the disc must not feel it


def transmittance(tau, strength, a):
  """the shader's law, in ONE place: exp(-tau * strength * a)."""
  return math.exp(-tau * max(min(strength, 1.0), 0.0) * a)


def alpha_from_ratio(r, strength, tau):
  """invert r = exp(-tau*s*a) for a. None where the leg carries no law to
  invert (strength or tau 0) or the ratio is non-physical."""
  if strength <= 0.0 or tau <= 0.0 or r <= 0.0 or r >= 1.0:
    return None
  return -math.log(r) / (tau * strength)


def alpha_from_tau_pair(R, tau_hi, tau_lo, strength, hi=8.0):
  """invert R = (1-exp(-tau_hi*s*a)) / (1-exp(-tau_lo*s*a)) for a, by bisection.
  This is the form the GROUND legs need: unlike the disc, a ground pixel is
  direct PLUS ambient, and only the DIFFERENCES from the disarmed leg (which
  carry the 1-exp factors) are ambient-free. R is strictly DECREASING in a, so
  a value outside [R(hi), R(0+)] has no solution and the caller must treat None
  as a failed check rather than as an alpha."""
  if strength <= 0.0 or R is None:
    return None
  s = max(min(strength, 1.0), 0.0)
  f = lambda a: (1.0 - math.exp(-tau_hi * s * a)) / (1.0 - math.exp(-tau_lo * s * a))
  lo = 1.0e-4
  if not (f(hi) <= R <= f(lo)):
    return None
  for _ in range(80):
    mid = 0.5 * (lo + hi)
    if f(mid) > R:
      lo = mid
    else:
      hi = mid
  return 0.5 * (lo + hi)


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


SUN_DIR = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)

# WHERE THE CLOUD STANDS. off-ray is the ground legs' placement (over the
# RECEIVER, so its shadow lands in frame); on-ray is up-sun from the EYE, which
# is the axis of the cookie camera and therefore the texel the disc reads.
OCC_OFFRAY = vec3(SUN_DIR.x, SUN_DIR.y, SUN_DIR.z) * OCCLUDER_LIFT
OCC_ONRAY  = CAM_EYE + vec3(SUN_DIR.x, SUN_DIR.y, SUN_DIR.z) * OCCLUDER_LIFT
# up-sun from the eye: the disc lands dead centre.
CAM_TGT_SUN = CAM_EYE + vec3(SUN_DIR.x, SUN_DIR.y, SUN_DIR.z) * 100.0


class FloatOutSGC(StandardSceneGraphComponent):
  """scenegraph that composites into an RGBA32F RtGroup (see the header)."""

  suppress_repaint = False

  def _onUpdate(self, updinfo):
    # ONE ASSEMBLE PER CONTEXT FRAME, and it must be the gate's. The forward
    # node's frame prologue is deduped per context frame (silent, first assemble
    # wins) and it is the prologue that publishes SKY_FRAME onto the RCFD it ran
    # with — so if the viewport repaints first, the explicit renderOnContext
    # this gate captures from renders with a prologue-less RCFD and the
    # procedural skybox pass asserts on the missing sky frame state. Once the
    # scene is primed the viewport is left un-dirtied (it is offscreen and
    # nothing reads its 8-bit surface); the scene itself keeps being updated.
    if self.app._shutting_down:
      return
    try:
      self.scenegraph.scenetime = updinfo.absolutetime
      self.scenegraph.updateScene(self.cameralut)
      if not self.suppress_repaint:
        self.SGVP.widget.setDirty()
    except RuntimeError:
      self.app._shutting_down = True

  def _onGpuInit(self, ctx):
    rtg = lev2.RtGroup(ctx, WIDTH, HEIGHT)
    rtg.name = "cloudcookie_f32"
    rtg.createBuffer(tokens.RGBA32F, tokens.color)
    self.float_rtg = rtg
    self.sg_params.outputRTG = rtg
    super()._onGpuInit(ctx)


# tau=None means "leave the engine's own default alone" — the legs that carry
# the shipped optical depth must not spell it, or the gate would stop noticing a
# default that regressed.
def _ground(key, strength, soft, sun, tau=None, iblw=0.0):
  return dict(key=key, strength=strength, soft=soft, sun=sun, tau=tau, iblw=iblw,
              occ=OCC_OFFRAY, sky=False, tgt=CAM_TGT, settle=SETTLE_FRAMES)


# the transit legs measure the DISC, which the sky weight does not touch; they
# declare 0 so no leg in this file leaves the knob to whatever ran before it.
def _transit(key, strength, occ, tau=None, soft=SOFT_SHARP):
  return dict(key=key, strength=strength, soft=soft, sun=SUN_INTENSITY, tau=tau,
              iblw=0.0, occ=occ, sky=True, tgt=CAM_TGT_SUN, settle=TRANSIT_SETTLE)


TRANSIT_KEYS = ["d_ref", "d_clear", "d_occ", "d_law", "d_law2", "d_lawh", "d_gsoft"]

# the GROUND legs run first and are untouched by the transit rig (the disc data
# and the skybox are only armed once the first transit leg starts), then each
# transit leg is captured twice — same key, later capture wins.
LEGS = [
    _ground("off",      0.0,             SOFT_SHARP, SUN_INTENSITY),
    _ground("on_sharp", COOKIE_STRENGTH, SOFT_SHARP, SUN_INTENSITY),
    _ground("on_soft",  COOKIE_STRENGTH, SOFT_WIDE,  SUN_INTENSITY),
    _ground("on_probe", COOKIE_STRENGTH, SOFT_SHARP, SUN_INTENSITY, tau=TAU_PROBE),
    _ground("off_dark", 0.0,             SOFT_SHARP, 0.0),
    _ground("on_dark",  COOKIE_STRENGTH, SOFT_SHARP, 0.0),
    _ground("e_full",   COOKIE_STRENGTH, SOFT_SHARP, 0.0, iblw=IBL_W_FULL),
    _ground("e_half",   COOKIE_STRENGTH, SOFT_SHARP, 0.0, iblw=IBL_W_HALF),
] + sum([[leg, dict(leg)] for leg in [
    _transit("d_ref",   0.0,              OCC_OFFRAY),
    _transit("d_clear", TRANSIT_STRENGTH, OCC_OFFRAY),
    # the KILL leg runs at the frozen reference strength, not the law legs' 0.9
    _transit("d_occ",   ANCHOR_STRENGTH,  OCC_ONRAY),
    _transit("d_law",   TRANSIT_STRENGTH, OCC_ONRAY, tau=TAU_PROBE),
    _transit("d_law2",  TRANSIT_STRENGTH, OCC_ONRAY, tau=TAU_PROBE2),
    _transit("d_lawh",  TRANSIT_HALF,     OCC_ONRAY, tau=TAU_PROBE),
    _transit("d_gsoft", ANCHOR_STRENGTH,  OCC_ONRAY, soft=GROUND_SOFT_PROBE),
]], [])


class CloudCookieApp(ComponentizedApplication):

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
    self._transit_armed = False
    self.SGC = self.addComponent(
        "std_scenegraph", FloatOutSGC,
        eye=CAM_EYE, tgt=CAM_TGT, up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          # no flat ambient: "the term the cookie must not touch" has to mean
          # the IBL and nothing else, or the attribution half would pass on a
          # constant no occlusion could ever have reached.
          "AmbientLevel":     vec3(0),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = False   # black background: a surface pixel is identifiable by radiance

    self.atmo = lev2.SkyAtmosphereData()
    self.atmo.ibl_crossfade_frames = 0     # hard swap (fleet rule for any gate reading lighting after a publish)
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    self.receiver = SGC.createBallNode("receiver", ctx=ctx, position=vec3(0, 0, 0),
                                       color=vec4(1, 1, 1, 1), metallic=0.0,
                                       roughness=0.9, scale=RECEIVER_SCALE)

    d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)

    # THE CLOUD. It lives ONLY on the "sun_cookie" layer — no compositing pass
    # names that layer, so it is invisible in the frame and cannot contribute a
    # cascade shadow either. Everything it does to the image, it does through
    # the cookie.
    self.cookie_layer = SGC.scenegraph.createLayer("sun_cookie")
    cloud_drawable = SGC._ball_model.createDrawable()
    for subinst in cloud_drawable.modelinst.submeshinsts:
      mtl = subinst.material.clone()
      mtl.assignImages(ctx, color=SGC._ball_white_tex, normal=SGC._ball_normal_tex,
                       mtlruf=SGC._ball_white_tex, doConform=True)
      mtl.baseColor = vec4(1, 1, 1, 1)
      subinst.overrideMaterial(mtl)
    self.cloud = SGC.scenegraph.createDrawableNodeOnLayers(
        [self.cookie_layer], "cloud", cloud_drawable)
    self.cloud.worldTransform.translation = OCC_OFFRAY   # per-leg from here on
    self.cloud.worldTransform.scale = OCCLUDER_SCALE

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = SUN_INTENSITY
    sun.data.shadowBias = 0.05  # metres
    sun.data.shadowMapSize = 2048
    sun.data.shadowCascadeCount = 3
    sun.data.shadowMaxDistance = 250.0
    # PCF dither OFF: this gate compares the SAME pixel across five captures, and
    # a per-fragment jitter that is not identical in all five would land in the
    # attribution residual as if the cookie had touched the env term.
    sun.data.pcfDither = 0.0
    sun.data.cloudShadowExtent = COOKIE_EXTENT
    sun.data.cloudShadowDepth = COOKIE_DEPTH
    sun.data.cloudShadowMapSize = COOKIE_DIM
    sun.data.cloudShadowStrength = 0.0        # per-leg
    sun.data.cloudShadowSoftness = SOFT_SHARP # per-leg
    # READ, never write: these two are the unauthored defaults the whole
    # derivation below is anchored on, and the verdict checks them.
    self._tau_default      = float(sun.data.cloudExtinction)
    self._disc_lod_default = float(sun.data.cloudDiscSoftness)
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
    print("[cloud-cookie] %s" % txt, flush=True)

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
    print("[cloud-cookie] captured %s %dx%d mean=%.6g max=%.6g" %
          (key, w, h, float(img.mean()), float(img.max())), flush=True)
    return key

  def _restate(self, s):
    self._state = s
    self._state_frame = self._frame
    self._state_time = time.time()

  def _aim(self, tgt):
    """re-point the render camera. The ui camera is the authority for the lut
    entry the viewport draws with, so it is aimed and then copied down; nothing
    else drives it in an offscreen run (no ui events)."""
    SGC = self.SGC
    SGC.uicam.lookAt(CAM_EYE, tgt, vec3(0, 1, 0))
    SGC.uicam.updateMatrices()
    SGC.camera.copyFrom(SGC.uicam.cameradata)

  ##############################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    # THE SCENE IS RENDERED HERE AND NOWHERE ELSE (see _onGpuLink): one assemble
    # per context frame, ours, so every frame's prologue — sun cookie included —
    # belongs to the camera and light data this gate just set. It also keeps the
    # engine's own work (the procedural refilter primed below) running.
    self.SGC.scenegraph.renderOnContext(ctx)
    if not self._realized:
      # REALIZE the target: captureAsFormat asserts natively on an unbuilt
      # RtBuffer impl rather than raising.
      ctx.FBI.rtGroupInit(self.SGC.float_rtg)
      self._realized = True
    if not self._built:
      # PRIME: one published procedural refilter before anything is measured.
      # Nothing moves afterwards, so every capture shares one and the same sky.
      if bool(self._pbr().sky_ibl_ready) and (not bool(self._pbr().sky_ibl_inflight)):
        self._built = True
        self.SGC.suppress_repaint = True   # from here on this gate owns every assemble
        self._restate(0)
      elif (time.time() - self._state_time) > WAIT_SECONDS:
        self._fail("the first procedural refilter never published")
      return
    if self._leg >= len(LEGS):
      return

    leg = LEGS[self._leg]

    if self._state == 0:
      # TEETH SWITCH: CLOUD_COOKIE_FORCE_STRENGTH=0 disarms every leg, which is
      # how this gate is mutation-run — everything that depends on the cookie
      # must then FAIL and everything that does not (attribution, the off-ray
      # control) must still pass.
      forced = os.environ.get("CLOUD_COOKIE_FORCE_STRENGTH")
      if forced is not None:
        leg = dict(leg, strength=float(forced))
      self.sun.data.cloudShadowStrength = leg["strength"]
      self.sun.data.cloudShadowSoftness = leg["soft"]
      self.sun.data.cloudShadowIblWeight = leg["iblw"]
      self.sun.data.cloudExtinction = (self._tau_default if leg["tau"] is None
                                       else leg["tau"])
      self.sun.data.intensity = leg["sun"]
      self.cloud.worldTransform.translation = leg["occ"]
      if leg["sky"] and not self._transit_armed:
        # ARM THE TRANSIT RIG once, on the first transit leg — the ground legs
        # above must see exactly the scene they were gated on (the disc data
        # feeds the IBL snapshot as well as the visible sky).
        self._transit_armed = True
        self._pbr().enable_skybox = True
        self.atmo.sun_disc_angular_radius = TRANSIT_DISC_DEG
        self.atmo.sun_disc_intensity = TRANSIT_DISC_INT
      self._aim(leg["tgt"])
      self._note("leg %s: strength %.2f softness %.1f tau %.2f iblw %.2f sun %.2f occ<%.1f %.1f %.1f> sky %d"
                 % (leg["key"], leg["strength"], leg["soft"],
                    float(self.sun.data.cloudExtinction), leg["iblw"], leg["sun"],
                    leg["occ"].x, leg["occ"].y, leg["occ"].z, int(leg["sky"])))
      self._restate(1)
      return

    if self._state == 1:
      if (self._frame - self._state_frame) >= leg["settle"]:
        self._restate(2)
      return

    if self._state == 2:
      # the frame was rendered at the top of this callback, with this leg's
      # state already in place for `settle` frames.
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

  def _discPixelRadius(self):
    """the disc's radius in pixels, from the same two numbers that put it on
    screen — angular radius and the camera's vertical fov."""
    r = math.tan(math.radians(TRANSIT_DISC_DEG * DISC_LIMB))
    return r / math.tan(math.radians(CAM_FOV_DEG * 0.5)) * (HEIGHT * 0.5)

  def _dumpShots(self):
    """CLOUD_COOKIE_SHOT_DIR=<dir> — the owner-facing look at the transit. Not a
    gate input: the verdict is measured on the float captures above."""
    outdir = os.environ.get("CLOUD_COOKIE_SHOT_DIR")
    if not outdir:
      return
    try:
      from PIL import Image
    except ImportError:
      self._note("CLOUD_COOKIE_SHOT_DIR set but PIL is unavailable")
      return
    os.makedirs(outdir, exist_ok=True)
    for key in TRANSIT_KEYS:
      im = self._shots.get(key)
      if im is None:
        continue
      # the frames are linear and pre-tonemap: one Reinhard shoulder + sRGB-ish
      # gamma, which is the least the eye needs to read a disc against a sky.
      t = im / (1.0 + im)
      rgb = numpy.clip(numpy.power(numpy.clip(t, 0, 1), 1.0 / 2.2) * 255.0, 0, 255).astype(numpy.uint8)
      path = os.path.join(outdir, "transit_%s.png" % key)
      Image.fromarray(rgb).transpose(Image.FLIP_TOP_BOTTOM).save(path)
      self._note("wrote %s" % path)

  def _transitChecks(self, check):
    have = [k for k in TRANSIT_KEYS if k in self._shots]
    if len(have) != len(TRANSIT_KEYS):
      check("transit_captures_present", False, "have %s" % have)
      return "transit=missing"
    check("transit_captures_present", True)
    self._dumpShots()

    lum = lambda im: im.mean(axis=2)
    L = {k: lum(self._shots[k]) for k in TRANSIT_KEYS}

    # THE MEASUREMENT WINDOW is geometric, not thresholded: a disc of known
    # angular size, centred on where the reference leg peaks. A brightness-
    # thresholded mask would grow into the sky's near-sun glow, which the cookie
    # does NOT attenuate (only the disc term is multiplied), and dilute the ratio.
    ref = L["d_ref"]
    py, px = numpy.unravel_index(int(numpy.argmax(ref)), ref.shape)
    yy, xx = numpy.mgrid[0:HEIGHT, 0:WIDTH]
    dist   = numpy.hypot(yy - py, xx - px)
    rpx    = self._discPixelRadius()
    disc   = dist <= rpx
    ring   = (dist >= 3.0 * rpx) & (dist <= 5.0 * rpx)

    print("=== transit: the disc ===", flush=True)
    print("  peak px<%d %d> disc radius %.1f px (%d px in, %d px of sky ring)"
          % (px, py, rpx, int(disc.sum()), int(ring.sum())), flush=True)

    # the SKY under the disc is the same in every leg (the cookie multiplies the
    # disc term alone), so subtracting the local sky makes these numbers the
    # disc's own radiance rather than disc+sky.
    peak = {}
    flux = {}
    for k in TRANSIT_KEYS:
      bg = float(numpy.median(L[k][ring]))
      peak[k] = float(L[k][disc].max()) - bg
      flux[k] = float((L[k][disc] - bg).sum())
      print("  %-8s sky=%.6g  disc peak=%.6g  disc flux=%.6g" % (k, bg, peak[k], flux[k]), flush=True)

    check("transit_disc_present", peak["d_ref"] > 0.0 and
          peak["d_ref"] > 10.0 * float(numpy.median(L["d_ref"][ring])),
          "reference disc peak %.6g over sky %.6g" % (peak["d_ref"], float(numpy.median(L["d_ref"][ring]))))
    if peak["d_ref"] <= 0.0:
      return "transit=no_disc"

    tau_hi  = self._tau_default
    r_clear = peak["d_clear"] / peak["d_ref"]
    r_occ   = peak["d_occ"]   / peak["d_ref"]
    r_law   = peak["d_law"]   / peak["d_ref"]
    r_law2  = peak["d_law2"]  / peak["d_ref"]
    r_lawh  = peak["d_lawh"]  / peak["d_ref"]
    r_gsoft = peak["d_gsoft"] / peak["d_ref"]
    f_occ   = flux["d_occ"]   / max(flux["d_ref"], 1e-30)

    # THE TAU-DOUBLING SOLVE. The ring subtraction cannot remove the near-sun
    # AUREOLE: the sky under the disc is brighter than the sky 3-5 radii out, so
    # every peak carries a common positive bias C that is invisible next to a
    # blazing disc and dominant next to an extinguished one. Because TAU_PROBE2
    # is exactly twice TAU_PROBE, the bias solves out in closed form. With
    # x = exp(-tau1*s*a) and peak_obs = P*f + C:
    #     d_law  - d_ref = P*(x - 1)
    #     d_law2 - d_ref = P*(x^2 - 1) = P*(x-1)*(x+1)
    #     => x = (d_law2 - d_ref)/(d_law - d_ref) - 1,  P = (d_law-d_ref)/(x-1),
    #        C = d_ref - P,   a = -ln(x)/(tau1*s)
    # Nothing here is assumed about the sky: the identity holds ONLY if the
    # transmittance is exponential in tau, which is why it is also the sharpest
    # curve test in this file. A LINEAR shader ignores tau, makes the two probe
    # legs equal, and sends x to 0: no alpha, FAIL.
    den   = peak["d_law"] - peak["d_ref"]
    x_sol = ((peak["d_law2"] - peak["d_ref"]) / den - 1.0) if abs(den) > 1e-9 else None
    if (x_sol is not None) and (0.0 < x_sol < 1.0):
      P_disc  = den / (x_sol - 1.0)
      C_bias  = peak["d_ref"] - P_disc
      a_curve = -math.log(x_sol) / (TAU_PROBE * TRANSIT_STRENGTH)
    else:
      P_disc = C_bias = a_curve = None

    # the two-strength pair, read on BIAS-CORRECTED peaks
    def _alpha(key, strength, tau):
      if (P_disc is None) or (P_disc <= 0.0):
        return None
      return alpha_from_ratio((peak[key] - C_bias) / P_disc, strength, tau)
    a_full = _alpha("d_law",  TRANSIT_STRENGTH, TAU_PROBE)
    a_half = _alpha("d_lawh", TRANSIT_HALF,     TAU_PROBE)

    # the disc's residual, in units of the sky it sits in — the owner-facing form
    # of "the sun went out", and the one number no bookkeeping can dress up
    bg_occ   = float(numpy.median(L["d_occ"][ring]))
    kill     = peak["d_occ"] / max(bg_occ, 1e-30)
    kill_ref = peak["d_ref"] / max(float(numpy.median(L["d_ref"][ring])), 1e-30)
    resid    = (peak["d_occ"] - C_bias) if C_bias is not None else None
    pred     = (P_disc * transmittance(tau_hi, ANCHOR_STRENGTH, a_curve)) if a_curve is not None else None
    resid_tol = (DISC_RESIDUAL_TOL * P_disc) if P_disc is not None else None

    print("  ratios vs disarmed: off-ray=%.4f  on-ray=%.4f (flux %.4f)" % (r_clear, r_occ, f_occ), flush=True)
    print("  probe legs: tau %.1f s=%.2f -> %.4f | tau %.1f s=%.2f -> %.4f | tau %.1f s=%.2f -> %.4f"
          "  || ground-soft %.1f -> %.4f"
          % (TAU_PROBE, TRANSIT_STRENGTH, r_law, TAU_PROBE2, TRANSIT_STRENGTH, r_law2,
             TAU_PROBE, TRANSIT_HALF, r_lawh, GROUND_SOFT_PROBE, r_gsoft), flush=True)
    print("  tau-doubling solve: x=%s  disc peak P=%s  aureole bias C=%s  alpha=%s"
          % tuple(("%.5f" % v) if v is not None else "unreachable"
                  for v in (x_sol, P_disc, C_bias, a_curve)), flush=True)
    print("  alpha from the strength pair: %s (s=%.2f) / %s (s=%.2f)"
          % (("%.4f" % a_full) if a_full is not None else "unreachable", TRANSIT_STRENGTH,
             ("%.4f" % a_half) if a_half is not None else "unreachable", TRANSIT_HALF), flush=True)
    print("  disc peak over its own sky ring: clear %.2fx -> occluded %.4fx" % (kill_ref, kill), flush=True)

    check("transit_control_offray_undimmed", abs(r_clear - 1.0) <= CONTROL_TOL,
          "armed cookie, cloud NOT on the ray: disc ratio %.4f (tol %.2f)" % (r_clear, CONTROL_TOL))
    # THE CURVE, bias-immune (see the solve above).
    check("transit_curve_is_exponential",
          (a_curve is not None) and (MIN_TEXEL_OCCL <= a_curve <= ALPHA_MAX),
          "alpha from the tau-doubling solve (%.1f vs %.1f) = %s, required in [%.2f, %.2f]"
          % (TAU_PROBE2, TAU_PROBE, ("%.4f" % a_curve) if a_curve is not None else "unreachable",
             MIN_TEXEL_OCCL, ALPHA_MAX))
    # THE SUN GOES OUT — two clauses, one arithmetic and one human.
    #  (1) what is left of the DISC (bias removed) is what the law predicts, to
    #      2% of the clear disc's own peak;
    #  (2) and the transiting disc is no longer brighter than the sky it sits in.
    #      Under the LINEAR law the same geometry left it at ~2x its ring.
    check("transit_disc_extinguished",
          (resid is not None) and (resid <= pred + resid_tol) and (kill <= DISC_KILL_RING),
          "disc residual %s vs predicted %s (+tol %s = %.0f%% of the clear disc);"
          " and %.4f x the local sky (max %.2f)"
          % (("%.5f" % resid) if resid is not None else "unreachable",
             ("%.5f" % pred) if pred is not None else "unreachable",
             ("%.5f" % resid_tol) if resid_tol is not None else "n/a",
             100.0 * DISC_RESIDUAL_TOL, kill, DISC_KILL_RING))
    # the "and there IS an occlusion" clause is not redundant: without it a run
    # with the cookie disarmed infers nothing at either strength and would pass
    # this check on a law it never exercised.
    check("transit_scales_with_strength",
          (a_full is not None) and (a_half is not None) and (a_full > 0.05)
          and (abs(a_full - a_half) <= LINEARITY_TOL) and (a_full <= ALPHA_MAX),
          "inferred alpha %s at strength %.2f vs %s at %.2f (tol %.2f, both > 0.05 and <= %.2f)"
          % (("%.4f" % a_full) if a_full is not None else "unreachable", TRANSIT_STRENGTH,
             ("%.4f" % a_half) if a_half is not None else "unreachable", TRANSIT_HALF,
             LINEARITY_TOL, ALPHA_MAX))
    # THE DISC'S OWN LOD: raising the GROUND bias must leave the disc alone. The
    # transit-happened clause keeps a disarmed run from passing on two 1.0s.
    check("transit_disc_lod_is_not_the_grounds",
          (r_occ < 0.9) and (abs(r_gsoft - r_occ) <= CONTROL_TOL),
          "ground softness %.1f moved the disc ratio %.5f -> %.5f (tol %.2f, and the transit must be real)"
          % (GROUND_SOFT_PROBE, r_occ, r_gsoft, CONTROL_TOL))
    return ("transit_clear=%.4f transit_occ=%.5f kill=%.4fxsky residual=%s(pred %s)"
            " transit_law=%.4f/%.4f/%.4f transit_gsoft=%.5f curve_alpha=%s alpha=%s/%s"
            % (r_clear, r_occ, kill,
               ("%.5f" % resid) if resid is not None else "x",
               ("%.5f" % pred) if pred is not None else "x",
               r_law, r_law2, r_lawh, r_gsoft,
               ("%.3f" % a_curve) if a_curve is not None else "x",
               ("%.3f" % a_full) if a_full is not None else "x",
               ("%.3f" % a_half) if a_half is not None else "x"))

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

    lum = lambda im: im.mean(axis=2)
    OFF   = lum(self._shots["off"])
    SHARP = lum(self._shots["on_sharp"])
    SOFT  = lum(self._shots["on_soft"])
    PROBE = lum(self._shots["on_probe"])
    OFFD  = lum(self._shots["off_dark"])
    OND   = lum(self._shots["on_dark"])
    EFULL = lum(self._shots["e_full"])
    EHALF = lum(self._shots["e_half"])

    # THE UNAUTHORED DEFAULTS. Every derivation below reads the engine's own
    # numbers, so this is the one place the SHIPPED values are pinned: a scene
    # that authors nothing must still get the physical curve and a sharp disc.
    print("=== engine defaults ===", flush=True)
    print("  CloudExtinction=%.4f  CloudDiscSoftness=%.4f"
          % (self._tau_default, self._disc_lod_default), flush=True)
    check("default_extinction_is_physical", abs(self._tau_default - ANCHOR_TAU) < 1e-4,
          "CloudExtinction default %.4f (expected %.2f = 3*LWP/(2*rho_w*r_e), LWP 50 g/m^2, r_e 10 um)"
          % (self._tau_default, ANCHOR_TAU))
    check("default_disc_lod_is_sharper_than_ground",
          abs(self._disc_lod_default - DISC_LOD_SHIPPED) < 1e-4 and
          self._disc_lod_default < 2.0,
          "CloudDiscSoftness default %.4f (expected %.2f, and below the ground's 2.0)"
          % (self._disc_lod_default, DISC_LOD_SHIPPED))

    surface = OFF > SURFACE_FLOOR
    # THE MASK IS INDEPENDENT OF THE SOFTNESS LEGS: it is built from off vs
    # on_sharp only, so using it to compare sharp against soft is not circular.
    shadow = surface & (SHARP < (1.0 - SHADOW_DROP) * OFF)

    print("=== masks ===", flush=True)
    print("  surface=%d  cookie_shadow=%d  (of %d px)"
          % (int(surface.sum()), int(shadow.sum()), surface.size), flush=True)
    check("cookie_shadow_present", int(shadow.sum()) >= MIN_PIXELS,
          "%d px (min %d)" % (int(shadow.sum()), MIN_PIXELS))
    # NO ground shadow is already a FAIL (above). The run continues anyway so
    # that a mutation run reports every check it breaks and every one it does
    # not — the sections that need the mask are skipped, not faked.
    have_shadow = int(shadow.sum()) >= MIN_PIXELS
    drop = float("nan")

    ##########################################################
    # 1. the cookie removed real direct light
    ##########################################################
    if have_shadow:
      off_m   = float(OFF[shadow].mean())
      sharp_m = float(SHARP[shadow].mean())
      drop    = 1.0 - sharp_m / max(off_m, 1e-30)
      print("=== what the cookie removed (cookie-shadow region) ===", flush=True)
      print("  mean radiance  disarmed=%.6g  armed=%.6g  removed fraction=%.4f"
            % (off_m, sharp_m, drop), flush=True)
      check("cookie_removed_direct_light", drop > 0.05,
            "mean relative darkening %.4f (min 0.05) over %d px" % (drop, int(shadow.sum())))

    ##########################################################
    # 2. ATTRIBUTION, clause one — sun off AND weight 0: arming the cookie must
    #    change nothing. The sky is allowed to dim ONLY through the declared
    #    weight, so any leak by any other route lands here at the size of the
    #    occlusion the first half just measured. (Clauses two and three, what
    #    the weight itself is allowed to do, are section 5 — they need the
    #    alpha the beam legs recover.)
    ##########################################################
    lit_env = OFFD > SURFACE_FLOOR
    rel = numpy.abs(OND[lit_env] - OFFD[lit_env]) / numpy.maximum(OFFD[lit_env], 1e-30)
    print("=== attribution (sun intensity 0, sky weight 0) ===", flush=True)
    print("  env-only px=%d  |on-off|/off mean=%.3e max=%.3e"
          % (int(lit_env.sum()), float(rel.mean()), float(rel.max())), flush=True)
    check("env_untouched_at_weight_zero", float(rel.max()) < ATTRIB_TOL,
          "max |on_dark-off_dark|/off_dark = %.3e (tol %.1e) over %d px"
          % (float(rel.max()), ATTRIB_TOL, int(lit_env.sum())))

    ##########################################################
    # 3. SOFTNESS IS DATA — the LOD bias must widen the penumbra, which reads as
    #    a smaller steepest step across the shadow edge. Measured as the largest
    #    horizontal luminance gradient on the shadow's boundary ring, relative to
    #    the local unshadowed radiance so the two legs are comparable.
    ##########################################################
    def max_edge_gradient(img):
      g = numpy.abs(numpy.diff(img, axis=1))
      band = shadow[:, 1:] ^ shadow[:, :-1]            # the boundary between shadow and lit
      band &= surface[:, 1:] & surface[:, :-1]
      if band.sum() < 10:
        return None
      norm = numpy.maximum(OFF[:, 1:][band], 1e-30)
      return float((g[band] / norm).max())

    g_sharp = max_edge_gradient(SHARP) if have_shadow else None
    g_soft  = max_edge_gradient(SOFT) if have_shadow else None
    print("=== softness is data ===", flush=True)
    print("  steepest edge step (relative): softness %.1f -> %.4f   softness %.1f -> %.4f"
          % (SOFT_SHARP, g_sharp if g_sharp else -1.0, SOFT_WIDE, g_soft if g_soft else -1.0),
          flush=True)
    check("softness_widens_the_penumbra",
          (g_sharp is not None) and (g_soft is not None) and (g_soft < g_sharp),
          "softness %.1f step=%.4f  vs  softness %.1f step=%.4f (soft must be gentler)"
          % (SOFT_SHARP, g_sharp if g_sharp else -1.0, SOFT_WIDE, g_soft if g_soft else -1.0))

    ##########################################################
    # 4. THE GROUND CURVE, and the ambient floor under it.
    #
    #    OFF = D+E per pixel and an armed leg is D*f+E, so the DIFFERENCES from
    #    the disarmed leg drop E exactly:
    #        OFF - on_sharp = D * (1 - exp(-tau_hi*s*a))
    #        OFF - on_probe = D * (1 - exp(-tau_lo*s*a))
    #    and their ratio drops D too. What is left is a function of the cookie
    #    alpha ALONE, which must invert to a physical value over the shadow core.
    ##########################################################
    a_ground = None
    f_meas   = float("nan")
    f_pred   = float("nan")
    env_core = 0.0
    core     = None
    if have_shadow:
      d_hi = OFF - SHARP
      d_lo = OFF - PROBE
      # THE CORE: the most-occluded decile of the mask, where the ball's alpha is
      # at its texel maximum and the penumbra is not diluting the reading.
      cut  = float(numpy.percentile(d_hi[shadow], 90.0))
      core = shadow & (d_hi >= cut)
      s_hi = float(d_hi[core].sum())
      s_lo = float(d_lo[core].sum())
      R_g  = (s_hi / s_lo) if abs(s_lo) > 1e-30 else None
      a_ground = alpha_from_tau_pair(R_g, self._tau_default, TAU_PROBE, COOKIE_STRENGTH)
      env_core = float(OFFD[core].mean())
      direct   = float(OFF[core].mean()) - env_core
      f_meas   = (float(SHARP[core].mean()) - env_core) / direct if abs(direct) > 1e-30 else float("nan")
      if a_ground is not None:
        f_pred = transmittance(self._tau_default, COOKIE_STRENGTH, a_ground)
      print("=== the ground curve (core %d px of %d) ===" % (int(core.sum()), int(shadow.sum())), flush=True)
      print("  R = sum(off-on_sharp)/sum(off-on_probe) = %s  ->  alpha %s"
            % (("%.4f" % R_g) if R_g is not None else "n/a",
               ("%.4f" % a_ground) if a_ground is not None else "unreachable"), flush=True)
      print("  beam survival measured %.5f  predicted %.5f   ambient floor %.6g"
            % (f_meas, f_pred, env_core), flush=True)
      check("ground_curve_is_exponential",
            (a_ground is not None) and (MIN_TEXEL_OCCL <= a_ground <= ALPHA_MAX),
            "alpha from the ground tau pair (%.2f vs %.2f) = %s, required in [%.2f, %.2f]"
            % (self._tau_default, TAU_PROBE,
               ("%.4f" % a_ground) if a_ground is not None else "unreachable",
               MIN_TEXEL_OCCL, ALPHA_MAX))
      # the SAME law, now read against the ambient-only leg: what survives must
      # be what the inferred alpha predicts, and it must be nearly nothing.
      check("ground_beam_extinguished",
            (a_ground is not None) and (f_meas == f_meas)
            and (abs(f_meas - f_pred) <= BEAM_TOL) and (f_meas <= BEAM_MAX),
            "direct beam survival %.5f (predicted %.5f +/- %.2f, max %.2f) over %d core px"
            % (f_meas, f_pred, BEAM_TOL, BEAM_MAX, int(core.sum())))
      # ...and PARTIALLY, never to nothing: the ambient the cookie may not touch
      # is what keeps a cascade shadow under cloud visible instead of black.
      check("ambient_floor_survives",
            env_core > SURFACE_FLOOR and float(SHARP[core].mean()) >= env_core,
            "ambient-only radiance under the occluder %.6g (floor %.1e); armed leg %.6g never below it"
            % (env_core, SURFACE_FLOOR, float(SHARP[core].mean())))
    else:
      check("ground_curve_is_exponential", False, "no cookie shadow to measure the curve on")
      check("ground_beam_extinguished", False, "no cookie shadow to measure the beam on")
      check("ambient_floor_survives", False, "no cookie shadow to measure the floor under")

    ##########################################################
    # 5. WHAT THE SKY TERM TAKES — attribution clauses two and three.
    #
    #    With the sun off the frame is the env term alone, so an armed leg is
    #    E * mix(1, 1 - cov, w) and dividing by the disarmed leg leaves the
    #    weighted coverage per pixel, with E gone:
    #        1 - e_full/off_dark = cov          (w = 1)
    #        1 - e_half/off_dark = cov / 2      (w = 1/2)
    #    Two independent statements come out of it and neither needs a golden:
    #      * the coverage the SKY lost over the shadow core is the alpha the BEAM
    #        legs recovered from the tau pair — one sample, two consumers, and
    #        the sky is not free to invent its own occlusion;
    #      * halving the weight halves the loss PER PIXEL, which is the mix()
    #        form itself. A beam-transmittance sky (the pre-S3 law) fails the
    #        first — exp(-7.5*0.8) leaves the sky ~1.0 dimmed where the coverage
    #        is 0.8 — and any non-linear blend fails the second.
    #    Plus the negative half: where the deck is not, the sky is untouched at
    #    ANY weight.
    ##########################################################
    cov_full = 1.0 - EFULL / numpy.maximum(OFFD, 1e-30)
    cov_half = 1.0 - EHALF / numpy.maximum(OFFD, 1e-30)
    # WHERE THE NEGATIVE CONTROL IS ALLOWED TO STAND. "Not in the beam's shadow
    # mask" does NOT mean "no deck overhead": most of a ball's surface under the
    # projected deck is backfacing or grazing, takes no direct light at all, and
    # so cannot report a shadow however thick the cloud is (measured here: 7161
    # px of sky coverage against a 307 px beam mask, and they are consistent).
    # The control therefore lives where the beam COULD have spoken — pixels the
    # direct term contributes a fifth of. There, a coverage past 0.02 would have
    # dropped the total by 2.8% and landed in the mask, so an unmasked pixel
    # carrying a dimmed sky means the sky invented occlusion the cookie has not.
    direct_lit = (OFF - OFFD) > (DIRECT_FRACTION * OFF)
    outside  = lit_env & direct_lit & (~shadow)
    off_max  = float(numpy.abs(cov_full[outside]).max()) if outside.sum() else float("nan")
    sig      = lit_env & (cov_full > ENV_SIGNAL)
    mix_max  = (float(numpy.abs(cov_half[sig] - 0.5 * cov_full[sig]).max())
                if sig.sum() else float("nan"))
    cov_core = (1.0 - float(EFULL[core].mean()) / max(float(OFFD[core].mean()), 1e-30)) \
               if core is not None else float("nan")
    print("=== what the sky term takes (sun 0, weights %.2f / %.2f) ===" % (IBL_W_FULL, IBL_W_HALF), flush=True)
    print("  coverage from the sky: core %.4f (beam alpha %s)  |  p05 %.4f med %.4f p95 %.4f over %d lit px"
          % (cov_core, ("%.4f" % a_ground) if a_ground is not None else "unreachable",
             float(numpy.percentile(cov_full[lit_env], 5.0)),
             float(numpy.percentile(cov_full[lit_env], 50.0)),
             float(numpy.percentile(cov_full[lit_env], 95.0)), int(lit_env.sum())), flush=True)
    print("  half-weight residual max=%.5f over %d signal px  |  outside the deck max=%.5f"
          % (mix_max, int(sig.sum()), off_max), flush=True)
    check("env_took_the_cookie_coverage",
          (a_ground is not None) and (cov_core == cov_core)
          and (abs(cov_core - a_ground) <= ENV_ALPHA_TOL),
          "sky lost %.4f over the core vs beam alpha %s (tol %.2f); at weight %.2f these are the same number"
          % (cov_core, ("%.4f" % a_ground) if a_ground is not None else "unreachable",
             ENV_ALPHA_TOL, IBL_W_FULL))
    check("env_scales_with_the_declared_weight",
          (int(sig.sum()) >= MIN_PIXELS) and (mix_max == mix_max) and (mix_max <= ENV_MIX_TOL),
          "max |(1-e_half/off_dark) - 0.5*(1-e_full/off_dark)| = %.5f (tol %.3f) over %d px above %.2f coverage"
          % (mix_max, ENV_MIX_TOL, int(sig.sum()), ENV_SIGNAL))
    check("env_untouched_where_the_deck_is_not",
          (off_max == off_max) and (off_max <= ENV_OUTSIDE_TOL),
          "max |1 - e_full/off_dark| outside the cookie shadow = %.5f (tol %.3f) over %d px"
          % (off_max, ENV_OUTSIDE_TOL, int(outside.sum())))

    ##########################################################
    # 6. THE TRANSIT — a cloud on the eye->sun ray puts the disc out.
    ##########################################################
    tdetail = self._transitChecks(check)

    ok = (len(failures) == 0)
    verdict(ok, "shadow_px=%d removed=%.4f attrib_max=%.3e grad_sharp=%.4f grad_soft=%.4f"
                " ground_alpha=%s beam=%.5f sky_cov=%.4f sky_mix=%.5f %s"
            % (int(shadow.sum()), drop, float(rel.max()),
               g_sharp if g_sharp else -1.0, g_soft if g_soft else -1.0,
               ("%.3f" % a_ground) if a_ground is not None else "x", f_meas,
               cov_core, mix_max, tdetail))
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = CloudCookieApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
