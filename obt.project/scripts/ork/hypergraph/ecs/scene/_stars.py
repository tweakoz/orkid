###############################################################################
# Scene NIGHT STAR DOME — the star field that fades in through twilight and
# turns with the sidereal clock.
#
# Sibling of _sun.py / _moon.py, mixed into Scene the same way, so self.stars()
# is the public API. Three pieces:
#   * the FIELD — assets/mesh/star_catalog.py, one camera-agnostic splat quad per
#     Bright Star Catalogue star, baked at LOCAL radius and blown up to the sky
#     radius by entity scale (the mesh pipeline mangles multi-km coordinates).
#   * the LOOK — assets/materials/star_splat.py, an UNLIT ptex3d material: an
#     analytic 2D gaussian per star, ADDITIVE (order-independent — no sort over
#     9096 overlapping quads), faded out whenever the sun is up.
#   * the AIM — _star_dome.py on a PythonComponent, which turns the entity by
#     tilt(pole) * spin(-sidereal) every frame.
#
# WHY IT HAS NO celestial= OF ITS OWN (same law as Scene.moon): stars over a
# different site or clock than the sun are a second sky. Scene.stars() REQUIRES a
# prior Scene.sun(celestial=...) and republishes that site/clock verbatim under
# the dome's entity name — the channel the sim subinterpreter can actually read
# (see _celestial_orbit.py's header). The config's `body` key stays at its "sun"
# default and is never consulted: the dome aims no light, it only needs the
# latitude and the clock.
#
# The dome node lands on std_transparent with the depth-prepass SKIPPED — a
# translucent sky shell must never z-occlude the sky or the clouds — and it
# never writes depth (the material's rasterstate), so terrain still occludes it.
###############################################################################

import json as _json
import os as _os

from ork.hypergraph.assets.materials.star_splat import StarSplat
from ork.hypergraph.ecs.scene._helpers import Transform
from ork.hypergraph.ecs.scene import _celestial
from ork.hypergraph.ecs.scene._sun import (_publish_celestial_config,
                                           _celestial_component)

# LOCAL shell radius; world radius comes from entity scale = radius_m / this.
LOCAL_RADIUS = 200.0

# Sky radius in WORLD meters. Far enough that walking the terrain gives no
# measurable star parallax, well inside the proven far-plane range (the cloud
# gauge's shells reach a 50 km rim).
SKY_RADIUS_M = 30000.0

# Shell tessellation, kept in the signature for source compatibility — the
# catalog field is per-star quads, so nothing consumes it.
SEGMENTS_U = 64
SEGMENTS_V = 48


class StarsMixin:
  """Scene night star dome — Bright Star Catalogue splats, sidereal rotation."""

  def stars(self, name="stars", *, sun="sun", radius_m=SKY_RADIUS_M,
            segments_u=SEGMENTS_U, segments_v=SEGMENTS_V, cap_deg=180.0,
            layer="std_transparent", material_params=None):
    """Declare the STAR DOME for a scene that already declared a celestial sun.

    sun             — name of the Scene.sun(celestial=...) whose site and clock
                      the dome shares. Absent from the published config table =
                      hard error (stars need a sky, and it must be THE sky).
    radius_m        — world radius of the celestial shell.
    segments_u/v    — INERT (the catalog field has no shell tessellation); kept
                      so callers of the procedural dome still load.
    cap_deg         — INERT for the same reason: the catalog covers the whole
                      sphere by construction, so no pole TILT can expose an
                      unstarred wedge at any latitude.
    material_params — extra kwargs for the StarSplat material (flux gain /
                      sigma floor / limiting magnitude / twilight-band
                      overrides; see that module)."""
    blob = _os.environ.get(_celestial.CONFIG_ENV_KEY, "")
    table = _json.loads(blob) if blob else {}
    sun_cfg = table.get(sun)
    if sun_cfg is None:
      raise KeyError(
        f"Scene.stars({name!r}): no celestial config published for sun {sun!r} "
        f"(declared: {sorted(table)}) — declare Scene.sun(celestial={{...}}) "
        f"first; the star dome shares its site and clock rather than owning a "
        f"second sky.")

    # Site + clock verbatim from the sun; body/base_intensity stay at their
    # defaults (the dome is not a light — _star_dome.py reads neither).
    cfg = {k: sun_cfg[k] for k in _celestial.MODEL_CONFIG_KEYS}
    cfg = _publish_celestial_config(name, cfg)

    # the material's quad half-angle and the bake's envelope BOTH default from
    # assets/mesh/_splat_sizing.py — one law, so they cannot drift apart.
    mtl = self.asset.Ptex3d(name + "_mtl", dsl_class=StarSplat,
                            **(material_params or {}))
    dome = self.asset.StarCatalogMesh(name + "_mesh",
                                      radius   = LOCAL_RADIUS,
                                      material = mtl)

    # manual declareNodeOnLayer (not SG.component(nodes=...)) for skip_auto_dpp:
    # a translucent sky shell must not join the depth prepass.
    sgc = self.declare_component("SceneGraphComponent")
    sgc.sub_calls.append(("declareNodeOnLayer", (), {
        "name":                name + "_node",
        "drawable":            dome.built,
        "layer":               layer,
        "drawable_asset_name": name + "_mesh",
        "skip_auto_dpp":       True}))

    # First-frame pose from the model, so frame 0 already has the real sky turn.
    q = _celestial.CelestialModel.from_config(cfg).at(0.0).star_dome_quat()

    return self.entity(name,
                       transform  = Transform(orientation=q,
                                              scale=float(radius_m) / LOCAL_RADIUS),
                       components = [
                         sgc,
                         _celestial_component(self, "_star_dome.py", cfg)])
