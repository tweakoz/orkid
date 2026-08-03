###############################################################################
# Scene directional MOON — the night half of the day/night shadow handoff.
#
# Sibling of _sun.py, mixed into Scene the same way (class Scene(..., SunMixin,
# MoonMixin) in __init__.py), so self.moon() is the public API. Same mechanism as
# a sun: a lev2.DirectionalLightData carried as a SceneGraph node's `drawable`,
# aimed by the entity orientation, re-aimed every frame by _celestial_orbit.py —
# with body="moon" in its celestial config, so the script uses the snapshot's
# MOON angles.
#
# WHY IT HAS NO celestial= OF ITS OWN: a moon with a different site or clock than
# its sun is a wrong scene (two skies, one sky). Scene.moon() REQUIRES a prior
# Scene.sun(celestial=...) and reuses that published site/clock verbatim.
#
# WHO CASTS: nobody decides here. The moon declares shadowCaster (so the light is
# built with a cascade rig) and _night_policy.py decides per frame whether the
# cascade is actually the sun's, the moon's, or nobody's — at most one, ever, and
# only where the corresponding intensity has faded out.
###############################################################################

from orkengine.core import vec3
from orkengine import lev2

from ork.hypergraph.ecs.scene._helpers import Transform
from ork.hypergraph.ecs.scene import _celestial
from ork.hypergraph.ecs.scene._sun import (_publish_celestial_config,
                                           _celestial_component)

import os as _os
import json as _json

# Moonlight is reflected sunlight — physically near-white and slightly RED. This
# cool blue-white is the perceptual convention instead (scotopic vision reads dim
# scenes as blue), which is what a night scene is expected to look like.
MOON_COLOR = vec3(0.62, 0.72, 1.0)

# ~1/10 of the Scene.sun() intensity default (4.0). The real ratio is ~1/400000,
# but against the scenes' flat AmbientLight(0.1) anything physically scaled is
# invisible — vet measured a 0.01 moon at <=2% of night ground level, zero
# directional signature. 0.4 direct vs 0.1 ambient reads as moonlight shading;
# the day-night exposure slice owns the real balance and the owner tunes it.
# Either way this is a tone-mapped stand-in — a moonlit scene must still be a
# visible scene, and the eye's own night adaptation is not modeled.
MOON_INTENSITY = 0.4

# Below the sun's default 10.0, so the sky/atmosphere source always resolves
# through the SUN (LightManager sorts directionals descending by priority and
# every single-directional pick reads index [0]).
MOON_PRIORITY = 5.0


class MoonMixin:
  """Scene directional moon light — night shadow caster, policy-gated."""

  def moon(self, name="moon", *, sun="sun", color=MOON_COLOR,
           intensity=MOON_INTENSITY, priority=MOON_PRIORITY,
           cascades=4, shadow_map_size=2048, shadow_max_distance=250.0,
           shadow_bias=2e-4, pcf_dither=1.0,
           shadow_caster=True):
    """Declare the MOON for a scene that already declared a celestial sun.

    sun            — name of the Scene.sun(celestial=...) whose site and clock
                     this moon shares. Absent from the published config table =
                     hard error (a moon needs a sky, and it must be THE sky).
    color          — dim cool white (see MOON_COLOR).
    intensity      — DECLARED intensity; the night policy scales it per frame
                     (risen x lit x dark-sky), so this is the full-moon,
                     high-in-a-dark-sky value.
    priority       — rank among directionals; stays BELOW the sun's.
    shadow_caster  — builds the cascade rig. Whether the moon actually casts on a
                     given frame is _night_policy.py's call, not this flag's."""
    blob = _os.environ.get(_celestial.CONFIG_ENV_KEY, "")
    table = _json.loads(blob) if blob else {}
    sun_cfg = table.get(sun)
    if sun_cfg is None:
      raise KeyError(
        f"Scene.moon({name!r}): no celestial config published for sun {sun!r} "
        f"(declared: {sorted(table)}) — declare Scene.sun(celestial={{...}}) "
        f"first; the moon shares its site and clock rather than owning a second "
        f"sky.")

    # Site + clock verbatim from the sun; body/base_intensity are ours.
    cfg = {k: sun_cfg[k] for k in _celestial.MODEL_CONFIG_KEYS}
    cfg["body"] = "moon"
    cfg["base_intensity"] = float(intensity)
    cfg = _publish_celestial_config(name, cfg)

    light = lev2.DirectionalLightData()
    light.color              = color
    light.intensity          = float(intensity)
    light.priority           = float(priority)
    light.sky_body           = 2  # MOON: the procedural sky's moon disc reads this
    light.shadowCaster       = bool(shadow_caster)
    light.shadowCascadeCount = int(cascades)
    light.shadowMapSize      = int(shadow_map_size)
    light.shadowMaxDistance  = float(shadow_max_distance)
    light.shadowBias         = float(shadow_bias)
    light.pcfDither          = float(pcf_dither)

    # Cascade SNAPSHOT CADENCE inherited from the sun's light, exactly as the
    # site and clock are: one sky, one cadence, and the moon is THE caster for
    # the whole night the knob was declared for. No kwarg of its own — a moon
    # that wants a different cadence than its sky is data (edit the light), not
    # a second authoring surface.
    sun_light = getattr(self, "_celestial_sun_lights", {}).get(sun)
    if sun_light is None:
      raise KeyError(
        f"Scene.moon({name!r}): sun {sun!r} has a published celestial config but "
        f"no light on THIS scene — a moon and its sun must be declared on the "
        f"same Scene (the shadow cadence, the site and the clock are one sky's).")
    light.shadowSnapshotInterval = sun_light.shadowSnapshotInterval
    # ...and how that snapshot is PAID FOR: the amortization granularity and the
    # flip crossfade are the same sky's, for the same reason.
    light.shadowSnapshotBandsPerFrame = sun_light.shadowSnapshotBandsPerFrame
    light.shadowCrossfadeFrames       = sun_light.shadowCrossfadeFrames
    light.shadowCrossfadeSecs         = sun_light.shadowCrossfadeSecs
    # Same reasoning for the world-band geometry: the bands are the SKY's
    # shadow rig, and the moon renders into the same one when it takes the
    # cascade at night. Inherited, no second authoring surface.
    light.shadowBandRadius           = sun_light.shadowBandRadius
    light.shadowBandRatio            = sun_light.shadowBandRatio
    light.shadowBandResRatio         = sun_light.shadowBandResRatio
    light.shadowJitterTexels         = sun_light.shadowJitterTexels
    # CLOUD SHADOWS are the same sky's too — one deck, one cookie rig. It is the
    # moon that projects it all night (the cascade holder owns the cookie), so a
    # moon that did not inherit these would drop the clouds' shadow at dusk and
    # leave the moon disc shining through a cloud it is behind.
    light.cloudShadowStrength = sun_light.cloudShadowStrength
    light.cloudShadowExtent   = sun_light.cloudShadowExtent
    light.cloudShadowSoftness = sun_light.cloudShadowSoftness
    light.cloudShadowDepth    = sun_light.cloudShadowDepth
    light.cloudShadowMapSize  = sun_light.cloudShadowMapSize
    light.cloudExtinction     = sun_light.cloudExtinction
    light.cloudDiscSoftness   = sun_light.cloudDiscSoftness

    # First-frame pose from the model, so frame 0 is already the real sky.
    q = _celestial.CelestialModel.from_config(cfg).at(0.0).moon_quat()

    return self.entity(name,
                       transform  = Transform(orientation=q),
                       components = [
                         self.SG.component(nodes={name: {"drawable": light}}),
                         _celestial_component(self, "_celestial_orbit.py", cfg)])
