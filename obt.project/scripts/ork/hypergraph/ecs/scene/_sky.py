###############################################################################
# _sky.py — ONE call for a whole SKY: the sky/IBL scenegraph params
# (_sky_dome.py), the celestial ensemble that lights it (_celestial_sky.py:
# sun + moon + star dome on the shared observer-frame day) and, optionally, the
# layered cloud decks (_cloud_deck.py). Every procedural-sky scene was assembling
# those three by hand, with the same literals; this mixin IS that assembly.
#
# PLUS the TONE stage the frame is encoded through, whose scene adaptation is
# driven engine-side by the MEASURED luminance of the published environment
# (PostFxNodeACES). Nothing about it is declared per tick from here: the sky
# publishes, the stage reads what it published.
#
# THE BAR (owner): a scene's sky should differ from another scene's only by its
# DEFAULT TIME OF DAY and its CLOUD COVER. Those two are the first two kwargs;
# everything else has a working default, and the passthroughs exist for the
# scenes that still need to say something the library can't yet infer.
###############################################################################

import os as _os


class SkyMixin:

  def sky(self, *,
          time_of_day  = 4.0,
          cloud_cover  = None,
          latitude_deg = 45.0,
          day_of_year  = 220.0,
          # 480 = 86400/180, a full day in 180 wall seconds. PROVISIONAL, the
          # owner's words: "for now (until we trust the sky rendering is
          # complete)". A MITIGATION, not a fix — the sun slows from ~6 to ~2
          # deg per wall second; the publish-chain latency is unchanged and the
          # lag defect stands.
          time_scale   = 480.0,
          year         = None,
          phase        = None,
          moon_initial_elevation = None,
          moon_orbit_rate = 1.0,
          celestial    = True,
          moon         = True,
          stars        = True,
          time_controls = True,
          sun_color    = None,
          sun_intensity = None,
          shadow_snapshot_interval = None,
          sun_params   = None,
          clouds       = False,
          cloud_params = None,
          tonemap      = None,
          **dome_params):
    """Declare the scene's sky: dome/IBL params + celestial ensemble + decks.

    time_of_day  — decimal hours on the sky clock at t=0 (the run's opening).
    cloud_cover  — sky-cover fraction 0..1; implies clouds=True. None with
                   clouds=True = the deck launch state from the env knobs.
    latitude_deg / day_of_year / year / time_scale — the site and clock (see
                   _celestial_sky.py; 480 = a 24h day in 180 wall seconds).
    phase        — the moon's phase at t=0: 'new' / 'first_quarter' / 'full' /
                   'last_quarter', or a 0..1 fraction. DECLARING IT is what
                   makes a night reproducible: undeclared, the moon is whatever
                   the real one did on the declared date of the declared year,
                   so the scene's night changes when the date does. It moves the
                   moon's illuminated fraction, hence the moon light's
                   intensity, hence the sky's moon disc and moonlit scatter —
                   one path, so phase='new' darkens all of them together.
    moon_initial_elevation — moon altitude in degrees at t=0. WITH a phase it is
                   constrained to a ~10-degree band (the phase already pins the
                   moon's longitude) and raises with that band's numbers if the
                   pair cannot hold; inside it, it is what fixes how high the
                   moon rides that night. Restated against the scene's own
                   time_of_day: re-cutting the clock moves the band with it.
    moon_orbit_rate — synodic-period scale, 1.0 = Earth's moon. Another world's
                   moon is this number.
                   See _celestial_sky.py for all four, and _celestial.py's MOON
                   PLACEMENT for what they move.
    celestial    — declare the sun/moon/stars ensemble. False leaves the scene
                   to declare its own light (a static or orbiting sun).
    moon / stars — ensemble members (both share the sun's site and clock).
    time_controls — attach the LIVE clock (scrub / pause / speed / date at
                   runtime, keys + controller messages; see sky_time_system.py).
                   On by default; an untouched clock evaluates the authored
                   instant, so the rendered sky is unchanged until asked.
    sun_color / sun_intensity — the sun light's radiance; None = Scene.sun()'s
                   defaults (see _celestial_sky.py — one sky, but the sun's
                   brightness balances against the scene's own IBL gains).
    shadow_snapshot_interval — SECONDS between cascade snapshots for the whole
                   ENSEMBLE: the sun and the moon that takes the cascade off it
                   at night (see Scene.sun(); None/0 = every frame). Spelled
                   out as a named
                   kwarg rather than left to sun_params because the ensemble's
                   cadence is not a scene param: spelled anywhere else it is an
                   unknown kwarg, which **dome_params now REFUSES (sky_dome's
                   SCENE_PARAM_KEYS) rather than dropping at declareParams.
    sun_params   — dict of any OTHER Scene.sun() kwarg (shadow_map_size,
                   shadow_caster, cascades, ...); the aiming keys are refused.
    clouds       — declare the cloud decks.
    cloud_params — dict forwarded to cloud_decks() (alt_offset_m, sag_datum_m,
                   sun_dir, colors, ...).
    tonemap      — attach the ACES tone stage (which carries the scene
                   adaptation). None (default) = ON for the procedural-sky
                   family, the whole set of scenes that HAVE a night; see below.
                   A DICT turns it on AND authors the stage's knobs — the
                   adaptation curve's three luminance anchors and three values
                   plus the authored exposure (_sky_dome.TONEMAP_KNOBS). The
                   dead-of-night one is
                   tonemap={"adapt_floor": ...}; it acts only below the twilight
                   anchor, so it cannot disturb the early night. An unknown key
                   RAISES — this dict never reaches declareParams, where an
                   unrecognized key would be silently dropped.
    **dome_params — forwarded to sky_dome() (sky_source, skybox_path, the four
                   exposure knobs, msaa/ssaa, ...).

    Returns the SceneGraphHandle."""

    # the cadence knob rides sun_params (Scene.sun() owns it) — merged here, and
    # loud on both ways of saying it twice or of saying it to nobody.
    sun_params = dict(sun_params or {})
    if shadow_snapshot_interval is not None:
      if "shadow_snapshot_interval" in sun_params:
        raise ValueError(
          "Scene.sky(shadow_snapshot_interval=...) AND "
          "sun_params['shadow_snapshot_interval'] — one cadence, one spelling.")
      if not celestial:
        raise ValueError(
          "Scene.sky(shadow_snapshot_interval=..., celestial=False) — the knob "
          "lives on the ensemble's sun, and this sky declares no sun. Pass it to "
          "the scene's own Scene.sun() instead.")
      sun_params["shadow_snapshot_interval"] = float(shadow_snapshot_interval)

    want_clouds = clouds or (cloud_cover is not None)
    if cloud_params and not want_clouds:
      raise ValueError(
        "Scene.sky(cloud_params=...) with no decks — pass clouds=True or a "
        "cloud_cover, or drop the cloud_params.")

    # WHO GETS THE TONE STAGE: the PROCEDURAL sky family. Its adaptation is a
    # function of the environment the sky publishes, so it needs no celestial
    # ensemble to be correct — a static procedural sky measures its own
    # luminance like any other. A baked-envmap gauge is lit by a constant and
    # has no night to adapt to. A scene overrides either way with tonemap=.
    if tonemap is None:
      tonemap = (dome_params.get("sky_source", "procedural") == "procedural")

    handle = self.sky_dome(tonemap=tonemap, **dome_params)
    # The sky's celestial half is the ensemble's; declaring any of it with no
    # ensemble to carry it is a scene that will silently not do what it says.
    orphaned = [k for k, v in (("year", year), ("phase", phase),
                               ("moon_initial_elevation",
                                moon_initial_elevation),
                               ("moon_orbit_rate",
                                None if moon_orbit_rate == 1.0
                                else moon_orbit_rate)) if v is not None]
    if orphaned and not celestial:
      raise ValueError(
        f"Scene.sky({'=..., '.join(orphaned)}=..., celestial=False) — those "
        f"belong to the sun/moon/stars ensemble, and this sky declares none. "
        f"Drop them, or turn the ensemble on.")

    if celestial:
      self.celestial_sky(latitude_deg  = latitude_deg,
                         day_of_year   = day_of_year,
                         time_of_day   = time_of_day,
                         time_scale    = time_scale,
                         year          = year,
                         phase         = phase,
                         moon_initial_elevation = moon_initial_elevation,
                         moon_orbit_rate = moon_orbit_rate,
                         moon          = moon,
                         stars         = stars,
                         time_controls = time_controls,
                         sun_color     = sun_color,
                         sun_intensity = sun_intensity,
                         sun_params    = sun_params)
    if want_clouds:
      self.cloud_decks(cover=cloud_cover, **(cloud_params or {}))
    return handle

