###############################################################################
# _celestial_sky.py — ONE call for the procedural-sky celestial ensemble: the
# observer-frame SUN (Scene.sun(celestial=...)) plus the MOON and STAR DOME that
# read its published site/clock. Every scene wearing the 60-second procedural
# day repeated the same three-call block with identical site defaults; this
# mixin IS that block, with the site/clock exposed as explicit kwargs and the
# moon/stars as toggles. No new behavior — a thin composition over the existing
# sun()/moon()/stars() mixins (the moon and stars re-read the sun's published
# celestial config, so they share its site and clock verbatim).
#
# DEFAULTS = the procsky gauge day: mid-latitude (45N) late-summer (day 220),
# 24h compressed into 180 wall seconds (time_scale = 86400/180 = 480), started
# just before dawn (time_of_day 4.0) so a run opens on sunrise.
###############################################################################


class CelestialSkyMixin:

  def celestial_sky(self, *,
                    latitude_deg = 45.0,
                    day_of_year  = 220.0,
                    time_of_day  = 4.0,
                    # 480 = 86400/180, a full day in 180 wall seconds.
                    # PROVISIONAL, the owner's words: "for now (until we trust
                    # the sky rendering is complete)". A MITIGATION, not a fix —
                    # the sun slows from ~6 to ~2 deg per wall second; the
                    # publish-chain latency is unchanged, the lag defect stands.
                    time_scale   = 480.0,
                    year         = None,
                    phase        = None,
                    moon_initial_elevation = None,
                    moon_orbit_rate = 1.0,
                    moon         = True,
                    stars        = True,
                    sun_color    = None,
                    sun_intensity = None,
                    sun_params   = None,
                    time_controls = True):
    """Declare the sun+moon+stars celestial ensemble for the procedural sky.

    The site/clock kwargs are the sun's celestial= config (observer-frame
    ephemeris, _celestial.py). The moon and star dome read that same published
    config, so they inherit the site and clock with no extra arguments.

    latitude_deg — observer latitude (degrees N).
    day_of_year  — 1..365; sets the solar declination (the season).
    time_of_day  — decimal hours on the sky clock at t=0 (the run's opening).
    time_scale   — sim-seconds of sky clock per wall-second (480 = 24h / 180s).
    year         — calendar year the day belongs to. The sun's arc repeats every
                   year; the MOON does not, which is why an undeclared moon is
                   only the moon that year had.

    THE MOON, DECLARED — the three below OVERRIDE the ephemeris at t=0 and
    nothing after it: the moon then propagates physically from that start, so a
    declared sky is a real sky, just not this year's. They re-epoch the lunar
    elements (_celestial.py, MOON PLACEMENT), which means they compose — a
    phase still lands exactly under any orbit rate — and they cost nothing at
    runtime (the solve happens once, at scene-declaration time).

    phase        — synodic phase at t=0: 'new' / 'first_quarter' / 'full' /
                   'last_quarter', or a 0..1 fraction. This is the ONE moon
                   brightness control: it moves the illuminated fraction, which
                   the night policy turns into the moon light's intensity, which
                   is what the sky's moon disc and its moonlit scatter read.
                   phase='new' therefore puts the moon out of the sky as well as
                   out of the shading, through that one path.
    moon_initial_elevation — moon altitude in degrees at t=0. Alone, it places
                   the moon anywhere it can reach that hour. TOGETHER WITH A
                   PHASE it is constrained: the phase pins the moon's longitude
                   and only its 5.15-degree orbital latitude is left, so the pair
                   holds inside a ~10-degree band and RAISES with the band's
                   numbers outside it. Inside the band it is worth declaring —
                   it is what pins the moon's declination, i.e. how high the
                   moon of that phase climbs.
    moon_orbit_rate — SYNODIC-period scale: 1.0 = Earth's moon (29.53 days),
                   2.0 = lunations twice as fast. Another world's moon is this
                   number, not another module.
    moon         — declare the MOON light (shares the sun's site/clock).
    stars        — declare the STAR DOME (shares the sun's site/clock).
    sun_color / sun_intensity — the sun light's RADIANCE, None = Scene.sun()'s
                   own defaults. Aiming is shared by construction (one sky), but
                   brightness is a per-scene balance against that scene's IBL
                   gains and grade — a scene that tuned its sun against a dimmer
                   frame states it here instead of losing it to the ensemble.
    sun_params   — dict of ANY other Scene.sun() kwarg (shadow_map_size,
                   shadow_caster, cascades, priority, ...). Radiance has the two
                   shortcuts above because every scene tunes it; the rest of the
                   sun's surface rides this dict rather than growing a kwarg
                   apiece. Aiming keys (elevation/azimuth/animate_orbit/
                   celestial) are the ensemble's and are refused.
    time_controls — attach the LIVE clock (sky_time_system.py): scrub / pause /
                   speed / date at runtime, from the keyboard and from
                   controller messages. On by default because an ensemble whose
                   hour can only be chosen at launch is the workflow this
                   replaces; the clock it publishes evaluates the AUTHORED
                   instant until something asks it for another one, so an
                   untouched scene renders exactly the sky it did before."""

    sun_kwargs = dict(sun_params or {})
    aiming = {"elevation", "azimuth", "animate_orbit", "celestial"} & set(sun_kwargs)
    if aiming:
      raise KeyError(
        f"celestial_sky(sun_params={{...}}): {sorted(aiming)} aim the sun, and "
        f"the ensemble does that from the site and clock. Drop them, or declare "
        f"a standalone Scene.sun() instead of the ensemble.")
    if sun_color is not None:
      sun_kwargs["color"] = sun_color
    if sun_intensity is not None:
      sun_kwargs["intensity"] = float(sun_intensity)

    cfg = {"latitude_deg": latitude_deg,
           "day_of_year":  day_of_year,
           "time_of_day":  time_of_day,
           "time_scale":   time_scale,
           "moon_orbit_rate": moon_orbit_rate}
    # UNDECLARED means undeclared: an absent key keeps _celestial.py's default,
    # rather than this layer restating it and drifting from it.
    if year is not None:
      cfg["year"] = year
    if phase is not None:
      cfg["moon_phase"] = phase
    if moon_initial_elevation is not None:
      cfg["moon_initial_elevation"] = moon_initial_elevation

    declared_moon = [k for k, v in (("phase", phase),
                                    ("moon_initial_elevation",
                                     moon_initial_elevation),
                                    ("moon_orbit_rate",
                                     None if moon_orbit_rate == 1.0 else
                                     moon_orbit_rate)) if v is not None]
    if declared_moon and not moon:
      raise ValueError(
        f"celestial_sky({', '.join(declared_moon)}=..., moon=False) — there is "
        f"no moon to place. The declaration reaches the sky through the MOON "
        f"LIGHT's intensity, so without one it would do nothing.")

    self.sun(celestial = cfg, **sun_kwargs)
    if moon:
      self.moon()
    if stars:
      self.stars()
    if time_controls:
      self.sky_time_controls()

  def sky_time_controls(self, script=None):
    """Attach the LIVE sky clock: scrub / pause / speed / date on a running
    scene, from the keyboard AND from controller messages (the two go through
    one implementation — see sky_time_system.py for the keymap and the message
    vocabulary). Composable: it is an APPENDED system script, so it layers onto
    whatever input script the scene already declared.

    script — point at your own copy to re-map the keys (the keymap is data in
    the script, walk_input_system.py's contract). Default is the shipped one.

    The ensemble attaches this for you; call it directly on a scene that
    declares its own celestial Scene.sun() instead of the ensemble."""
    import os as _os
    path = script or _os.path.join(_os.path.dirname(_os.path.abspath(__file__)),
                                   "sky_time_system.py")
    self.append_system_script(path)
    return path
