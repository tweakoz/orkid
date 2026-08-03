###############################################################################
# _night_policy.py — the DAY/NIGHT SHADOW HANDOFF: how bright the sun and the
# moon are for a given sky, and WHICH ONE holds the scene's single shadow
# cascade. PURE MATH, ZERO IMPORTS — so it loads by path in the ECS sim
# SUBINTERPRETER (see _celestial_orbit.py) and under a bare python3 in
# obt.project/unittests/night_policy.py.
#
# THE LAW (owner ruling): at most ONE shadow caster ever. The sun holds it while
# it is up; the moon may take it once the sun is gone; sun down with no usable
# moon means shadows cleanly OFF. Intensity crossfades so no handoff pops.
#
# HOW THE "NEVER POPS" PART IS STRUCTURAL, not tuned: every caster gate sits at
# the FOOT of the ramp that drives that body's intensity, so wherever a caster
# flips, that body's own intensity scale is ~0 and the flip is invisible:
#     sun  casts off at SUN_SHADOW_OFF_ELEVATION_DEG, inside the bottom of the
#          sun fade band (scale ~0.011 there)
#     moon casts on only above MOON_DARK_ELEVATION_DEG (foot of its elevation
#          ramp), only above MOON_DARK_ILLUMINATION (foot of its illumination
#          gate), and only once the sun is below MOON_DAYLIGHT_SUN_ELEVATION_DEG
#          (foot of its daylight-suppression ramp) — which is the SAME angle the
#          sun stops casting at, so the handoff itself starts the moon from 0.
# obt.project/unittests/night_policy.py holds the curves to that invariant on a
# dense sweep; moving a constant below without re-running it is how the pop
# comes back.
#
# The scales are UNITLESS 0..1 multipliers on the light's DECLARED intensity —
# the per-frame component multiplies and writes ent.vars.light_intensity.
###############################################################################

###############################################################################
# tech-artist tunables
###############################################################################

# Sun fade band: full daylight at/above, fully dark at/below (civil twilight-ish).
SUN_FULL_ELEVATION_DEG = 2.0
SUN_DARK_ELEVATION_DEG = -6.0

# Sun releases the cascade here — inside the bottom of the fade band, so the
# shadows vanish while the sunlight is already ~1% of full.
SUN_SHADOW_OFF_ELEVATION_DEG = -5.5

# Moon elevation ramp: nothing at/below the horizon, full contribution once well
# clear of it. Its foot IS the moon's shadow-cast threshold.
MOON_DARK_ELEVATION_DEG = 0.0
MOON_FULL_ELEVATION_DEG = 10.0

# Moon illumination gate: a thin crescent lights (and shadows) nothing. Foot IS
# the illumination threshold for casting.
MOON_DARK_ILLUMINATION = 0.15
MOON_FULL_ILLUMINATION = 0.35

# Moon daylight suppression: the moon stays black until the sun has released the
# cascade, then comes up through twilight. The high edge is pinned to
# SUN_SHADOW_OFF_ELEVATION_DEG on purpose — that pin is what makes the sun->moon
# caster handoff start the moon from exactly zero.
MOON_DAYLIGHT_SUN_ELEVATION_DEG = SUN_SHADOW_OFF_ELEVATION_DEG
MOON_NIGHT_SUN_ELEVATION_DEG = -18.0

# Below this scale the moon is not worth a cascade (guards the exact-zero foot).
MOON_CAST_MIN_SCALE = 1.0e-4

# The crossfade invariant the unit tests enforce: a caster may only flip where
# that body's intensity scale is under this.
HANDOFF_SCALE_LIMIT = 0.05

###############################################################################


def _smoothramp(x, x0, x1):
  """0 at x0, 1 at x1, smoothstep between — C1 at both ends, so a product of
  these never kinks. x1 may lie BELOW x0 (a descending ramp)."""
  if x1 == x0:
    return 1.0 if x >= x1 else 0.0
  t = (x - x0) / (x1 - x0)
  if t <= 0.0:
    return 0.0
  if t >= 1.0:
    return 1.0
  return t * t * (3.0 - 2.0 * t)


def sun_intensity_scale(sun_elevation_deg):
  """0..1 multiplier on the sun's declared intensity."""
  return _smoothramp(float(sun_elevation_deg),
                     SUN_DARK_ELEVATION_DEG, SUN_FULL_ELEVATION_DEG)


def moon_intensity_scale(sun_elevation_deg, moon_elevation_deg, moon_illumination):
  """0..1 multiplier on the moon's declared intensity: risen x lit x dark-sky.

  The lit term is the literal illuminated fraction TIMES a gate that zeroes the
  crescents — physical falloff above MOON_FULL_ILLUMINATION, nothing below the
  gate's foot."""
  illumination = float(moon_illumination)
  illumination = 0.0 if illumination < 0.0 else (1.0 if illumination > 1.0 else illumination)
  risen = _smoothramp(float(moon_elevation_deg),
                      MOON_DARK_ELEVATION_DEG, MOON_FULL_ELEVATION_DEG)
  lit = illumination * _smoothramp(illumination,
                                   MOON_DARK_ILLUMINATION, MOON_FULL_ILLUMINATION)
  dark_sky = _smoothramp(float(sun_elevation_deg),
                         MOON_DAYLIGHT_SUN_ELEVATION_DEG, MOON_NIGHT_SUN_ELEVATION_DEG)
  return risen * lit * dark_sky


def night_policy(sun_elevation_deg, moon_elevation_deg, moon_illumination):
  """The whole ruling for one instant of sky:

      sun_intensity_scale  / moon_intensity_scale  — 0..1, continuous
      sun_casts            / moon_casts            — bool, NEVER both true

  Elevations in degrees (astronomical altitude, + above the horizon),
  illumination the moon's lit disc fraction 0..1 — exactly the fields a
  CelestialSnapshot reports."""
  sun_scale = sun_intensity_scale(sun_elevation_deg)
  moon_scale = moon_intensity_scale(sun_elevation_deg, moon_elevation_deg,
                                    moon_illumination)

  sun_casts = float(sun_elevation_deg) > SUN_SHADOW_OFF_ELEVATION_DEG
  moon_casts = ((not sun_casts)
                and float(moon_elevation_deg) > MOON_DARK_ELEVATION_DEG
                and float(moon_illumination) > MOON_DARK_ILLUMINATION
                and moon_scale > MOON_CAST_MIN_SCALE)

  return {
    "sun_intensity_scale":  sun_scale,
    "moon_intensity_scale": moon_scale,
    "sun_casts":            sun_casts,
    "moon_casts":           moon_casts,
  }


__all__ = ["night_policy", "sun_intensity_scale", "moon_intensity_scale",
           "HANDOFF_SCALE_LIMIT"]
