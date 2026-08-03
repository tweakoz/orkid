#!/usr/bin/env ork.python
###############################################################################
# scn_forest.py — scn_forest_sun's forest under the PROCEDURAL SKY +
# the jul25 CLOUD LAYERS instead of the desert4k envmap. A composition variant:
# subclass ForestScene (full scattered forest + xxx3 terrain + walker), flip
# the scenegraph's SkySource to "procedural" (Hillaire sky-view LUT + analytic
# sun disc + jul25 below-horizon sky floor; B3 procedural IBL lights the trees
# from the sky's own refiltered snapshot once warm — desert4k remains only the
# warm-up fallback), put the forest on the CELESTIAL CLOCK (sun + moon + star
# dome over one site, _celestial.py — see THE SOLVE below), and raise the sky
# library's cloud decks (_cloud_deck.py, promoted out of the retired gauge) above
# the terrain.
#
#   ork.scene.viewer.py scn_forest
#
# ALTITUDE NOTE — the xxx3 terrain is ALPINE (baked heights 1165..1816 m ASL,
# mean ~1451; see the forest content's spawn comment): the deck altitudes
# (cumulus 1.5 km / alto 4 km / cirrus 8 km) are ABOVE-GROUND-LEVEL figures for
# a sea-level plain, so each deck rides CLOUD_BASE_M higher here (cumulus base
# ~1.9 km above the valleys — plausible mountain cumulus). The dome sag targets
# the TERRAIN's mean level instead of sea level, so the far rim of each deck
# descends into the ridgeline and the mountains occlude the decks at the
# horizon (depth_test leq in CloudLayerMtl).
#
# ENV KNOBS (cloudgauge precedent — launch-time state, all optional):
#   ORK_FORESTSKY_TOD      time of day, hours  (default = the solved 9.56923,
#                          which is the pose the scene shipped with; the whole
#                          day runs in the library's 180 wall seconds, so any
#                          hour is one launch away and night is real night)
#   ORK_FORESTSKY_SUNEL    DECK-radiometry sun elevation deg (default 55)
#   ORK_FORESTSKY_SUNAZ    DECK-radiometry sun azimuth   deg (default 258)
#                          — these two no longer aim the light: they only build
#                          the STATIC sun_dir the cloud decks shade against
#                          (CloudLayerMtl takes a constant vector; a deck that
#                          tracks the celestial sun is a later slice). Values
#                          and vector are UNCHANGED from what shipped, so the
#                          decks are lit exactly as they were.
#   ORK_FORESTSKY_SUNINT   sun intensity       (default 1.5 — scn_forest_sun's
#                          3.0 was balanced against the desert4k sky; the
#                          procsky's fixed presentation exposure is much dimmer,
#                          so the ground comes DOWN to the sky's level and the
#                          HSVG post gamma below lifts the whole frame back)
#   ORK_FORESTSKY_DIFFINT  IBL DiffuseIntensity (default 1.5, forest had 3.0 —
#                          same rebalance as SUNINT)
#   ORK_FORESTSKY_POSTG    HSVG post-node gamma (default 1.8): the display-
#                          side lift that re-exposes the sky-matched frame.
#                          EXPOSURE NOTE — the DESIGNED per-scene knob is
#                          SkyAtmosphereData.sky_exposure via the
#                          "SkyAtmosphere" scene param, which DOES ride a .ecs
#                          now (the varmap object codec registered in
#                          lev2_init.cpp; it used to serialize null:, which is
#                          why this grade exists). This grade is still what
#                          ships here — sun+IBL down to the sky, post gamma up —
#                          and moving it onto the medium is a retune, not a fix.
#   ORK_FORESTSKY_CLOUDBASE  deck altitude offset m ASL (default 1850)
#   ORK_FORESTSKY_CLOUDGAIN  cloud radiance gain (lit/shadow/haze; default 1.0
#                            — cloudgauge colors were scaled to a dimmer
#                            32-deg-sun exposure; raise if decks read dark
#                            against the brighter afternoon sky)
#   ORK_FORESTSKY_VISTA    >0 = crane the walker CAMERA this many m above the
#                          feet (establishing shots; 0 = 1.7 m eye default)
#   FOREST_ADAPT_FLOOR     the tone stage's DEAD-OF-NIGHT adaptation anchor
#                          (default ADAPT_FLOOR below). See that constant.
#   FOREST_TIME_SCALE      clock rate (default 25.0, the shipped bench rate).
#                          0 FREEZES the ephemeris at TIME_OF_DAY — the only way
#                          to measure a still of this scene at a stated hour: the
#                          library day runs ~2 deg of sun per wall second, so an
#                          offscreen boot at midnight otherwise lands in twilight
#                          (scn_nightcal's header states the same at length).
#   plus the shared deck knobs via _cloudgauge_input.env_state():
#   ORK_CLOUDGAUGE_MODE=all|cirrus|alto|cumulus  ORK_CLOUDGAUGE_T=<thresh 0..1.05>
#   ORK_CLOUDGAUGE_TILE=<mult>  ORK_CLOUDGAUGE_RES=1024|2048
#   ORK_CLOUDGAUGE_WINDX=<mult>  ORK_CLOUDGAUGE_EVO=<mult>
#   ORK_CLOUDGAUGE_NODELAYER=<layer>   (deck render layer; library default)
#
# NO gamepad cloud-control layer here (the deck input script encodes the
# coverage threshold through spec-altitude transforms that don't know about
# CLOUD_BASE_M) — decks are static at launch state; wind still drifts them on
# the GPU clock. (The live deck-tuning gauge scene was retired jul28; its
# machinery is the sky library's _cloud_deck.py.)
###############################################################################

import math
import os
import sys

from orkengine.core import vec3

# The scenes dir isn't on sys.path when the resolver loads a scene by file path;
# add it so the deck library's shared launch-state module (_cloudgauge_input.py,
# which must stay a plain scenes-dir module — the player appends it as a system
# script) resolves through its primary import rather than its fallback.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# The forest itself is LIBRARY CONTENT now (promoted verbatim out of the retired
# ork.data/scenes/scn_forest.py) — this scene is the runnable member of the pair.
from ork.hypergraph.ecs.scene.content import ForestScene


def _envf(key, default):
  try:
    return float(os.environ.get(key, default))
  except ValueError:
    return float(default)


# DECK RADIOMETRY POSE — the numbers the scene's sun carried before it went on
# the celestial clock, kept because CloudLayerMtl shades against a CONSTANT
# sun_dir (see the ENV KNOBS note). They no longer aim any light.
SUN_ELEV_DEG  = _envf("ORK_FORESTSKY_SUNEL",  55.0)
SUN_AZIM_DEG  = _envf("ORK_FORESTSKY_SUNAZ",  258.0)
SUN_INTENSITY = _envf("ORK_FORESTSKY_SUNINT", 1.5)

DIFF_INTENSITY = _envf("ORK_FORESTSKY_DIFFINT", 1.5)
POST_GAMMA    = _envf("ORK_FORESTSKY_POSTG",  1.8)
_se, _sa = math.radians(SUN_ELEV_DEG), math.radians(SUN_AZIM_DEG)
SUN_DIR = (math.cos(_se) * math.sin(_sa),      # unit vector TOWARD the sun —
           math.sin(_se),                      # cloudgauge's convention, feeds
           math.cos(_se) * math.cos(_sa))      # CloudLayerMtl's radiometry

###############################################################################
# THE SOLVE — the site and clock that put the CELESTIAL sun exactly where the
# scene's hand-posed sun was, so the conversion is look-preserving by
# construction rather than by tuning.
#
# TARGET: the pose the scene shipped, in the DSL's own terms — elevation 55,
# azimuth 258 as Scene.sun(elevation=, azimuth=) consumed them. That azimuth is
# about world +Y with 0 = light travelling toward +Z, which is the NEGATIVE of
# an astronomical bearing (_celestial.py header), so the light the scene has
# always rendered comes from bearing -258 = 102 deg: EAST-SOUTH-EAST, i.e. a
# MORNING sun. The old "3:30 PM Jul 4" comment described the intent, not the
# frame — the solve preserves the frame.
#
# METHOD: hold the day of year at the scene's stated Jul 4 (doy 185) and solve
# the remaining two unknowns (latitude, time of day) against the two angles
# through _celestial.CelestialModel — grid over (lat, tod) then local refine.
# Latitude had to be free: at the library's default 45N the pair (el 55, bearing
# 102) is unreachable on any date at any hour (best residual 5.6 deg), while
# 36.35N reaches it exactly.
#
# RESIDUALS at the constants below (tolerance: 0.5 deg per axis):
#   elevation  +0.00004 deg      azimuth  +0.00004 deg
###############################################################################

SOLVED_LATITUDE_DEG = 36.3455    # observer latitude, + north
SOLVED_DAY_OF_YEAR  = 185.0      # Jul 4, the scene's stated date
SOLVED_TIME_OF_DAY  = 9.56923    # hours UT (longitude 0)

# NO time_scale here: the clock is the LIBRARY's (a full day in 180 wall
# seconds) and every family member inherits it — a scene that needs a different
# one is a finding, not a local constant.

# The knob the architecture puts in front: launch the same forest at any hour.
TIME_OF_DAY = _envf("ORK_FORESTSKY_TOD", SOLVED_TIME_OF_DAY)

# clock rate. The forest's 25x is an owner bench tweak (waived in the sky-library
# family invariant); 0 freezes the ephemeris, which is what a measured still of a
# stated hour needs.
TIME_SCALE = _envf("FOREST_TIME_SCALE", 25.0)

###############################################################################
# THE DEAD-OF-NIGHT ADAPTATION ANCHOR — the one number that decides whether this
# scene's night reaches the 8-bit store or quantizes to black. It acts only BELOW
# the tone stage's twilight luminance, so it cannot disturb the daylight or the
# early night the scene was graded at.
#
# WHY IT IS DECLARED AT ALL: the engine default (0.65) is the one that rendered
# this forest's midnight as black, and until this line existed the scene had no
# way to say otherwise — Scene.sky() takes tonemap= but the scene never passed
# it, so the shipped .ecs carried the default unconditionally.
#
# WHY NOT scn_nightcal's 512: that rig renders a BARE frame under a MOONLESS
# sky. This scene grades on top of the tone stage (POST_GAMMA 1.8 lifts the whole
# picture) and keeps its moon, its IBL diffuse and its cloud decks, so the light
# arriving at the stage is nearly two orders of magnitude above the rig's — the
# calibration band lands two decades LOWER. 512 here is a white-out (measured at
# the default eye height: 12.3% of the frame at full white).
#
# THE LADDER, measured at the dead of night (ORK_FORESTSKY_TOD=0) on a frozen
# clock, from the vista vantage, in 8-bit steps (sky mean / terrain mean /
# separation / fraction at 255):
#   0.65 (engine default)  0.30 / 0.24 /  0.07 / 0%   — black, no skyline
#   4                      1.93 / 0.70 /  1.23 / 0%   — stars only, ridge unreadable
#   12                     9.99 / 3.38 /  6.61 / 0%   — sky present, ridge faint
#   32                    38.59 / 15.99 / 22.60 / 0%  — DECLARED: stars, a read
#                                                        skyline, dark ground
###############################################################################
ADAPT_FLOOR = _envf("FOREST_ADAPT_FLOOR", 32.0)

# terrain datum (from the xxx3 .terrain.json manifest, quoted in the forest
# content module):
# the dome-sag target — far deck rims descend to ~this level so the ridgeline
# occludes them at the horizon.
TERRAIN_MEAN_M = 1451.0

# deck altitude offset: AGL spec altitudes -> ASL over the alpine terrain
# (cumulus 1500+1850=3350 ASL ~ 1.9 km over the valley floor).
CLOUD_BASE_M = _envf("ORK_FORESTSKY_CLOUDBASE", 1850.0)

# cloud radiance gain — CloudLayerMtl's constructor colors (mirrored here;
# keep in sync with _cloud_deck.CloudLayerMtl defaults) were tuned against
# the dim 32-degree gauge sun; one gain rides all four so the decks track the
# brighter (or, at sunset, dimmer) forest exposure without re-tuning each.
CLOUD_GAIN = _envf("ORK_FORESTSKY_CLOUDGAIN", 1.0)
_CLOUD_BASE_COLORS = {
    "lit_color":      (0.60, 0.575, 0.55),
    "shadow_color":   (0.155, 0.170, 0.20),
    "haze_color":     (0.30, 0.38, 0.47),
    "overcast_color": (0.295, 0.305, 0.325),
}


def _gain(rgb):
  return (rgb[0] * CLOUD_GAIN, rgb[1] * CLOUD_GAIN, rgb[2] * CLOUD_GAIN)


class ForestProcSkyScene(ForestScene):

  def __init__(self):
    super().__init__()

    ##########################
    # The whole sky in one call: the sky-source flip + exposure rebalance, and
    # the CELESTIAL ENSEMBLE (sun + moon + star dome over the solved site and
    # clock) that replaces the hand-posed sun. ForestScene already declared the
    # scenegraph, so the dome half AMENDS it (appends a second declareParams;
    # later keys win — see _sky_dome.py). skybox_path (desert4k) stays as
    # ForestScene declared it: the IBL warm-up fallback until the first
    # procedural IBL cycle publishes (B3). DiffuseIntensity comes down with the
    # sun (see the ENV KNOBS note: the procsky's presentation exposure is left at
    # the engine default and graded on the post gamma, not on the medium).
    # AmbientLight is restated because the amend writes the whole exposure
    # block, and ForestScene's 0.0 is NOT the library default (0.1) — the forest
    # is lit by sun + IBL alone. The sun's warm color and its rebalanced
    # intensity are restated too: the ensemble owns the aiming, the scene owns
    # the radiance it was graded against.
    #
    # This call also brings the ACES display stage with it (Scene.sky() turns it
    # on for a procedural sky with an ensemble) — the frame now runs through the
    # author's HSVG grade FIRST and the tonemap LAST.
    ##########################

    self.sky(latitude_deg  = SOLVED_LATITUDE_DEG,
             day_of_year   = SOLVED_DAY_OF_YEAR,
             time_of_day   = TIME_OF_DAY,
             moon          = True,
             stars         = True,
             sun_color     = vec3(1.0, 0.93, 0.80),
             sun_intensity = SUN_INTENSITY,
             diffuse_intensity = DIFF_INTENSITY,
             ambient_light     = vec3(0.0),
             time_scale    = TIME_SCALE,
             # the display-encode stage's dead-of-night anchor, DECLARED (see
             # ADAPT_FLOOR above) — it rides the .ecs into the player, which is
             # the only process that renders this scene.
             tonemap       = {"adapt_floor": ADAPT_FLOOR},
             # cloud shadows off the decks below (strength 0 = engine default =
             # whole cookie path disarmed, so it must be armed per-scene).
             # Extent sized to the alpine sightlines; softness = the mip bias
             # that keeps these fuzzier than the cascade edges.
             # FOREST_CLOUD_SHADOW=<0..1> (0 = off, the A/B leg).
             sun_params    = {"cloud_shadow_strength":
                                  float(os.environ.get("FOREST_CLOUD_SHADOW", "1.5")),
                              "cloud_shadow_extent": 9000.0,
                              "cloud_shadow_softness": 0.0})

    # display-side lift: the forest's own HSVG post node (an identity pass at
    # value=gamma=1) — raise gamma so the sky-matched dim frame re-exposes
    # without clipping highlights (out = rgb^(1/gamma), monotone, clip-free).
    for _call, _args, _kw in self.SG._decl.sub_calls:
      if _call == "addPostFxNode" and _args and _args[0] == "hsvg":
        _args[1].gamma = POST_GAMMA

    ##########################
    # The cloud decks — the sky library's, raised by CLOUD_BASE_M onto the alpine
    # terrain and sagged toward its mean level, left STATIC (no gamepad layer;
    # wind is a GPU-clock UV scroll inside the material, so the decks still
    # drift). Coverage/tile/resolution stay unset here so the shared
    # ORK_CLOUDGAUGE_* launch state still reaches them.
    ##########################

    self.cloud_decks(alt_offset_m = CLOUD_BASE_M,
                     sag_datum_m  = TERRAIN_MEAN_M,
                     sun_dir      = SUN_DIR,
                     colors       = {k: _gain(v)
                                     for k, v in _CLOUD_BASE_COLORS.items()})

    ##########################
    # VISTA crane (establishing shots): raise the walker's CAMERA eye height —
    # data-side tweak of the already-declared CharacterControllerComponent
    # (camera-only; the physics capsule and its collider are untouched).
    ##########################

    vista_m = _envf("ORK_FORESTSKY_VISTA", 0.0)
    if vista_m > 0.0:
      for comp in self._archetypes["Arch_walker"]._components:
        if comp.typename == "CharacterControllerComponent":
          comp.kwargs["eye_height"] = vista_m


__all__ = ["ForestProcSkyScene"]
