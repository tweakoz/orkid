###############################################################################
# Scene directional SUN + cascaded shadows
#
# Split out of scene/__init__.py for readability. Mixed into Scene via multiple
# inheritance (class Scene(TerrainMixin, WalkerMixin, ProjectilesMixin, SunMixin)
# in __init__.py), so the public API is unchanged: self.sun(...) works.  Methods
# reference self.* (entity / declare_component / SG / _ensure_system / ...)
# provided by the core Scene + sibling mixins.
#
# A sun is a bare lev2.DirectionalLightData dropped in as a SceneGraph node's
# `drawable`: SceneGraphSystem polymorphically detects the LightData and injects
# a Light instead of a mesh (SceneGraphSystem.cpp _onStageComponent). The light's
# direction is the ENTITY's world +Z (Light::direction()), so aiming the sun is
# just the entity orientation — computed here from (elevation, azimuth).
###############################################################################

import json as _json
import os as _os

from orkengine.core import vec3
from orkengine import lev2

from ork.hypergraph.ecs.scene._helpers import Transform, elevation_azimuth_quat
from ork.hypergraph.ecs.scene import _celestial


def _publish_celestial_config(name, cfg):
  """Validate a celestial config and register it under an ENTITY NAME.

  The registry is the AUTHOR-TIME one: Scene.moon() and Scene.stars() read the
  sun's entry back out of it to inherit its site and clock. It lives in the
  process environment (read-modify-write, so several suns — or several scenes
  declared in one process — accumulate instead of clobbering).

  The SIM does NOT get the config from here. Each celestial component carries
  its own config on PythonComponentData.scriptData (see
  _celestial_component()), which is reflected and therefore rides the .ecs
  across the two-process author->player recipe that the environment cannot."""
  blob = _os.environ.get(_celestial.CONFIG_ENV_KEY, "")
  table = _json.loads(blob) if blob else {}
  table[name] = _celestial.normalize_config(cfg)
  _os.environ[_celestial.CONFIG_ENV_KEY] = _json.dumps(table)
  return table[name]


def _celestial_component(scene, script_name, cfg):
  """A PythonComponent running one of the celestial behavior scripts, with the
  entity's OWN config attached as the component's scriptData.

  scriptData is the reflected channel — it serializes into the .ecs, so the
  player process reads exactly what the authoring process declared. Nothing
  about a celestial light may be left to the environment: the shipped playback
  recipe authors and plays in two different processes."""
  scene._ensure_system("PythonSystem")
  script = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), script_name)
  return scene.declare_component("PythonComponent",
                                 scriptFile = script,
                                 scriptData = _json.dumps(cfg))


class SunMixin:
  """Scene directional sun light + cascaded shadow maps."""

  def sun(self, name="sun", *, color=vec3(1), intensity=4.0,
          elevation=45.0, azimuth=30.0, cascades=4,
          shadow_map_size=2048, shadow_max_distance=250.0,
          shadow_bias=0.05, pcf_dither=1.0,
          shadow_snapshot_interval=None,
          shadow_refresh_angle_deg=None, shadow_refresh_distance=None,
          shadow_refresh_max_secs=None,
          shadow_snapshot_bands_per_frame=None, shadow_crossfade_frames=None,
          shadow_crossfade_secs=None,
          shadow_band_radius=None, shadow_band_ratio=None,
          shadow_band_res_ratio=None, shadow_jitter_texels=None,
          cullsets=None, band_cullsets=None,
          shadow_caster=True, priority=10.0,
          cloud_shadow_strength=None, cloud_shadow_extent=None,
          cloud_shadow_softness=None, cloud_shadow_depth=None,
          cloud_shadow_map_size=None, cloud_shadow_refresh_frames=None,
          cloud_extinction=None, cloud_disc_softness=None,
          cloud_ibl_weight=None, cascade_ibl_weight=None, cascade_floor=None,
          animate_orbit=None, celestial=None):
    """Declare a directional SUN: a DirectionalLightData carried as the
    `drawable` of a SceneGraph node, aimed by the entity orientation.

    color / intensity        — radiance of the sun (reflected LightData).
    elevation / azimuth       — sky angle in DEGREES; sets the entity
                                orientation so the light travels along +Z
                                (see _helpers.elevation_azimuth_quat).

                                AZIMUTH IS NOT A COMPASS BEARING. It is degrees
                                about world +Y measuring the direction the light
                                TRAVELS, with 0 = travelling toward +Z (which is
                                SOUTH in the engine frame: +X east, +Y up, +Z
                                south). The sun is therefore on the OPPOSITE
                                side of the sky from the number:

                                    travel  = ( cos(el)*sin(az),
                                               -sin(el),
                                                cos(el)*cos(az) )
                                    to-sun  = -travel
                                    compass bearing of the sun = -azimuth
                                                                 (mod 360)

                                So azimuth=285 puts the sun at bearing 75 —
                                EAST-north-east, a morning sun — not the WNW
                                afternoon 285 reads as. Two shipped scenes were
                                written with the compass reading in mind and
                                rendered the mirrored one for months; if what
                                you want is a bearing B, pass azimuth=-B.
                                ELEVATION is unaffected by azimuth (the +Y
                                rotation leaves the vertical component alone),
                                so an elevation that looks wrong is never a
                                convention problem here.

                                The celestial path uses the same convention:
                                CelestialSnapshot.sun_light_angles() returns
                                (elevation, -astronomical_azimuth) precisely so
                                its output can be passed straight in.
    cascades                  — shadowCascadeCount (number of world bands, 2..5).
                                The ladder is geometric off shadow_band_radius /
                                shadow_band_ratio, so a fifth band is a km-scale
                                one at the stock knobs (10/40/160/640/2560 m) —
                                what the haze march and the distant ground need
                                to be shadowed at all. Past 5 is a hard error,
                                not a clamp.
    shadow_map_size           — per-cascade shadow map resolution.
    shadow_max_distance       — toward-light extrusion depth of each band's ortho
                                box (how far a caster may stand off a band and
                                still cast into it). NOT the coverage radius —
                                the bands below set that.
    shadow_band_radius        — radius of the innermost world band, in meters
    shadow_band_ratio           (default 10), and the multiplier between
                                successive bands (default 4): 10/40/160/640m.
                                Bands are spheres centered on the VIEWER, so the
                                fit is independent of where the camera looks and
                                each band's world texel size is a constant.
                                None = the engine defaults.
    shadow_band_res_ratio     — resolution STEP between bands: band i renders at
                                shadow_map_size / ratio^i (floored at 256) into a
                                viewport sub-rect of its slice, so the near band
                                keeps the full dim and the outer bands — whose
                                texels are already meters wide — stop paying for
                                one. None/1 = every band at the full dim, the
                                shipped behavior. Storage is unchanged either way
                                (one array at the near band's dim).
    shadow_jitter_texels      — per-SNAPSHOT sub-texel jitter of the fit origin,
                                in texels. Successive snapshots sample the
                                penumbra at different sub-texel offsets and the
                                crossfade blends them, which supersamples the
                                shadow edge over time. Requires
                                shadow_crossfade_frames > 0 (a hard swap would
                                show the offset as crawl, so the engine forces it
                                to 0 there). None/0 = off, the shipped fit.
    cullsets / band_cullsets  — CULLSETS: which caster families each band's
                                shadow pass looks at. A cullset is a NAMED list
                                of families and every band subscribes to exactly
                                one; the engine culls a set ONCE against the
                                union of only ITS bands' radii and draws only its
                                families in those bands.

                                    cullsets      = {"near": ["terrain",
                                                              "instanced",
                                                              "other"],
                                                     "far":  ["terrain"]},
                                    band_cullsets = ["near","near","near",
                                                     "near","far"],

                                The families are the engine's: "terrain" (the
                                terrain chunks), "instanced" (GPU-culled
                                scattered instances — a forest's trees, grass,
                                rocks), "other" (everything else: props, models,
                                particles), and "all" as shorthand for the three.
                                band_cullsets has one name PER BAND, so its
                                length must equal `cascades`.

                                WHY: with one union volume and one survivor list,
                                a km-scale outer band drags every scattered
                                instance in that whole radius into EVERY band's
                                depth pass. Subscribing the outer band to a
                                terrain-only set leaves the canopy in the near
                                set's small volume and lets the far band draw
                                mountains alone. Declaring neither is the engine
                                default: one implicit all-families set, byte for
                                byte the behavior of a scene that never heard of
                                cullsets.
    shadow_bias / pcf_dither  — constant depth bias in WORLD METRES (the slack
                                under which a receiver stays lit; the evaluator
                                converts it per band, so one number means the
                                same distance in every cascade) + PCF dither.
    shadow_snapshot_interval  — SECONDS between cascade snapshots. The cascade
                                fit, the shadow cull and the per-cascade depth
                                passes are held between ticks and the maps are
                                sampled as they stand; None/0 = every frame.
                                THE NUMBER IS THE PERIOD — nothing shortens it:
                                not a sweeping sun, not a walking viewer. Only a
                                caster flip (sun->moon) or a shadow_map_size /
                                band tunable edit refits early, because those
                                change the maps themselves. The SUN is never
                                held (direction, color and cookie are written
                                every frame). What a hold DOES freeze is the
                                shadows of moving casters — a walking
                                character's shadow is stuck until the next tick
                                — and the fit's viewer anchor, so a long
                                interval on a scene the camera travels far
                                across will out-run its own near band.
                                Scene.moon() INHERITS this value (one sky, one
                                cadence: the moon holds the cascade all night).
    shadow_refresh_angle_deg / shadow_refresh_distance / shadow_refresh_max_secs
                              — the REFRESH GATE, consulted only when no
                                interval is declared above. Undeclared, a
                                cadence of "every frame" redraws four identical
                                depth passes for a viewer standing still under a
                                still sun; the gate takes a new snapshot when the
                                light has TURNED past the angle (default 0.1
                                deg), or the viewer has WALKED past the distance
                                from the anchor the bands are centered on
                                (default 0.5 m), or the caster set has changed.
                                NOTHING MOVED = NO REFIT: a frozen sun over a
                                parked viewer does zero cascade work. The
                                ceiling is the CASTERS' clock — wind and walkers
                                move nothing the other triggers can see — and it
                                is OFF by default (0), because a wall-clock
                                refit costs a full fit + cull + four depth
                                passes for shimmer in leaves; declare a rate to
                                buy that back. All three at 0 disarms the gate
                                entirely: refit every frame, the pre-gate
                                behavior. A declared interval overrides the lot
                                (the declaration is the cadence).
    shadow_snapshot_bands_per_frame — how many cascade bands ONE frame may
                                render. None/0 = all of them (the whole snapshot
                                lands in the frame it is taken, which is what
                                the engine ships). 1 spreads a 4-band snapshot
                                over 4 frames: the LIVE maps stay sampled the
                                whole time and the new set is published only
                                once every band is drawn, so the cost of a
                                snapshot stops being one spike.
    shadow_crossfade_frames   — frames over which a published snapshot fades in
                                over the one it replaces (the shader blends the
                                two shadow FACTORS). None/0 = hard swap, the
                                shipped behavior. This is the knob that hides
                                the flip; it costs a second shadow evaluation
                                for the length of the window only.
    shadow_crossfade_secs     — the WALL-CLOCK length of that window. Frames are
                                not a duration: 12 of them are 24 ms in an
                                unthrottled offscreen loop and 200 ms at 60 Hz,
                                so without a clock the smoothing (and its cost)
                                changed with the frame rate. Above 0 the blend
                                ramps on this clock and the frame count only
                                arms it; 0 = ride the frames. Engine default
                                0.2 s.
                                All three are inherited by Scene.moon(), like
                                the cadence above.
    cloud_shadow_strength     — 0..1, how much of the cloud decks' occlusion
                                reaches the ground (and dims this body's disc).
                                None/0 = OFF, which is the default and costs
                                nothing: no cookie pass runs at all. The decks
                                cast through their OWN shader — the engine draws
                                the "sun_cookie" layer from a sun-aligned ortho
                                camera and multiplies the result into the DIRECT
                                sun term only (never the IBL/ambient), so the
                                decks and the ground can never disagree about
                                how thick a cloud is.
    cloud_shadow_extent       — half-width in METERS of the cookie's ortho
                                window about the viewer (default 4000). The
                                cookie covers a square 2x this on a side; beyond
                                it the sun is unoccluded.
    cloud_shadow_softness     — mip LOD bias on the cookie sample. Cloud shadows
                                are big soft patches and must read FUZZIER than
                                any ground-object shadow in the same frame; this
                                is the knob that says by how much (default 2).
    cloud_shadow_depth        — toward-light extrusion of the cookie's ortho box
                                (meters). Must comfortably clear the deck
                                altitude at the lowest sun the scene shows.
    cloud_shadow_map_size     — cookie resolution (default 512, mipped). Softness
                                comes from the LOD bias, not from starving this.
    cloud_shadow_refresh_frames — how many frames ONE cookie fill serves
                                (default 4; 1 = refill every frame). The fill
                                redraws every deck and regenerates a mip chain
                                to capture a kilometers-wide slab of cloud that
                                is then read through a blurring LOD bias —
                                nothing in it changes in a frame. The held
                                cookie keeps its own matrix, so the shadow stays
                                put on the ground while the viewer moves; a
                                DISARM (strength 0, decks gone) is never held.
    cloud_extinction          — the BEAM's Beer-Lambert optical depth at full
                                cookie occlusion: transmittance = exp(-tau*a).
                                It is the beam's, not the ground's and not the
                                disc's — the DIRECT sun term and the sun/moon
                                DISC are one and the same beam and apply this
                                one law. Default 7.5, the optical depth of a
                                thin fair-weather cumulus (tau = 3*LWP /
                                (2*rho_w*r_e) with LWP 50 g/m^2, r_e 10 um), so
                                a solid deck puts the sun out (0.05% through)
                                while a wisp at a=0.1 still passes 47% and
                                grades. Raise it for heavier cloud, lower it for
                                haze; 0 makes the deck transparent (use
                                cloud_shadow_strength to disarm, not this).
    cloud_disc_softness       — mip LOD bias for the DISC's cookie sample only
                                (default 0.5). The ground wants a wide penumbra;
                                a transit edge across the sun is crisp in life,
                                so the disc reads a much sharper tap than
                                cloud_shadow_softness gives the dirt.
    cloud_ibl_weight          — how much of the CLOUD shadow reaches the
                                ambient/IBL term (default 0.65). A cloud
                                occludes a broad wedge of sky, so it dims the
                                sky light itself; with the env term carrying
                                most of a daylit frame's energy, a cookie
                                confined to the direct beam does not read at
                                all. Short of 1 on purpose: sky light still
                                arrives from beyond the cloud.
    cascade_ibl_weight        — the same dial for the CASCADE shadow, default 0
                                and meant to stay there: a tree occludes the sun
                                disc, not the dome, and "cascades never touch
                                the env term" is an asserted engine invariant.
                                Nonzero is scene experimentation.
    shadow_caster             — whether the sun casts shadows at all.
    priority                  — rank among directional lights. The renderer
                                sorts them descending by this and resolves the
                                sky source and the single shadow cascade slot
                                through the top-ranked one, so a secondary
                                directional (e.g. a moon) declares a LOWER
                                priority and only takes over when the sun is
                                switched off.

    animate_orbit=<seconds>   — when set, attaches a PythonComponent running
                                _sun_orbit.py so the sun sweeps a full azimuth
                                orbit at the declared elevation. NOTE: the orbit
                                PERIOD is a module constant in _sun_orbit.py
                                (PythonComponentData reflects only a script path,
                                exactly like Scene.spinner() / _spin.py) — the
                                elevation/azimuth start pose DOES reach the script
                                via the entity transform. Any non-None value
                                enables the orbit.

    celestial=<dict>          — when set, the sun is aimed every frame by the
                                OBSERVER-FRAME EPHEMERIS (_celestial.py) instead
                                of a spin: a real solar arc for the declared site
                                and date, with the right noon elevation and the
                                right day length. Keys (all optional, the site
                                and clock half of _celestial.CONFIG_DEFAULTS):
                                latitude_deg, longitude_deg, day_of_year,
                                time_of_day, time_scale, year, plus the authored
                                lunar placement moon_phase /
                                moon_initial_elevation / moon_orbit_rate, which
                                is ephemeris too and so travels with the site and
                                clock onto Scene.moon() (_celestial.py, MOON
                                PLACEMENT) — the remaining
                                config keys (body, base_intensity) are the DSL's
                                and declaring them here is an error. The
                                elevation/azimuth kwargs still set the FIRST-frame
                                pose but the model overrides it on frame one.
                                Mutually exclusive with animate_orbit. The
                                evaluated sun/moon/phase values are republished on
                                the entity varmap each frame, along with the
                                night-policy scale that drives this light's
                                intensity and shadow casting (_celestial_orbit.py,
                                _night_policy.py). Scene.moon() reuses whatever is
                                declared here."""
    if celestial is not None and animate_orbit is not None:
      raise ValueError(
        f"Scene.sun({name!r}): animate_orbit and celestial both declared — "
        f"they are two different aiming laws for one light. Pick one.")

    light = lev2.DirectionalLightData()
    light.color              = color
    light.intensity          = float(intensity)
    light.priority           = float(priority)
    light.sky_body           = 1  # SUN: the procedural sky's sun disc reads this
    light.shadowCaster       = bool(shadow_caster)
    light.shadowCascadeCount = int(cascades)
    light.shadowMapSize      = int(shadow_map_size)
    light.shadowMaxDistance  = float(shadow_max_distance)
    light.shadowBias         = float(shadow_bias)
    light.pcfDither          = float(pcf_dither)
    if shadow_snapshot_interval is not None:
      light.shadowSnapshotInterval = float(shadow_snapshot_interval)
    # Refresh gate — only consulted when no interval is declared (see the
    # engine header): what makes an UNDECLARED cadence take a new snapshot.
    if shadow_refresh_angle_deg is not None:
      light.shadowRefreshAngleDeg = float(shadow_refresh_angle_deg)
    if shadow_refresh_distance is not None:
      light.shadowRefreshDistance = float(shadow_refresh_distance)
    if shadow_refresh_max_secs is not None:
      light.shadowRefreshMaxSecs = float(shadow_refresh_max_secs)
    if shadow_snapshot_bands_per_frame is not None:
      light.shadowSnapshotBandsPerFrame = int(shadow_snapshot_bands_per_frame)
    if shadow_crossfade_frames is not None:
      light.shadowCrossfadeFrames = int(shadow_crossfade_frames)
    if shadow_crossfade_secs is not None:
      light.shadowCrossfadeSecs = float(shadow_crossfade_secs)
    # Band geometry: None means "whatever the engine ships"
    # — the defaults live in DirectionalLightData and are not copied here, so
    # there is one place to change them.
    if shadow_band_radius is not None:
      light.shadowBandRadius = float(shadow_band_radius)
    if shadow_band_ratio is not None:
      light.shadowBandRatio = float(shadow_band_ratio)
    if shadow_band_res_ratio is not None:
      light.shadowBandResRatio = float(shadow_band_res_ratio)
    if shadow_jitter_texels is not None:
      light.shadowJitterTexels = float(shadow_jitter_texels)
    # CULLSETS. Declared as a dict + a list here and carried as two flat strings
    # on the light (which is what rides the reflected .ecs into the player
    # process). Validated on BOTH sides: this side so a typo is a python error at
    # the authoring line that made it, the engine side because a hand-edited .ecs
    # must fail just as loudly.
    if (cullsets is None) != (band_cullsets is None):
      raise ValueError(
        f"Scene.sun({name!r}): cullsets and band_cullsets go together — sets with "
        f"no per-band subscription list (or the reverse) name nothing.")
    if cullsets is not None:
      known = {"terrain", "instanced", "other", "all"}
      if not isinstance(cullsets, dict) or not cullsets:
        raise ValueError(f"Scene.sun({name!r}): cullsets must be a non-empty dict "
                         f"of set-name -> list of family tokens.")
      decls = []
      for setname, families in cullsets.items():
        if isinstance(families, str):
          families = [families]
        if not families:
          raise ValueError(f"Scene.sun({name!r}): cullset {setname!r} declares no "
                           f"families — a band subscribed to it would draw nothing.")
        for fam in families:
          if fam not in known:
            raise ValueError(f"Scene.sun({name!r}): cullset {setname!r} names unknown "
                             f"caster family {fam!r} — known: {sorted(known)}")
        decls.append(f"{setname}=" + ",".join(families))
      if isinstance(band_cullsets, str):
        band_cullsets = [band_cullsets]
      if len(band_cullsets) != int(cascades):
        raise ValueError(f"Scene.sun({name!r}): band_cullsets has "
                         f"{len(band_cullsets)} entries but cascades={int(cascades)} "
                         f"— one set name PER BAND.")
      for bandname in band_cullsets:
        if bandname not in cullsets:
          raise ValueError(f"Scene.sun({name!r}): band subscribes to cullset "
                           f"{bandname!r}, which is not declared in cullsets.")
      # Older engine binaries predate the reflected cull-set properties (the
      # cascade-cullsets slice). A declaring scene on such a binary should say WHY
      # it cannot honor the declaration and keep the classic every-caster shadows,
      # not die in reflection with a bare AttributeError deep under Scene.sun().
      if not hasattr(light, "shadowCullSets"):
        print(f"[SUN] Scene.sun({name!r}): this engine binary predates shadow "
              f"cull-sets (DirectionalLightData.shadowCullSets missing) — "
              f"declarations IGNORED, every band draws all casters. Run against a "
              f"staging built from this source to get the declared cull-sets.",
              flush=True)
      else:
        light.shadowCullSets     = ";".join(decls)
        light.shadowBandCullSets = ",".join(band_cullsets)
    # Cloud shadows: strength 0 (the engine default) disarms the whole cookie
    # path, so a scene that says nothing here pays nothing.
    if cloud_shadow_strength is not None:
      light.cloudShadowStrength = float(cloud_shadow_strength)
    if cloud_shadow_extent is not None:
      light.cloudShadowExtent = float(cloud_shadow_extent)
    if cloud_shadow_softness is not None:
      light.cloudShadowSoftness = float(cloud_shadow_softness)
    if cloud_shadow_depth is not None:
      light.cloudShadowDepth = float(cloud_shadow_depth)
    if cloud_shadow_map_size is not None:
      light.cloudShadowMapSize = int(cloud_shadow_map_size)
    if cloud_shadow_refresh_frames is not None:
      light.cloudShadowRefreshFrames = int(cloud_shadow_refresh_frames)
    if cloud_extinction is not None:
      light.cloudExtinction = float(cloud_extinction)
    if cloud_disc_softness is not None:
      light.cloudDiscSoftness = float(cloud_disc_softness)
    if cloud_ibl_weight is not None:
      light.cloudShadowIblWeight = float(cloud_ibl_weight)
    if cascade_ibl_weight is not None:
      light.cascadeShadowIblWeight = float(cascade_ibl_weight)
    if cascade_floor is not None:
      light.cascadeShadowFloor = float(cascade_floor)

    # ENSEMBLE CADENCE. The snapshot interval belongs to the SKY, not to one
    # light: at night the MOON holds the cascade, so a cadence that died at the
    # handoff would deliver nothing exactly when it is needed. Scene.moon()
    # inherits it off THIS light — the same way it already inherits the site and
    # clock — keyed by the name it already names in sun=. One authoring surface
    # (sky()/sun()), the property still per-light underneath.
    self._celestial_sun_lights = getattr(self, "_celestial_sun_lights", {})
    self._celestial_sun_lights[name] = light

    # Aim = entity orientation (Light::direction() == entity world +Z).
    q = elevation_azimuth_quat(elevation, azimuth)

    components = [self.SG.component(nodes={name: {"drawable": light}})]

    if animate_orbit is not None:
      # Same wiring as Scene.spinner(): a per-entity PythonComponent whose
      # onUpdate re-aims the sun each frame. The orbit reads its start pose
      # from the entity transform (below) and its period from _sun_orbit.py.
      self._ensure_system("PythonSystem")
      script = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)),
                             "_sun_orbit.py")
      components.append(self.declare_component("PythonComponent",
                                               scriptFile=script))

    if celestial is not None:
      # Same wiring, different aiming law. The config rides the component's
      # scriptData (reflected -> in the .ecs), and is ALSO registered under this
      # entity's name so Scene.moon()/Scene.stars() can inherit the site+clock.
      # body/base_intensity are the per-light half of the config: the script
      # aims by the SUN's angles and scales the DECLARED intensity by the night
      # policy's sun curve (see _night_policy.py).
      cfg = dict(celestial)
      reserved = {"body", "base_intensity"} & set(cfg)
      if reserved:
        raise KeyError(
          f"Scene.sun({name!r}): celestial key(s) {sorted(reserved)} are set by "
          f"the DSL, not by the scene — a sun aims the sun at its declared "
          f"intensity. Declare a moon with Scene.moon().")
      cfg["body"] = "sun"
      cfg["base_intensity"] = float(intensity)
      cfg = _publish_celestial_config(name, cfg)
      # First-frame pose from the model, so frame 0 is already the real sky.
      q = _celestial.CelestialModel.from_config(cfg).at(0.0).sun_quat()
      components.append(_celestial_component(self, "_celestial_orbit.py", cfg))

    return self.entity(name,
                       transform  = Transform(orientation=q),
                       components = components)
