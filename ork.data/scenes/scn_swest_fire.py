###############################################################################
# scn_swest_fire.py — THE HERO BONFIRE. scn_swest's pueblo vale (2 km, ~300 m
# relief, banded strata, procedural sky, walkable) with ONE thing added and made
# the subject of the frame: a monumental ceremonial FIRE PIT (bld_firepit — a
# 12.4 m cyclopean drystone ring round a sunken hearth) with a full-scale
# BONFIRE burning in it (bonfire.py — a 10 m buoyant plume in seven layers, six
# of which publish their own flickering point light), on a graded ceremonial
# ground 53 m outside the nearest village.
#
# TIME OF DAY IS A PARAMETER (SWEST_TOD). The scene OPENS AT DUSK — 18.7 h, just
# past the parent's solved sunset — because that is the hour where both halves
# read at once: the fire is already the dominant warm source, and the vale, the
# pueblo terraces and the ridge behind are still lit well enough to place it in
# a world. NIGHT (21.5, and set SWEST_FIRE_FLOOR per the block below) is the
# hour the FIRE was authored against — it is the only one where the fire is the
# sole light source — but it renders the terrain and buildings nearly black.
# Pass SWEST_TOD=21.5 for it. Every hour renders correctly: the
# sky, the sun, the shadows, the cloud deck radiometry and the ACES adaptation
# all ride the one celestial clock, and the fire's own light rig is constant.
# The clock is FROZEN by default (SWEST_TIMESCALE=0) — a hero scene wants a
# named instant, and an offscreen still cannot survive a drifting one. The live
# scrub keys (] [ \ = - ' ; 0) still work; set SWEST_TIMESCALE=100 for the
# parent's compressed day.
#
# WHAT THIS FORKS, AND WHY IT HAD TO FORK
# ---------------------------------------
# The pit is placed by a HERO TERRAIN SINK, not by an entity transform:
# HypermeshComponent deliberately ignores the entity transform (the graph owns
# placement, HypermeshComponent.cpp:216-219), and a single instance_matrices
# entry does not reach the instanced path either. So the pit rides
# instance_source=(TERRA, "firepit", 0) out of a scatter_place sink — which
# buys a graded pad, a ring collider and an exact world position in one move,
# and costs a terrain fork (swestvale is scn_swest's and must not change).
# swestvale_fire.py carries the site; this scene imports its constants rather
# than restating them, so the pad, the hearth cut, the collider, the pit mesh
# and the fire all derive from ONE set of numbers.
#
#   ork.scene.viewer.py scn_swest_fire
#   ork.scene.tojson.py -i scn_swest_fire -o /tmp/fire.ecs && \
#       ork.ecs.player.exe /tmp/fire.ecs
#
# OFFSCREEN STILLS — both preconditions are non-negotiable and inherited:
#   SWEST_TIMESCALE=0   (default here) or the clock drifts hours during settle
#   SWEST_IBLFADE=0     or the snapshot lands mid IBL crossfade
#
# ...AND ONE THAT IS NOT INHERITED, because it cost this round a re-shoot:
# WHICH BINARY READS WHICH KNOB. SWEST_SPAWN / SWEST_FIRE_EYE / SWEST_FIRE_MOON
# / SWEST_TOD / SWEST_CLOUDS are read by THIS FILE, i.e. by
# ork.scene.tojson.py, at BUILD time. SWEST_FIRE_YAW and SWEST_FIRE_PITCH are
# read by _swest_fire_cam.py, which is a runtime PythonSystem — so they must be
# on the PLAYER, and putting them on the build step (as the old establishing
# recipe did) aims nothing: the camera keeps its link default of 180 deg and the
# frame comes back as a black ridge. Spawn/eye/moon on the build, yaw/pitch on
# the player.
#
#   # EYE LEVEL, 10 m south of the pit, on the graded pad.
#   # NOT 15 m: the pad is flat to 8 m and feathers out by 13, so a spawn at
#   # 15 m lands on NATURAL ground ~5.6 m above the pad and the "eye-level" shot
#   # is really a 7.3 m crane looking down into the pit. Measured: the same
#   # frame at SWEST_FIRE_EYE 1.7 vs 6.0 shifts 162 px, which pins fovy 65 and
#   # puts the old eye at y 167.8 against a pad at 160.5. 10 m keeps the walker
#   # on the pad, so the eye is where it says it is.
#   SWEST_TIMESCALE=0 SWEST_IBLFADE=0 SWEST_TOD=21.5 SWEST_SPAWN=-720,166,582 \
#     ork.scene.tojson.py -i scn_swest_fire -o /tmp/fire.ecs
#   SWEST_FIRE_YAW=180 ork.ecs.player.exe /tmp/fire.ecs --offscreen \
#     -S /tmp/eye.png -F 400
#
#   # ESTABLISHING, 22 m up on the SW approach — AND DECLARE THE MOON. See the
#   # per-shot block below: a night establishing shot needs a KEY, and without
#   # one this frame is a warm pool floating in an unreadable void.
#   SWEST_TIMESCALE=0 SWEST_IBLFADE=0 SWEST_TOD=21.5 SWEST_FIRE_MOON=1 \
#     SWEST_SPAWN=-760,170,552 SWEST_FIRE_EYE=22 \
#     ork.scene.tojson.py -i scn_swest_fire -o /tmp/estab.ecs
#   SWEST_FIRE_YAW=135 SWEST_FIRE_PITCH=-20 ork.ecs.player.exe /tmp/estab.ecs \
#     --offscreen -S /tmp/estab.png -F 400
#
# SETTLE FRAMES: the fire is a SIM, and -F is how long it has been burning.
# Measured on this rig: -F 60 is a bare hearth glow, 240 has a column but no
# trail, 400 is fully developed, 700 is the shipped gallery state (a ~20 m
# downwind smoke trail). Any still under -F 400 is a picture of a fire being
# lit, not of a fire.
#
# THE PER-SHOT RECIPE (round 2). One exposure genuinely cannot serve both night
# framings — but the fix is not two exposures, it is that the two shots want
# different KEYS:
#
#   EYE-LEVEL HERO   MOON off, SWEST_FIRE_FLOOR=4 (the shipped default).
#                    The fire is the only light and it is 15 m away, so the
#                    frame is carried entirely by its own falloff. 2.5 is the
#                    same frame a stop deeper — richer ground, dimmer stone;
#                    both are shippable and the choice is taste.
#   ESTABLISHING     MOON=1, FLOOR 4. At 40 m the fire lights a 25 m pool and
#                    NOTHING else: no ridge, no village, no horizon. Raising
#                    the floor alone (16, 48 — measured) buys stars and a wider
#                    white pool but never landform, because there is no light on
#                    it to develop. Declaring the moon puts the valley, the
#                    cloud deck and the graded pad back in frame while the fire
#                    stays the only WARM thing in it — which is the whole point
#                    of an establishing shot. The moon is off by default because
#                    it flattens the EYE-LEVEL frame; it is right for this one.
#
# ENV KNOBS (this scene's own are SWEST_FIRE_*; the SWEST_* ones are the
# parent's, unchanged, so the parent's recipes carry over verbatim):
#   SWEST_TOD=<h>          the hour the run opens at        (default 18.7, dusk;
#                          pass 21.5 for the night the fire was authored against)
#   SWEST_TIMESCALE=<x>    sim-sec per wall-sec             (default 0 = FROZEN)
#   SWEST_IBLFADE=<n>      procedural-IBL crossfade frames  (stills MUST pass 0)
#   SWEST_SPAWN=x,y,z      walker spawn                     (default: at the pit)
#   SWEST_FLY=1            walkable OFF (orbit cam — aims at the WORLD ORIGIN,
#                          which is nowhere near the pit; use the walker instead)
#   SWEST_FIRE_MOON=1      declare a full moon               (default 0 = OFF —
#                          a full moon at 36 N washes the fire out of its own
#                          scene; the fire is the light here)
#   SWEST_FIRE_FLOOR=<x>   ACES dead-of-night adaptation     (default 4 —
#                          NOT the sky-only night ladder; see the block below)
#   SWEST_FIRE_LIGHTINT=<x> / SWEST_FIRE_LIGHTRAD=<x>
#                          global multipliers on EVERY published point light
#                          (intensity / falloff radius)
#   SWEST_FIRE_NEARINT=<x> the NEAR-FIELD TRIM: an extra multiplier on the core
#                          and plume lights only, i.e. the ones parked at the
#                          fuel bed  (default 0.12; 1.0 = round 1's balance,
#                          which is what blew the hearth to white)
#   SWEST_FIRE_SMOKE_INT=<x> smoke sprite intensity   (default 0.15; the asset's
#                          own 0.28 is a dusk value and reads as a pale grey
#                          mass against a night sky)
#   SWEST_FIRE_HEAT=<x>    heat-shimmer refraction strength  (default 2.0;
#                          0 removes the aux channel + postfx node entirely)
#   SWEST_FIRE_CHROMA=<x>  heat-shimmer chromatic spread     (default 0.05)
#   SWEST_FIRE_PITMTL=<n>  pit materials 0..3                (default 0 — the
#                          engine's 255-shader-program ceiling; see the block
#                          at the pit. 1 = a dedicated sooted hearth, and it
#                          needs SWEST_CLOUDS=0 to fit)
#   SWEST_FIRE_LAYERS=<n>  how many fire layers to declare   (default: ALL of
#                          them. Lower is DIAGNOSTIC only, and it truncates the
#                          list in declaration order — cores, plumes, column
#                          fill, bounce ring, embers, smoke)
#   SWEST_FIRE_FILL=0      drop the RIM FILL LIGHT (see the block at
#                          _fire_layers) — the fire is then bonfire.py's 7
#   SWEST_FIRE_FILL_Y=<m>  fill-light height up the column   (default 9.0)
#   SWEST_FIRE_FILL_INT / SWEST_FIRE_FILL_RAD  its light knobs (78 / 45,
#                          before SWEST_FIRE_LIGHTINT's 0.45)
#   SWEST_FIRE_BNC=<n>     GROUND-BOUNCE RING lights          (default 4; 0 off)
#                          — the pit's outer wall has no other light source.
#                          See the block at _fire_layers.
#   SWEST_FIRE_BNC_R=<m>   ring radius from the pit axis      (default 8.0)
#   SWEST_FIRE_BNC_Y=<m>   height above the EXTERIOR grade    (default 0.55)
#   SWEST_FIRE_BNC_INT / SWEST_FIRE_BNC_RAD  its light knobs  (300 / 13, before
#                          SWEST_FIRE_LIGHTINT's 0.45). MEASURED bracket at
#                          n=4, Y 0.55: 100 = still a silhouette, 200 = dim
#                          stone, 300 = DECLARED (coursing legible, wall still
#                          darker than the ground), 600 = wall as bright as the
#                          ground and reading lamp-lit, 900 = anchors clipping.
#   SWEST_FIRE_BNC_PHASE=<deg> / SWEST_FIRE_BNC_SPR=<x>  ring phase / sprite
#                          brightness (the sprites are meant to be invisible)
#   SWEST_FIRE_BED=0       drop the EMBER-BED GLOW (see the block at
#                          _fire_layers) — the pyre's ash dome and burnt stubs
#                          then have no light source of their own
#   SWEST_FIRE_BED_R / _Y / _INT / _RAD / _SPR   its ring + light knobs
#                          (1.45 m, 0.45 m, 40, 5.0, 0.010)
#   SWEST_FIRE_EYE=<m>     walker eye height                 (default 1.7)
#   SWEST_FIRE_YAW=<deg>   camera heading, 0=-Z 90=+X 180=+Z (default 180)
#                          READ BY THE PLAYER, NOT BY tojson — see the recipe
#                          block above
#   SWEST_FIRE_PITCH=<deg> camera pitch, + = up              (default 0)
#                          READ BY THE PLAYER, NOT BY tojson
#   SWEST_CLOUDS / SWEST_CLOUD_* / SWEST_MATMODE / SWEST_TERRA_MODE / ...
#                          as scn_swest
###############################################################################

import inspect
import math
import os

from orkengine.core import vec3, lev2_pyexdir
from orkengine.lev2 import ParticleSystemGenData, PostFxNodeHeatDistort

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource
from ork.hypergraph.assets.hypermesh.bld_pueblo_row import PuebloRow
from ork.hypergraph.assets.hypermesh.bld_pueblo_stepped import PuebloStepped
from ork.hypergraph.assets.hypermesh.bld_kiva import Kiva
from ork.hypergraph.assets.hypermesh.bld_firepit import FirePit, GID_ANCHOR, GID_SOOT
from ork.hypergraph.assets.hypermesh.bld_pyre import Pyre, GID_CHAR
from ork.hypergraph.assets.materials.adobe import Adobe, AdobeStored
from ork.hypergraph.assets.materials.timber import Timber, TimberStored
from ork.hypergraph.assets.materials.hypermesh.section_array import SectionArrayPBR

# THE SITE, imported — never restated. swestvale_fire.py grades the pad to
# PIT_PAD_Y, cuts the hearth PIT_HEARTH_CUT_M below it, and places the pit at
# (PIT_X, PIT_PAD_Y - PIT_HEARTH_DEPTH, PIT_Z) with a ring collider derived from
# the same three pit numbers. This scene reads that one set and hands it to the
# mesh and to the fire, so nothing here can drift from the ground it stands on.
from ork.hypergraph.assets.terrain.swestvale_fire import (
    PIT_SINK, PIT_X, PIT_Z, PIT_PAD_Y,
    PIT_HEARTH_DEPTH, PIT_RIM_H, PIT_ANCHOR_RISE, PIT_MESH_ORIGIN,
    PYRE_SINK, PYRE_FOOT_R, PYRE_TOP_Y)

# THE FIRE, loaded for its LAYER CLASSES only. bonfire.py lives on the PARTICLE
# DSL search path (<ork.data>/particles), not on sys.path, so it is resolved the
# way every particle host resolves it — ONE exec of the module, four classes off
# it (load_dsl_class execs per call, which would give four unrelated copies).
#
# layers() is a host-side helper that returns already-built systems at the world
# ORIGIN; this scene needs the same seven at the pit, so it restates the
# per-layer arguments below (copied from layers() verbatim except offset / axis
# / wind, which ARE the placement) and drains each system itself.
from ork.hypergraph.dflow.particles.resolve import resolve_dsl_file, load_dsl_module
from ork.hypergraph.colors import hsv

_bonfire      = load_dsl_module(resolve_dsl_file("bonfire"))
BonfireCore   = _bonfire.BonfireCore
BonfirePlume  = _bonfire.BonfirePlume
BonfireEmbers = _bonfire.BonfireEmbers
BonfireSmoke  = _bonfire.BonfireSmoke
# the fire's y datum: every layer ctor adds this to the offset it is given, so a
# height expressed against the pit's EXTERIOR grade has to subtract it back off.
# Read from the asset, never restated.
HEARTH_Y      = _bonfire.HEARTH_Y
# THE CRIB-VENT SEATING, imported — the (base_r, base_y, live) triple per plume
# ring and the servo inversion that turns a release height into a lifespan and a
# rate. Restating either here is how the two files would drift.
PLUME_SEATING = _bonfire.PLUME_SEATING
plume_seating = _bonfire.plume_seating

lev2_pyexdir.addToSysPath()


def _envf(key, default):
  try:
    return float(os.environ.get(key, default))
  except ValueError:
    return float(default)


###############################################################################
# SITE + CLOCK. Same site and date as scn_swest (36 N pueblo country, Aug 11 —
# the date the parent's sunset/moon pose was solved on), so any hour reachable
# there is reachable here and the two scenes are comparable frame for frame.
# Only the DEFAULT HOUR moves: 21.5 h is full night at this latitude and date
# (the sun is ~20 deg below the horizon), which is what the bonfire was
# authored against.
###############################################################################

SITE_LATITUDE_DEG = 36.0
SITE_DAY_OF_YEAR  = 223.0
SITE_TIME_OF_DAY  = 18.7        # DUSK, just past the parent's solved sunset
                                # (18.67833). The terrain, the pueblo terraces
                                # and the ridge still read here while the fire
                                # already dominates. 21.5 is the authored night,
                                # where the fire is the only light and the
                                # landform goes black.

TIME_OF_DAY = _envf("SWEST_TOD", SITE_TIME_OF_DAY)
# FROZEN by default (the parent runs 100): a hero scene is a named instant, and
# every still in this scene's gallery would otherwise be a different hour.
TIME_SCALE  = _envf("SWEST_TIMESCALE", 0.0)

# THE MOON IS OFF BY DEFAULT, and that is a LOOK decision, not an oversight.
# The parent declares a FULL moon so its village reads at night; a full moon at
# 36 N puts ~45 deg of hard blue-white key on the same stone the fire is trying
# to light, and the fire stops being the light source in its own scene. With it
# off, the only things lighting the pit are six flickering point lights and the
# sky's own scattered residual — which is the whole picture.
MOON_ON    = os.environ.get("SWEST_FIRE_MOON", "0") == "1"
MOON_PHASE = "full" if MOON_ON else None

# ACES DEAD-OF-NIGHT ADAPTATION. The published ladder (scn_nightcal.py) is for
# a scene lit only by the SKY: 0.65 (engine default) = black, 256 -> sky mean
# 1.90, 512 -> 4.51, 1024 -> 11.37. A BONFIRE SCENE IS NOT ON THAT LADDER, and
# this is the round-1 measurement that matters: the adaptation term multiplies
# the whole frame, so a floor sized to develop an unlit night ALSO multiplies
# six point lights that are already the brightest thing in it. MEASURED here, at
# the default eye vantage:
#     256  everything clips — fire, smoke and ground are one white mass
#      48  ground blown to white in a 20 m pool, flame mostly white
#      16  ground still white-hot near the pit, smoke pure white
#       4  DECLARED — flame keeps its orange-to-red ramp, the ground reads as a
#          warm firelit pool, the smoke has a value range, stars survive
#    0.65  (engine default) the fire alone in a black world; no ground read
# It acts only BELOW the twilight anchor, so it cannot disturb any daylight hour
# this scene is asked for — the noon and dusk frames are identical at any of
# these values.
#
# ROUND 2 RE-MEASURED IT, and the ladder above still holds — but it is no longer
# the lever it looked like. The hearth interior was ~4 stops over at 4 while the
# ground 10 m out was correctly exposed in the SAME frame, and no single number
# closes a 4-stop spread: at 0.25 the hearth develops and everything else is
# black. That spread was a FALLOFF problem, and it is fixed at the light rig
# (see NEAR_INT_MUL), not here. With the rig fixed the useful range at eye level
# is 2.5-6 and the frame is shippable across all of it — 2.5 is a stop deeper,
# 6 pushes the ground golden. 4 stays DECLARED.
ADAPT_FLOOR = _envf("SWEST_FIRE_FLOOR", 4.0)

TERRA = "swest_fire_terra"

VARIANTS = [("row", PuebloRow), ("stepped", PuebloStepped)]

# THE FIRE'S FRAME. bonfire.py's layers add HEARTH_Y (0.15 — the hearth floor
# SLAB surface) to whatever offset they are given, and every offset is in the
# pit's own frame where y=0 is the hearth-floor PLANE. PIT_MESH_ORIGIN is that
# plane in world, so this vec3 is the whole placement.
FIRE_ORIGIN = vec3(*PIT_MESH_ORIGIN)
# The confinement servo pulls each particle toward (axis_x, axis_z); left at its
# (0,0) default the whole column would be dragged toward the WORLD origin 925 m
# away. It travels with the offset — always.
FIRE_AXIS = (PIT_X, PIT_Z)
# target horizontal wind VELOCITY (m/s) — bonfire.layers()' default.
FIRE_WIND = vec3(0.90, 0.0, 0.32)

# WHERE THE FIRE'S LIGHT SITS, VERTICALLY — the one number that decides whether
# the pit stone catches its own fire. The engine publishes ONE point light per
# particle drawable, at the sprite renderer's luminance-weighted centroid, and
# `light_body_bias` (emission_lum_power) is the exponent on that weight: high
# values concentrate it on the BRIGHTEST = YOUNGEST = LOWEST particles, i.e.
# down at the fuel bed. bonfire.py's own values (core 1.60, plume 2.20) park
# every light at the bed, which is BELOW the pit's rim — so every up-facing
# coping stone has N.L < 0 and the ring renders as a black silhouette inside a
# brightly lit ring of ground. Lower values raise the centroid into the column,
# where a 10 m flame actually radiates from, and the rim lights.
#
# A8: live reflected per-material knobs, exposed here rather than folded in.
# They are the SCENE's restatement of the layer arguments, not an edit to the
# fire asset — the asset's own values are the defaults.
CORE_LIGHT_BIAS  = _envf("SWEST_FIRE_CORE_BIAS",  1.60)
PLUME_LIGHT_BIAS = _envf("SWEST_FIRE_PLUME_BIAS", 2.20)

# GLOBAL multipliers on every published light's intensity and falloff radius.
# Diagnostic first (a huge radius is how you tell "this surface is out of the
# light's range" from "this surface has no point-light path at all"), knob
# second — the fire's own numbers are 1.0/1.0.
# 0.45 is DECLARED, not neutral: bonfire.py's published intensities were tuned
# without a pit under them, and at 1.0 they wash the graded ceremonial ground to
# white in a 20 m pool at this scene's night exposure. The asset is untouched —
# this is the scene's balance, on a live knob.
LIGHT_INT_MUL = _envf("SWEST_FIRE_LIGHTINT", 0.45)
LIGHT_RAD_MUL = _envf("SWEST_FIRE_LIGHTRAD", 1.0)

# THE NEAR-FIELD TRIM — round 2's headline, and the fix for the blown hearth.
#
# THE MEASUREMENT. At night eye level the pit's INTERIOR (hearth slab + fire
# curb tops) rendered as one featureless white mass with a hard edge. Legs, one
# per run, at the default eye vantage:
#   * kill every published light (SWEST_FIRE_LIGHTINT=1e-4) -> the interior goes
#     BLACK and the fire's own sprites read as a clean orange column with only a
#     small white heart. The white mass is therefore LIT STONE, not particles.
#   * drop the ACES floor 4 -> 0.25 (4 stops) -> the interior finally develops
#     tone. So it was ~4 stops over, while the ground 10 m out was correctly
#     exposed at the SAME frame. That spread is not an exposure problem, it is a
#     FALLOFF problem.
#   * SWEST_FIRE_PITMTL=1 (a dedicated near-black soot material on the hearth)
#     changed almost nothing: 3x less albedo against a 16x overexposure.
#
# THE CAUSE, and it is geometry: core and plume park their published lights at
# the FUEL BED, ~0.3-1.0 m over the slab, and the slab is the first thing they
# hit. Inverse square across 1 m vs 10 m is a 100:1 spread, so no single
# exposure can hold both — the hearth clips or the ground goes black.
#
# THE RECIPE IS THE STANDARD ONE: demote the practical, promote the rig light.
# A near-field source with a 1 m throw cannot light a 20 m pool evenly; a source
# 9 m up the column (SWEST_FIRE_FILL_Y) reaches the hearth at 9 m and the ground
# at 13 m — a 2:1 spread instead of 100:1. So the fire's OWN six lights are cut
# to a flicker term (0.12) and the column fill carries the key. Measured: at
# 0.12 the hearth keeps a bright foot under the flame and falls off to legible
# stone across the rest of the floor, and the ground pool is unchanged.
#
# 1.0 restores round 1's balance exactly (the blown hearth with it).
NEAR_INT_MUL  = _envf("SWEST_FIRE_NEARINT", 0.12)


def _firepit_default(name):
  """Read a bld_firepit ctor default LIVE from the asset's own signature.

  The pit's quarry palette (four stone variants + the anchor megaliths + the
  soot) is authored ON the asset, and `self.asset.Hypermesh(...)` discards the
  DSL instance after generatedflow() — so materials() and _palette are both
  unreachable from a scene. Reading the signature is the one way to bind the
  scene's materials to the asset's OWN declared colours instead of to a copy
  that goes stale the next time the pit is tuned."""
  return inspect.signature(FirePit).parameters[name].default


def _pyre_proxy_drift():
  """Warn if bld_pyre's collider grammar has moved out from under the terrain.

  swestvale_fire RESTATES the pyre's capsule proxy (PYRE_FOOT_R / PYRE_TOP_Y)
  because a scatter sink is where a collider is declared and the DSL instance
  that publishes `pyre_proxy` is discarded — the same standing coupling the
  pit's ring carries. Every term of the footprint radius IS a plain ctor
  default, so THAT half can be checked here for free; the crown involves the
  seeded course stack, so only its non-stack terms are checked and the crib top
  is reported as the residual. Loud on stdout, never a raise: a moved default
  is a re-measure, not a broken scene."""
  def d(n):
    return float(inspect.signature(Pyre).parameters[n].default)
  foot = max(d("lean_r"), d("spar_r")) + d("lean_r_jit") + d("log_d") * 0.5
  if abs(foot - PYRE_FOOT_R) > 1e-3:
    print("scn_swest_fire: bld_pyre's mantle footprint is now %.3f m but the "
          "terrain declares a %.3f m collider capsule — update PYRE_FOOT_R in "
          "swestvale_fire." % (foot, PYRE_FOOT_R), flush=True)
  crib_top = PYRE_TOP_Y - (d("spar_over") + d("lean_head_jit") + d("log_d") * 0.5)
  nominal  = d("hearth_top") + d("log_d") * (1.0 + (d("crib_courses") - 1.0) * d("crib_settle"))
  if abs(crib_top - nominal) > 0.5:
    print("scn_swest_fire: the pyre collider's implied crib top (%.3f m) is "
          "%.2f m off the course-stack nominal (%.3f m) — re-read bld_pyre's "
          "pyre_proxy and update PYRE_TOP_Y in swestvale_fire."
          % (crib_top, crib_top - nominal, nominal), flush=True)


###############################################################################
# THE FIRE SYSTEMS — bonfire.layers()' seven, restated, plus this scene's own
# six composition layers (column fill, ember bed, four-light bounce ring).
#
# The asset layer instantiates exactly ONE dsl_class per ParticleSystem
# (assets.py:1299-1308), and bonfire declares four, so layers()' seven-drawable
# composition has to be spelled out here. Each entry is (name, class, ctor
# kwargs, emitter_intensity, emitter_radius); the kwargs are layers()' own,
# except offset/axis/wind which carry the placement.
#
# WHY THESE GO IN THROUGH gendata= AND NOT AS ASSET KWARGS. `axis` is a 2-tuple
# and the ParticleSystemGenData param varmap cannot encode one: it tries
# fvec4/fvec3/fvec2 and then float, and a python tuple casts to none of them, so
# the call RAISES (measured). vec2/vec3 encode but are not subscriptable, which
# is what the DSL does to `axis`. The system is therefore built here — exactly
# as layers() builds it — and handed over as an embedded graph, which is the
# same model-B artifact the kwarg path would have produced (assets.py:1336-1341
# uses gendata.graph directly and never re-runs the DSL). The cost is that the
# per-layer numbers live in THIS file rather than in the .ecs param varmap;
# they are all here, in one table, for exactly that reason.
###############################################################################

def _fire_layers():
  """[(name, cls, kwargs, emitter_intensity, emitter_radius), ...]."""
  out = []

  # --- CORE x2 — the white-hot heart, low in the curb. The two offsets are what
  # separate the two light centroids AND set the width of the clipped white ball.
  core_variants = [
      (vec3( 0.24, 0.0,  0.15), hsv(37, 0.32, 1.0), 0.035, 1.00, 1.00),
      (vec3(-0.27, 0.0, -0.17), hsv(31, 0.50, 1.0), 0.065, 0.86, 1.12),
  ]
  for i, (off, tint, smth, isc, rsc) in enumerate(core_variants):
    out.append(("fire_core%d" % i, BonfireCore, dict(
        offset          = FIRE_ORIGIN + off,
        axis            = FIRE_AXIS,
        wind            = FIRE_WIND * 0.6,
        base_r          = 0.46,
        size            = 0.40,
        rise            = 2.20,
        intensity       = 1.30 * isc,
        light_tint      = tint,             # colors.hsv(h,s,v) is a REAL vec3
                                            # (the 4-arg form is the vec4 one) —
                                            # no lazy-colour unwrap needed here
        light_smoothing = smth,
        light_body_bias = CORE_LIGHT_BIAS,
        rate            = 520.0 / len(core_variants),
        spin            = 3.10 + 0.7 * i,
        pool_size       = 320,
        seed            = 41.0 + 13.0 * i),
        8.5 * isc * NEAR_INT_MUL, 9.0 * rsc))

  # --- PLUME x3 on CONCENTRIC RINGS — since round 5 the CRIB-VENT SEATING
  # (bonfire.PLUME_SEATING): 0.40 m released 2.70 m up the pyre's flue, 1.05 m
  # released in the hollow crib at 1.70 m, and 2.60 m released at 0.70 m OUTSIDE
  # the timber, just clear of the fire curb's 0.67 m top.
  #
  # WHY IT MOVED (measured by the pyre round, fixed here): all three rings used
  # to release at 0.05 m, i.e. INSIDE the stack, and opaque timber depth-tests
  # sprites away — the visible column lost about a third of its height. The
  # column ALSO read narrower than the 6 m curb it stands in; the outer ring
  # going 2.35 -> 2.60 m (plus its 0.76 m sprite) is that fix, made at the
  # EMISSION rather than by re-tuning the confinement servo.
  #
  # The lifespan and rate are DERIVED from the release height, not authored: the
  # servo identity h(L) = v*(L - tau) inverts so all three rings still meet at a
  # 10.5 m tip, and the per-ring LIVE POPULATION (742/663/546) is round 4's
  # exactly — the same photons, re-seated. This is the layer that writes the
  # "heat" aux channel.
  plume_variants = [
      # size  spin  offset                      tint             smth  int   rad
      (1.10, 2.30, vec3( 0.18, 0.0, -0.14), hsv(28, 0.62, 1.0), 0.12, 1.05, 0.85),
      (0.90, 2.90, vec3(-0.30, 0.0,  0.22), hsv(23, 0.74, 1.0), 0.18, 1.00, 1.00),
      (0.76, 1.90, vec3( 0.24, 0.0,  0.30), hsv(18, 0.84, 1.0), 0.26, 0.82, 1.15),
  ]
  for i, (sz, spin, off, tint, smth, isc, rsc) in enumerate(plume_variants):
    _rr, _by, _live = PLUME_SEATING[i]
    _life, _rate = plume_seating(_by, _live)
    out.append(("fire_plume%d" % i, BonfirePlume, dict(
        offset          = FIRE_ORIGIN + off,
        axis            = FIRE_AXIS,
        wind            = FIRE_WIND,
        base_r          = _rr,
        base_y          = _by,
        size            = sz,
        spin            = spin,
        rise            = 3.40,
        lifespan        = _life,
        intensity       = 0.24 * isc,
        light_tint      = tint,
        light_smoothing = smth,
        light_body_bias = PLUME_LIGHT_BIAS,
        rate            = _rate,
        pool_size       = 900,
        seed            = 57.0 + 17.0 * i,
        curl_speed      = 0.16 + 0.05 * i),
        13.5 * isc * NEAR_INT_MUL, 16.0 * rsc))

  # --- THE RIM FILL LIGHT — a SCENE-COMPOSITION layer, not part of the fire.
  #
  # THE PROBLEM IT SOLVES (round 1, measured): the engine publishes ONE point
  # light per particle drawable, at the sprite renderer's luminance-weighted
  # centroid, and for every layer of this fire that centroid sits down at the
  # fuel bed — which is BELOW the pit's 1.25 m rim. Every up-facing coping stone
  # and every outward-facing wall stone therefore has N.L <= 0 and the ring
  # renders as a black silhouette inside a brightly lit ring of ground. Verified
  # not to be a range problem (a 200x radius lights the village 53 m away and
  # still leaves the ring black) and not fixable from light_body_bias (0.0, i.e.
  # a uniform centroid, moved nothing visible).
  #
  # THE RECIPE IS THE STANDARD ONE: a real 10 m flame radiates from the whole
  # COLUMN, so light the surround from the column's middle. This is a games
  # fill-light rig — one more drawable whose SPRITES are near-invisible
  # (intensity 0.05 inside a plume running 0.24) and whose only product is a
  # published light at `SWEST_FIRE_FILL_Y` metres up the axis.
  #
  # It deliberately reuses core0's EXACT fragment parameters (seed, noise, soft
  # fade), because materialize_fragment keys the generated shader on those — an
  # identical set shares the compiled material and costs ZERO extra shader
  # programs, which this scene has none of (see the pit-material block).
  #
  # SWEST_FIRE_FILL=0 removes it; the fire is then bonfire.py's seven exactly.
  if _envf("SWEST_FIRE_FILL", 1.0) > 0.0:
    out.append(("fire_fill", BonfireCore, dict(
        offset          = FIRE_ORIGIN,
        axis            = FIRE_AXIS,
        wind            = FIRE_WIND * 0.6,
        base_y          = _envf("SWEST_FIRE_FILL_Y", 9.00),
        base_r          = 0.80,
        size            = 0.55,
        rate            = 90.0,
        lifespan        = 0.95,
        intensity       = 0.05,          # sprites: a whisper inside the plume
        light_tint      = hsv(30, 0.55, 1.0),
        light_smoothing = 0.10,
        light_body_bias = 1.00,
        confine         = 0.85,
        spin            = 2.70,
        pool_size       = 128,
        seed            = 41.0),         # == core0: SHARES the compiled fragment
        # MEASURED (round 1): the coping stones first read as lit stone at
        # y 9.0 with an effective intensity ~27 and radius 45. y 4.2 / 11 was
        # invisible — a fire's rim light comes from high in the COLUMN, not from
        # just above the fuel. 60 here x LIGHT_INT_MUL 0.45 = the 27 that
        # measured right, so the two knobs compose the way the rest do.
        #
        # ROUND 2 raises it 60 -> 78 (effective 27 -> 35). This is now the KEY,
        # not a fill: with the near-field practicals trimmed to 0.12 (see
        # NEAR_INT_MUL) the column light is what develops the ground pool, and
        # it does it with a 2:1 near/far spread instead of the fuel bed's 100:1.
        _envf("SWEST_FIRE_FILL_INT", 78.0),
        _envf("SWEST_FIRE_FILL_RAD", 45.0)))

  # --- THE EMBER-BED GLOW — the ash dome's only light source.
  #
  # THE PROBLEM (measured this round): bld_pyre stands in an ASH DOME of radius
  # 1.85 m with six burnt-through billets half-sunk in it, and all of it is gid
  # CHAR — a near-black material. Nothing lights it. The fire's own practicals
  # are trimmed to a flicker term (NEAR_INT_MUL 0.12) and the column fill is 9 m
  # up, so the bed renders as a dark hole under a bright fire: the one place in
  # frame that should read as INCANDESCENT reads as shadow.
  #
  # THE RECIPE IS THE PRACTICAL A FIRE ACTUALLY HAS. In life the brightest thing
  # in a bonfire at close range is not the flame, it is the bed of coals under
  # it — a low, WIDE, deep-orange area source lying in the ash, an order of
  # magnitude closer to the stubs than the flame is. A rasterizer with no GI
  # gets it the same way the bounce ring does: one more drawable whose sprites
  # are a whisper and whose product is a published point light, parked at the
  # bed, tinted to glowing charcoal rather than to flame.
  #
  # Same fragment parameters as core0 (seed 41, the default noise/erode/soft
  # fade), so it SHARES the compiled material and costs ZERO shader programs —
  # this scene has none to spend (see the pit-material block). Its light is
  # NOT put through NEAR_INT_MUL: that trim exists to demote practicals that
  # were lighting the whole hearth from 1 m away, and this one is DECLARED as a
  # near-field source with a short radius, which is the point of it.
  #
  #   SWEST_FIRE_BED=0        remove the layer
  #   SWEST_FIRE_BED_R=<m>    ember-ring radius        (default 1.45, inside the
  #                           pyre's 1.85 m ash dome)
  #   SWEST_FIRE_BED_Y=<m>    height above the slab    (default 0.45 — just over
  #                           the 0.16 m dome and the stubs lying in it)
  #   SWEST_FIRE_BED_INT / _RAD  its light knobs, before LIGHT_INT_MUL's 0.45.
  #                           MEASURED bracket at Y 0.45, RAD 5: 12 = the dome
  #                           barely separates from the slab, 40 = DECLARED
  #                           (ash dome and stubs read as lit, the hearth floor
  #                           beyond the bed stays dark), 100 = the bed reads
  #                           as a lamp under the stack and the stubs blow out.
  #   SWEST_FIRE_BED_SPR=<x>  sprite brightness (meant to be nearly invisible)
  if _envf("SWEST_FIRE_BED", 1.0) > 0.0:
    out.append(("fire_embed", BonfireCore, dict(
        offset          = FIRE_ORIGIN,
        axis            = FIRE_AXIS,
        wind            = vec3(0.0, 0.0, 0.0),
        base_y          = _envf("SWEST_FIRE_BED_Y", 0.45),
        base_r          = _envf("SWEST_FIRE_BED_R", 1.45),
        size            = 0.34,
        rate            = 80.0,
        lifespan        = 0.90,
        rise            = 0.0,           # a PARKED light: coals do not rise
        confine         = 1.20,
        confine_band    = (0.0, 0.40),
        turbulence      = 0.0,
        curl_strength   = 0.0,
        intensity       = _envf("SWEST_FIRE_BED_SPR", 0.010),
        # GLOWING CHARCOAL, not flame: a coal bed is deeper and redder than the
        # gas above it, and that colour difference is most of what says the two
        # are different materials.
        light_tint      = hsv(16, 0.88, 1.0),
        light_smoothing = 0.28,          # coals are SLOW — they must not strobe
        light_body_bias = 1.00,
        spin            = 2.90,
        pool_size       = 128,
        seed            = 41.0),         # == core0: SHARES the compiled fragment
        _envf("SWEST_FIRE_BED_INT", 40.0),
        _envf("SWEST_FIRE_BED_RAD", 5.0)))

  # --- THE GROUND-BOUNCE RING — the OUTER WALL's only light source.
  #
  # THE PROBLEM (round 1, left open): the pit's outward-facing wall stones point
  # radially AWAY from every light this fire has, all of which are on the column
  # axis. N.L <= 0 on every one of them, so the ring reads as a black silhouette
  # standing in a brightly lit pool of ground — and no amount of intensity,
  # radius or centroid bias can fix a sign.
  #
  # THE RECIPE IS THE ONE FILM AND GAMES BOTH USE FOR THIS EXACT SHOT: the term
  # that lights the outside of a fire ring in life is not the flame, it is the
  # GROUND BOUNCE. The ground around the pit is the brightest surface in frame;
  # its diffuse reflection is a large, low, warm area source lying in the dirt,
  # and that is what puts the roll onto the drystone. A rasterizer with no GI
  # gets it the way it always has: a RING OF LOW POINT LIGHTS just outside the
  # object, at bounce height, tinted to the ground's albedo. One light would
  # only light the wall it faces — the ring is what makes it read from every
  # heading the walker can take.
  #
  # Same fragment parameters as core0 (seed 41, default noise, soft fade on), so
  # the whole ring shares ONE compiled material and costs ZERO shader programs —
  # which is the entire budget this scene has (see the pit-material block).
  # The sprites are a whisper (0.05, 0.30 m) and hover: rise 0, no wind, no
  # turbulence, confined hard to their own spot, so each light stays PARKED.
  #
  #   SWEST_FIRE_BNC=<n>      lights on the ring   (default 4; 0 = off)
  #   SWEST_FIRE_BNC_R=<m>    ring radius from the pit axis     (default 8.0 —
  #                           the pit's outer wall is at ~6.2 m, so this sits
  #                           1.8 m clear of the stone, in the lit ground)
  #   SWEST_FIRE_BNC_Y=<m>    height above the EXTERIOR grade   (default 0.55)
  #   SWEST_FIRE_BNC_INT / _RAD  its light knobs, before LIGHT_INT_MUL
  #   SWEST_FIRE_BNC_PHASE=<deg>  ring phase — which way the first one faces
  _bnc_n = int(_envf("SWEST_FIRE_BNC", 4))
  if _bnc_n > 0:
    _bnc_r  = _envf("SWEST_FIRE_BNC_R", 8.00)
    # base_y is measured from the HEARTH FLOOR plane and the ctor adds HEARTH_Y,
    # so an exterior-grade height has to climb back out of the pit first. Both
    # terms are imported, so a re-graded pit carries the bounce ring with it.
    _bnc_y  = PIT_HEARTH_DEPTH - HEARTH_Y + _envf("SWEST_FIRE_BNC_Y", 0.55)
    _bnc_ph = _envf("SWEST_FIRE_BNC_PHASE", 0.0)
    for i in range(_bnc_n):
      _a  = math.radians(_bnc_ph + 360.0 * i / _bnc_n)
      _off = vec3(math.sin(_a) * _bnc_r, 0.0, math.cos(_a) * _bnc_r)
      out.append(("fire_bounce%d" % i, BonfireCore, dict(
          offset          = FIRE_ORIGIN + _off,
          # the confinement servo pulls toward ITS OWN spot, not the column's,
          # or the whole puff would be dragged onto the fire axis.
          axis            = (FIRE_AXIS[0] + _off.x, FIRE_AXIS[1] + _off.z),
          wind            = vec3(0.0, 0.0, 0.0),
          base_y          = _bnc_y,
          base_r          = 0.30,
          size            = 0.22,
          rate            = 70.0,
          lifespan        = 0.80,
          rise            = 0.0,           # a PARKED light: no rise, no drift
          confine         = 1.60,
          confine_band    = (0.0, 0.30),
          turbulence      = 0.0,
          curl_strength   = 0.0,
          # SPRITES: INVISIBLE BY CONSTRUCTION. Unlike the column fill, these
          # puffs sit on OPEN LIT GROUND in the middle of the frame, and at the
          # fill's 0.05 they read as two white smudges lying in the dirt
          # (measured). The published light colour is (gradient x colorIntensity
          # x tint) — i.e. LINEAR in this number — so dividing the sprite
          # brightness by 8 and multiplying BNC_INT by 8 is an exact trade: the
          # light is identical and the sprite drops below the ground it sits on.
          intensity       = _envf("SWEST_FIRE_BNC_SPR", 0.006),
          # the ground's albedo, warmed by the fire it is reflecting — a bounce
          # takes the colour of the surface it comes off, not of the source.
          light_tint      = hsv(27, 0.68, 1.0),
          light_smoothing = 0.35,          # bounce is SLOW: it averages the whole
                                           # lit pool, so it must not strobe
          light_body_bias = 1.00,
          spin            = 2.30,
          pool_size       = 96,
          seed            = 41.0),         # == core0: SHARES the compiled fragment
          _envf("SWEST_FIRE_BNC_INT", 300.0),
          _envf("SWEST_FIRE_BNC_RAD", 13.0)))

  # --- EMBERS: StreakRenderer publishes NO light, so the knobs are zero BY
  # CONTRACT, not by taste.
  out.append(("fire_embers", BonfireEmbers, dict(
      offset = FIRE_ORIGIN,
      axis   = FIRE_AXIS,
      wind   = FIRE_WIND * 1.8,
      base_r = 1.35),
      0.0, 1.0))

  # --- SMOKE: released AT the column top (8.8 m), absorptive, leaning downwind.
  #
  # SWEST_FIRE_SMOKE_INT is a SCENE knob on the asset's own `intensity` kwarg,
  # not an edit to bonfire.py. The asset's 0.28 was authored against a DUSK sky,
  # where the smoke competes with real sky luminance and reads as a dark billow.
  # At this scene's night exposure there is nothing behind it, the 300-sprite
  # stack whitens the way any additive stack does, and the same layer reads as a
  # pale grey mass hanging over the frame. 0.15 puts it back to a dim,
  # fire-lit-at-the-head plume that occludes stars — same layer, same shape,
  # a night value instead of a dusk one. Set 0.28 to restore the asset default.
  out.append(("fire_smoke", BonfireSmoke, dict(
      offset    = FIRE_ORIGIN,
      axis      = FIRE_AXIS,
      wind      = FIRE_WIND * 2.6,
      base_r    = 1.80,
      base_y    = 8.80,
      size      = 2.90,
      rate      = 34.0,
      intensity = _envf("SWEST_FIRE_SMOKE_INT", 0.15)),
      0.7, 9.0))
  return out


class SwestFireScene(Scene):

  def __init__(self):
    super().__init__()

    matmode = os.environ.get("SWEST_MATMODE", "proc")
    if matmode not in ("proc", "stored"):
      raise ValueError("SWEST_MATMODE must be 'proc' or 'stored' (got %r)" % matmode)
    stored   = (matmode == "stored")
    bake_res = int(os.environ.get("SWEST_BAKE_RES", "4096"))
    AdobeCls  = AdobeStored  if stored else Adobe
    TimberCls = TimberStored if stored else Timber

    ##########################
    # HEAT SHIMMER. bonfire's PLUME layers (and only those) generate a heat
    # technique pair, writing shimmer into the "heat" aux RT; this node refracts
    # the composited frame by that field's gradient. Both numbers are settled:
    # 0.20 strength is invisible and 8.0 shreds the column; 0.20-0.25 chroma
    # gives coloured outlines rather than refraction.
    #
    # It must run BEFORE the tone stage, and that is automatic — sky_dome always
    # APPENDS ACES to whatever chain the scene declares (_sky_dome.py:252-263).
    ##########################
    # SWEST_FIRE_HEAT=0 removes the aux channel AND the postfx node outright
    # (not just a zeroed strength): the A/B leg, and one lever on the shader
    # PROGRAM BUDGET this scene lives close to — see the pit-material note.
    heat_strength = _envf("SWEST_FIRE_HEAT", 2.0)
    heat_on = heat_strength > 0.0
    heat_kw = {}
    if heat_on:
      heat_fx = PostFxNodeHeatDistort()
      heat_fx.strength = heat_strength
      heat_fx.chroma   = _envf("SWEST_FIRE_CHROMA", 0.05)
      heat_kw = dict(aux_channels = ["heat"],
                     postfx       = [("heatdistort", heat_fx)])

    ##########################
    # THE SKY — scn_swest's call, with three declarations changed and one added:
    # the opening hour (night), the clock (frozen), the moon (off), and the ACES
    # dead-of-night adaptation floor. Everything else is the parent's, restated
    # so the two scenes are comparable at the same hour.
    #
    # NOTE bloom (PostFxNodeDecompBlur) is deliberately ABSENT: it blacks out
    # the frame on this rig (filed separately). A bonfire is exactly the content
    # that would want it — revisit when that is fixed.
    ##########################

    self.sky(
        skybox_path        = "<ork_envmaps2>/desert4k.xir",
        skybox_intensity   = 1.2,
        diffuse_intensity  = 1.5,
        specular_intensity = 1.0,
        ambient_light      = vec3(0.0),
        msaa               = 2,
        ssaa               = 0,
        latitude_deg  = SITE_LATITUDE_DEG,
        day_of_year   = SITE_DAY_OF_YEAR,
        time_of_day   = TIME_OF_DAY,
        time_scale    = TIME_SCALE,
        phase         = MOON_PHASE,
        celestial     = True,
        moon          = MOON_ON,
        stars         = True,
        sun_color     = vec3(1.0, 0.82, 0.60),
        sun_intensity = 2.5,
        # the ONE tone-stage knob this scene authors. Valid keys are exhaustive
        # and an unknown one RAISES (_sky_dome.py:135-138) — it never reaches
        # declareParams, where it would have been dropped in silence.
        tonemap       = {"adapt_floor": ADAPT_FLOOR},
        shadow_snapshot_interval = 10.0,
        sun_params    = {"shadow_map_size": 4096,
                         "shadow_caster": (os.environ.get("SWEST_SUNSHADOW") != "0"),
                         "shadow_snapshot_bands_per_frame": 1,
                         "shadow_crossfade_frames": 12,
                         "cloud_shadow_strength":
                             float(os.environ.get("SWEST_CLOUD_SHADOW", "3.0")),
                         "cloud_shadow_extent": 9000.0,
                         "cloud_shadow_softness": 0.0},
        # THE AUX CHANNEL (heat_kw above). Declaring it is what makes every
        # ParticlesComponent below auto-attach to the "aux_heat" layer
        # (declare_component appends ",aux_<c>" to layername); materials that
        # never query ctx.is_heat are skipped at draw, so the six non-plume
        # layers cost nothing.
        **heat_kw)

    # SWEST_IBLFADE=<frames>: offscreen STILLS must pass 0 — at unthrottled
    # offscreen fps the auto-sized fade spans hundreds of frames, so a snapshot
    # after a publish captures a half-faded IBL blend.
    iblfade = os.environ.get("SWEST_IBLFADE")
    if iblfade is not None:
      from orkengine.lev2 import SkyAtmosphereData
      atmo = SkyAtmosphereData()
      atmo.ibl_crossfade_frames = int(iblfade)
      self.SG._decl.sub_calls.append(("declareParams", ({"SkyAtmosphere": atmo},), {}))

    self.system_data("HypermeshSystem")
    self.system_data("ParticlesGlobalSystem")

    ##########################
    # TERRAIN FIRST — declaration order IS dependency order: this bake produces
    # the relief AND every placement the drawables below read, the hero pit's
    # included. swestvale_fire is a FORK of swestvale (the parent's graph is
    # shared and must stay byte-identical), so this scene cooks its own terrain.
    #
    # DEFAULT SPAWN = 10 m south of the pit, ON THE GRADED PAD: with the camera
    # script's default 180 deg heading the eye looks straight up the
    # pit-to-village axis, which is the frame this scene exists to make. The
    # walker ground-snaps, so the Y here is only a drop height.
    #
    # 10, NOT 15 (round-5 measurement — the old default was not eye level at
    # all): the pad is flat to PIT_PAD_HALF (8 m) and feathers out by
    # +PIT_PAD_APRON (13 m), so a spawn 15 m out snaps to NATURAL ground about
    # 5.6 m ABOVE the pad and the shipped "eye level" frame was really a 7.3 m
    # crane looking down into the hearth. Inside 13 m the walker stands on the
    # ceremonial ground the pit stands on, which is what a 1.7 m eye means here.
    ##########################
    spawn = vec3(PIT_X, PIT_PAD_Y + 5.5, PIT_Z - 10.0)
    sp = os.environ.get("SWEST_SPAWN")
    if sp:
      p = [float(c) for c in sp.replace(" ", "").split(",")]
      spawn = vec3(p[0], p[1], p[2])
    walkable = (os.environ.get("SWEST_FLY") != "1")
    self.terrain(TERRA,
                 dsl_file         = "swestvale_fire",
                 spawn            = spawn,
                 chunk            = 128,
                 render_dimension = 2048,
                 bake_dimension   = 8192,
                 bake_res         = 8192,
                 mode             = os.environ.get("SWEST_TERRA_MODE", "stored"),
                 walkable         = (dict(eye_height=_envf("SWEST_FIRE_EYE", 1.7))
                                     if walkable else False))
    if walkable:
      # AIM THE CAMERA (see _swest_fire_cam.py): the controller links at
      # heading 0 = looking -Z, and the pit is at +Z from the default spawn.
      # Layered, so walk_input_system keeps the keyboard.
      self.append_system_script(
          os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "_swest_fire_cam.py"))

    ##########################
    # MATERIALS — the village's, unchanged from scn_swest (this scene is the
    # same vale; only the subject moved), then the fire pit's own quarry.
    ##########################
    def _mat(name, cls, **kw):
      return self.asset.Ptex3d(name,
                               dsl_class     = cls,
                               vertex_source = GpuMeshRenderSource(instanced=True),
                               **kw)

    wall = _mat("sw_adobe",
                AdobeCls,
                instance_variation = 0.14)
    roof = _mat("sw_adobe_roof",
                AdobeCls,
                albedo_lo          = vec3(0.42, 0.315, 0.20),
                albedo_hi          = vec3(0.55, 0.43, 0.285),
                grain_m            = 0.18,
                instance_variation = 0.12)
    door = _mat("sw_door",
                TimberCls,
                early_color        = vec3(0.24, 0.17, 0.11),
                late_color         = vec3(0.13, 0.095, 0.07),
                silver             = 0.12,
                roughness          = 0.90)
    trim = _mat("sw_timber",
                TimberCls,
                instance_variation = 0.18)

    for tid, (nm, cls) in enumerate(VARIANTS):
      mesh = self.asset.Hypermesh("bld_pueblo_" + nm,
                                  dsl_class = cls,
                                  **({"sectioned": True} if stored else {}))
      sampler = (_mat("sw_sampler_" + nm, SectionArrayPBR, instance_variation = 0.14)
                 if stored else None)
      for sink in ("buildings", "buildings_hill"):
        tag = "" if sink == "buildings" else "_hill"
        if stored:
          dd = mesh.drawable_data(
              material        = sampler,
              materials       = {0: wall, 2: door, 3: roof, 4: trim},
              section_bake    = True,
              bake_res        = bake_res,
              cull            = True,
              instance_source = (TERRA, sink, tid))
        else:
          dd = mesh.drawable_data(
              material        = wall,
              materials       = {2: door, 3: roof, 4: trim},
              cull            = True,
              instance_source = (TERRA, sink, tid))
        self.entity(
            "swest_" + nm + tag,
            components=[self.declare_component(
                "HypermeshComponent",
                drawabledata = dd,
                layername = "std_forward",
                nodename  = "hm_" + nm + tag)])

    kiva_stone = _mat("sw_kiva_stone",
                      AdobeCls,
                      albedo_lo          = vec3(0.40, 0.36, 0.295),
                      albedo_hi          = vec3(0.56, 0.51, 0.415),
                      grain_m            = 0.14,
                      roughness          = 0.97,
                      instance_variation = 0.10)
    kiva_mesh = self.asset.Hypermesh("bld_kiva",
                                     dsl_class = Kiva,
                                     height    = 1.3,
                                     course_h  = 0.44,
                                     radius    = 3.35,
                                     wall_t    = 1.2,
                                     stone_len = 1.4,
                                     floor     = False,
                                     ladder_drop = 3.0,
                                     **({"sectioned": True} if stored else {}))
    kiva_sampler = (_mat("sw_sampler_kiva", SectionArrayPBR, instance_variation = 0.10)
                    if stored else None)
    if stored:
      kiva_dd = kiva_mesh.drawable_data(
          material        = kiva_sampler,
          materials       = {0: kiva_stone, 4: trim},
          section_bake    = True,
          bake_res        = bake_res,
          cull            = True,
          instance_source = (TERRA, "kivas", 0))
    else:
      kiva_dd = kiva_mesh.drawable_data(
          material        = kiva_stone,
          materials       = {4: trim},
          cull            = True,
          instance_source = (TERRA, "kivas", 0))
    self.entity(
        "swest_kiva",
        components=[self.declare_component(
            "HypermeshComponent",
            drawabledata = kiva_dd,
            layername = "std_forward",
            nodename  = "hm_kiva")])

    ##########################
    # THE HERO FIRE PIT.
    #
    # Three of bld_firepit's ctor kwargs are passed EXPLICITLY from the terrain
    # module's constants rather than left to the asset's defaults, because the
    # terrain derives real geometry from them: hearth_depth is the placement
    # lift AND the hearth cut's datum, rim_h + anchor_rise size the ring
    # collider's half-height. Passing them is what makes the pad, the hole, the
    # collider and the stone one set of numbers. If bld_firepit's own defaults
    # move, this scene keeps rendering the pit the ground was cut for and says
    # so on stdout — update swestvale_fire's constants to adopt the change.
    ##########################
    for _k, _v in (("hearth_depth", PIT_HEARTH_DEPTH),
                   ("rim_h",        PIT_RIM_H),
                   ("anchor_rise",  PIT_ANCHOR_RISE)):
      _d = _firepit_default(_k)
      if abs(float(_d) - float(_v)) > 1e-9:
        print("scn_swest_fire: bld_firepit.%s default is %g but the terrain is "
              "graded for %g — rendering %g (adopt by editing swestvale_fire)."
              % (_k, _d, _v, _v), flush=True)

    # THE PIT'S MATERIALS ARE ON A HARD BUDGET, and the budget is an ENGINE
    # CEILING, not taste. The vulkan backend packs a GLOBAL shader-program index
    # into 8 bits and asserts at 255 (vulkan_fxi_DBread.cpp:857). MEASURED on
    # this scene (bake_dimension 8192, MATMODE proc, the whole vale material set
    # + the particle systems + the heat aux pass), one leg per run:
    #
    #   pit shares the kiva stone, deck ON                    -> PASS
    #   + ONE dedicated pit material (soot),   deck ON        -> ASSERT
    #   + ONE dedicated pit material (soot),   deck OFF       -> PASS
    #   + THREE dedicated pit materials,       deck OFF       -> ASSERT
    #   pit shares the kiva stone, deck ON, heat OFF          -> PASS (heat is
    #                                                            NOT the cost)
    #
    # i.e. there is room for exactly ZERO extra ptex3d materials with the cloud
    # deck declared, and ONE without it. The shipped default keeps the DECK,
    # because the deck is the vale's look at every daylight hour this scene is
    # asked for, and draws the whole pit in the KIVA's drystone material — the
    # same grey-tan sandstone by declaration, and the pit's read is carried by
    # its coursing, its anchors and its entry gap, not by its tint.
    #
    # WHAT THAT COSTS: the fire-blackened hearth floor and inner curb faces
    # (gid 5) draw as clean stone. At night, with the deck invisible anyway,
    # `SWEST_FIRE_PITMTL=1 SWEST_CLOUDS=0` buys the soot back at no visible
    # price — that A/B is in the round-1 gallery for the owner to choose from.
    #
    #   SWEST_FIRE_PITMTL=0  kiva stone everywhere, 0 new materials  (DEFAULT)
    #                    =1  + dedicated SOOT                (needs CLOUDS=0)
    #                    =2  + dedicated bed stone + anchors (needs more headroom)
    #                    =3  the asset's full 4-bed quarry   (needs more headroom)
    _albedos = _firepit_default("stone_albedos")
    _roughs  = _firepit_default("stone_roughs")
    _pitmtl  = int(_envf("SWEST_FIRE_PITMTL", 0))

    def _stone(name, albedo, rough, grain=0.13, var=0.10):
      c = vec3(*albedo)
      return _mat(name, AdobeCls,
                  albedo_lo          = c * 0.90,
                  albedo_hi          = c * 1.10,
                  grain_m            = grain,
                  roughness          = float(rough),
                  instance_variation = var)

    # the bed count is capped by the ASSET's own variant count too, so trimming
    # the quarry there needs no edit here.
    _nvar = min(int(_firepit_default("mtl_variants")), len(_albedos), len(_roughs))
    pit_gids = {}
    if _pitmtl >= 2:
      pit_stone = [_stone("fp_stone%d" % i, _albedos[i], _roughs[i])
                   for i in range(1 if _pitmtl < 3 else max(1, _nvar))]
      # gid 0 is the base draw (material=); 1..N-1 the other beds.
      pit_gids.update({i: pit_stone[i] for i in range(1, len(pit_stone))})
      pit_gids[GID_ANCHOR] = _stone("fp_anchor",
                                    _firepit_default("anchor_albedo"),
                                    _firepit_default("anchor_rough"),
                                    grain=0.17, var=0.06)
    else:
      pit_stone = [kiva_stone]
    if _pitmtl >= 1:
      # SOOT — the hearth floor and the inner curb faces. Dead rough,
      # near-black, and DELIBERATELY not varied: soot is a deposit, not a bed.
      pit_gids[GID_SOOT] = _stone("fp_soot",
                                  _firepit_default("soot_albedo"),
                                  _firepit_default("soot_rough"),
                                  grain=0.09, var=0.02)

    pit_mesh = self.asset.Hypermesh("bld_firepit",
                                    dsl_class    = FirePit,
                                    hearth_depth = PIT_HEARTH_DEPTH,
                                    rim_h        = PIT_RIM_H,
                                    anchor_rise  = PIT_ANCHOR_RISE,
                                    scale_figure = False)
    pit_dd = pit_mesh.drawable_data(
        material        = pit_stone[0],                  # gid 0 = the base bed
        materials       = pit_gids,
        cull            = True,
        instance_source = (TERRA, PIT_SINK, 0))
    self.entity(
        "swest_firepit",
        components=[self.declare_component(
            "HypermeshComponent",
            drawabledata = pit_dd,
            layername = "std_forward",
            nodename  = "hm_firepit")])

    ##########################
    # THE PYRE — THE FUEL (bld_pyre). A 5.4 m crib of timber with a leaner
    # mantle and an ember bed, standing 4.2 m out of the hearth. Without it a
    # 10 m flame column rises off a bare stone slab and reads as a gas jet; with
    # it the fire has something that is BURNING, and the dark backlit timber
    # inside the bright core is what says so.
    #
    # IT RIDES THE PIT'S OWN INSTANCE MATRIX — the same (TERRA, PIT_SINK, 0)
    # scatter entry the stone reads. bld_pyre's mesh origin contract is
    # bld_firepit's (y=0 = the hearth floor plane), so one matrix places both and
    # the two can never drift apart; no terrain edit, no second sink, no lift.
    # `hearth_top` is read from bld_firepit's OWN floor_top default, so a change
    # to the slab carries the stack with it.
    #
    # MATERIALS: ZERO NEW ONES, and that is the whole design constraint (see the
    # pit-material block above — this scene has no shader-program headroom). The
    # pyre declares exactly two gids and both bind to materials the vale has
    # already paid for:
    #     gid 0 (fresh timber) -> `trim` = sw_timber, the pueblo viga/ladder wood
    #     gid 1 (char/embers)  -> `door` = sw_door, Timber at early 0.24/0.17/0.11
    #                             with silver 0.12, i.e. a dark scorched wood by
    #                             declaration already
    # MEASURED: adding this drawable with those two bindings, deck ON, is a PASS.
    #
    #   SWEST_FIRE_PYRE=0   burn on the bare slab (the round-0 A/B)
    #   SWEST_FIRE_PYRE_SEED=<n>  restack the timber (deterministic)
    ##########################
    pyre_on = _envf("SWEST_FIRE_PYRE", 1.0) > 0.0
    if pyre_on:
      _pyre_proxy_drift()
      pyre_mesh = self.asset.Hypermesh("bld_pyre",
                                       dsl_class  = Pyre,
                                       seed       = _envf("SWEST_FIRE_PYRE_SEED", 19.0),
                                       hearth_top = _firepit_default("floor_top"),
                                       **({"sectioned": True} if stored else {}))
      if stored:
        pyre_sampler = _mat("sw_sampler_pyre", SectionArrayPBR, instance_variation = 0.12)
        pyre_dd = pyre_mesh.drawable_data(
            material        = pyre_sampler,
            materials       = {0: trim, GID_CHAR: door},
            section_bake    = True,
            bake_res        = bake_res,
            cull            = True,
            instance_source = (TERRA, PIT_SINK, 0))
      else:
        pyre_dd = pyre_mesh.drawable_data(
            material        = trim,                      # gid 0 = fresh timber
            materials       = {GID_CHAR: door},          # gid 1 = char + ember bed
            cull            = True,
            instance_source = (TERRA, PIT_SINK, 0))
      self.entity(
          "swest_pyre",
          components=[self.declare_component(
              "HypermeshComponent",
              drawabledata = pyre_dd,
              layername = "std_forward",
              nodename  = "hm_pyre")])

    ##########################
    # THE BONFIRE — THIRTEEN drawables, TWELVE lights (streaks publish none).
    # The arithmetic, so it cannot go stale again (the header said eleven while
    # the build emitted twelve):
    #
    #     2 core + 3 plume + 1 embers + 1 smoke          = 7   bonfire.py's own
    #     + 1 column fill  + 1 ember bed  + 4 bounce     = 6   SCENE COMPOSITION
    #                                               total 13 drawables
    #     lights = 13 - 1 (the ember STREAKS publish none) = 12
    #
    # All three scene layers are BonfireCore instances reusing core0's fragment
    # parameters, so the whole set shares ONE compiled material — this scene has
    # zero shader-program headroom (see the pit-material block). Twelve lights
    # is at the bottom of the 12-24 guidance bonfire.py cites, and it has to be
    # — frustum culling is off, so every light costs every pixel. Each is
    # its own entity because the engine publishes exactly ONE DynamicPointLight
    # per particle DRAWABLE, at the sprite renderer's hot-particle centroid: a
    # pool of independently flickering lights IS a set of drawables.
    #
    # The entity transform is IRRELEVANT here and deliberately so — the graph
    # emits in WORLD space and ParticlesComponent keeps its scenegraph node at
    # identity (ParticlesComponent.cpp:573-585), so placement is the emitter's
    # Offset (and the servo's axis) alone. Left at the origin to say that.
    ##########################
    # SWEST_FIRE_LAYERS caps how many layers are declared (the default is ALL of
    # them, currently thirteen — the list truncates in DECLARATION order: cores,
    # plumes, column fill, ember bed, bounce ring, embers, smoke). Lower is a
    # DIAGNOSTIC leg for the program budget / for isolating one layer's look,
    # never a shipping value.
    _layers = _fire_layers()
    for name, cls, kwargs, e_int, e_rad in _layers[:int(_envf("SWEST_FIRE_LAYERS",
                                                              len(_layers)))]:
      # BUILD-THEN-DRAIN, ONE AT A TIME. ParticleSystem.__init__ opens a trace
      # that only closes in generatedflow(); constructing several before
      # draining any nests the traces and every system but the last silently
      # loses its staged Expr bindings — here that is Size, i.e. it renders at
      # default size, i.e. invisibly, with no error anywhere.
      sysobj = cls(**kwargs)
      gd = ParticleSystemGenData(dsl_file  = "bonfire",
                                 dsl_class = cls.__name__)
      gd.graph = sysobj.generatedflow()
      gd.emitter_intensity = float(e_int) * LIGHT_INT_MUL
      gd.emitter_radius    = float(e_rad) * LIGHT_RAD_MUL
      psys = self.asset.ParticleSystem(name, gendata = gd)
      self.entity(
          name,
          components=[self.declare_component(
              "ParticlesComponent",
              drawabledata = psys,
              pool_size    = 1,
              duration     = 0.0)])

    ##########################
    # COLLIDERS — one compound static per scatter sink, built from the proxies
    # the sinks themselves carry. The pit's is a RING annulus (a walk-INTO
    # collar: blocks laterally, leaves the interior open) whose dims come from
    # bld_firepit's GENERATING params via swestvale_fire, never from the render
    # mesh — the physics-proxy law.
    #
    # THE PYRE NEEDS ITS OWN SINK, and the reason is the ring's own design: a
    # walk-INTO collar leaves the hearth open on purpose, so with only the pit
    # declared a walker who comes down the entry gap walks straight through 5 m
    # of stacked timber. swestvale_fire places a second, collider-only sink at
    # the same lattice point carrying bld_pyre's capsule (mantle footprint
    # 2.805 m, crown 4.723 m — its generating grammar, restated there). It is
    # skipped when the pyre is not rendered: SWEST_FIRE_PYRE=0 is the bare-slab
    # A/B and must not leave an invisible stack standing in the fire.
    ##########################
    self.scatter_collider(TERRA,
                          sink        = "buildings",
                          friction    = 0.9,
                          restitution = 0.05)
    self.scatter_collider(TERRA,
                          sink        = "buildings_hill",
                          friction    = 0.9,
                          restitution = 0.05)
    self.scatter_collider(TERRA,
                          sink        = "kivas",
                          friction    = 0.9,
                          restitution = 0.05)
    self.scatter_collider(TERRA,
                          sink        = PIT_SINK,
                          friction    = 0.95,
                          restitution = 0.02)
    if pyre_on:
      self.scatter_collider(TERRA,
                            sink        = PYRE_SINK,
                            friction    = 0.95,
                            restitution = 0.02)

    ##########################
    # CLOUD DECK — scn_swest's, unchanged. WEATHER DATA ONLY: the deck's
    # radiometry follows the live celestial sun, so it reads correctly at every
    # hour this scene's clock can be set to, night included. The horizon color
    # the deck fades toward is likewise not stated: distance now fades through
    # the atmosphere's own in-scatter, so the fade target is this scene's
    # declared medium rather than a frozen color.
    ##########################
    if os.environ.get("SWEST_CLOUDS", "1") != "0":
      cg = _envf("SWEST_CLOUD_GAIN", 2.0)
      self.cloud_decks(
          specs        = [("cloud_cumulus_2k", "cumulus", "cumulus2k")],
          cover        = _envf("SWEST_CLOUD_COVER", 0.20),
          tile         = _envf("SWEST_CLOUD_TILE", 1.0),
          alt_offset_m = _envf("SWEST_CLOUD_BASE", 2000.0),
          sag_datum_m  = 150.0,
          colors       = {
              "lit_color":      (0.60 * cg, 0.575 * cg, 0.55 * cg),
              "shadow_color":   (0.155 * cg, 0.170 * cg, 0.20 * cg),
              "overcast_color": (0.295 * cg, 0.305 * cg, 0.325 * cg),
          })


__all__ = ["SwestFireScene"]
