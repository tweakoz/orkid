###############################################################################
# bld_firepit — a MONUMENTAL CEREMONIAL FIRE PIT for scn_swest_fire: a low,
# broad drystone ring of CYCLOPEAN masonry (11 m outer diameter) enclosing a
# sunken hearth, with a raised inner fire curb framing the blaze. Descended
# from bld_kiva (the Chacoan great kiva) but MASSED FOR 10 m FROM THE START —
# not a scaled-up kiva. The read: Anasazi great kiva at monumental scale meets
# a Beltane bonfire pit. A 1.7 m eye standing at the rim sees stone at
# waist-to-chest; a 5 m fire sits in the curb with ~1.5 m of clearance to it.
#
# TECHNIQUE (inherited from bld_kiva, proven): jittered stone boxes laid in
# running bond -> ONE winding-sign voxelization -> redistance -> adaptive
# volumeToMesh. The SDF pass weathers every block edge round while the radial
# face steps between neighbouring blocks reconstruct as DEEP RECESSED JOINTS —
# one connected shell, no per-stone shell blowup.
#
# WHAT CHANGED vs the kiva, and WHY (the two budgets, designed against):
#
#  1. REGISTER POOL — 64 GpuMesh regs (hmdflow.cpp:507 createRegisters
#     "hm_mesh"); live regs ~= total leaf-box count. The kiva's stone_len=1.15
#     at 5.5 m radius would need ~28 blocks/course x 4 courses = 112 boxes,
#     nearly 2x over cap. So the coursing goes MEGALITHIC: 2 body courses of
#     2.1 m x 0.86 m blocks (2.4:1 — proper cyclopean ashlar) + 1 COPING course
#     of 2.8 m capstones. MEASURED arithmetic at defaults (self.register_arithmetic):
#         course 0 (body,   Rc 5.25): circ 32.98 / (2.10+0.15) -> 16 blocks
#         course 1 (body,   Rc 5.18): circ 32.55 / (2.10+0.15) -> 14 blocks
#         course 2 (coping, Rc 4.83): circ 30.35 / (2.80+0.15) -> 10 capstones
#         + 6 hearth-floor planks + 10 fire-curb blocks     = 56 of 64
#     (counts are rounded UP to even — the alternating bond step below keys off
#     i%2 and an odd count wraps two same-parity blocks with no joint between.)
#     `reg_budget` is a live kwarg and the ctor RAISES with the arithmetic if a
#     param slide would blow the pool — a hard failure beats silent corruption.
#     The optional `scale_figure` validation mannequin costs 3 more (59 of 64).
#
#     ROUND 2 re-count (analytic hearth floor + anchors + threshold):
#         courses 16 + 14 + 10 = 40   (the entry gap SKIPS coping blocks -> fewer live)
#         + 0 hearth floor (now ONE sdf_eval cylinder — zero registers)
#         + 10 fire curb + 4 anchor (2 megaliths x butt+head) + 1 threshold = 55 of 64
#     (+3 with the scale_figure judgment mannequin = 58.)
#
#  2. SDF VOXELS — the brick is CUBIC, so ext must span the full 11 m ring:
#     ext = 2*radius + sdf_margin = 13.0 m. DECISION: keep dim moderate (288,
#     23.9 M voxels) and SIZE THE RELIEF TO THE VOXELS rather than chase 3 cm
#     voxels at dim 448 (90 M — 4x the cost for detail nobody reads at 2 m
#     block scale). 13.0/288 = 4.51 cm voxels; the joint-makers are
#         bond_step 0.13 m  -> a 0.26 m face step at EVERY vertical joint = 5.8 vox
#         course_step 0.07 m -> a 0.14 m neighbour delta between courses  = 3.1 vox
#     both clearing the ">= 2 voxels or it vanishes" contract with room to spare.
#     This is ALSO the honest look: real megalithic drystone with 2 m blocks has
#     10-25 cm shadow joints, not 8 cm ones. Slide `sdf_dim` up for finer.
#     WATCH THE BRICK WALL: anything you add must sit inside half-ext 6.50 m.
#     Two separate bugs this round were geometry silently clipped there (the
#     hearth floor's corners, then the scale figure) — check before you add.
#
# MESH ORIGIN CONTRACT: y=0 is the HEARTH FLOOR plane (= the ring base, same
# convention as bld_kiva). Exterior grade sits at y = hearth_depth, so a scene
# that sinks the terrain by hearth_depth and drops this mesh on the pit floor
# (lift = -hearth_depth) shows `rim_h` of stone proud of the court and the rest
# lining the cut. Standalone (no sink) the whole hearth_depth+rim_h wall stands
# free — render the eye at y = hearth_depth + 1.7 to judge rim height honestly.
#
# PHYSICS-PROXY (law): the collider derives from the GENERATING params only —
# ring radii / heights / hearth depth — never from the render mesh. The single
# `collider_half_extents` box is the bld_kiva-compatible proxy (a solid drum:
# the pit is not enterable). The RING grammar a scene needs to build a walkable
# annulus compound is published alongside it as `ring_proxy` — same params,
# no mesh inspection.
#
# GID MAP (ROUND 2 — the quarry): 0..3 = four STONE VARIANTS (per-block albedo /
# roughness), 4 = the ANCHOR megaliths, 5 = SOOT (hearth floor + inner curb faces).
# Scenes may bind their own material per gid; the defaults are a dry-sandstone quarry.
#
# PER-BLOCK MATERIAL (round 2, the biggest look gain): gids do NOT survive
# sdf_to_mesh_clean, so the blocks are re-identified AFTER the remesh by an EXACT
# WEDGE TEST against the same azimuthal partition that built them — no atan needed
# and no drift, because the build RECORDS each block's (mid azimuth, half width, y
# span, radial band) into self._blocks and the selects read that list back:
#     in-wedge  <=>  dot(P.xz, (cos m, sin m))  >=  cos(half_w) * length(P.xz)
# One select+assign_gid per variant (they share the `_B_MTL` scratch bit; `replace`
# makes the group exactly the matched set, so the bit is reusable). This is the
# standard "attribute-driven material masking" idiom — capture the generating
# attribute, mask on it downstream — with the generating attribute being the
# quarry partition itself rather than a baked texture channel.
#
# SILHOUETTE (round 2): a flat annulus reads MACHINED from a distance, so two
# oversized ANCHOR megaliths (butt + lapped head, standing `anchor_rise` proud of
# the rim) punctuate the ring, and one ENTRY GAP — coping omitted, body course
# clipped to a sill, one threshold stone laid across — gives the eye a way in, as
# every great kiva has. Both are param-driven and published in `ring_proxy`.
#
# VET: allow_self_intersect — blocks overlap by construction (the contact law).
# `fanfold` USED TO FAIL here (36 folded polys) and round 1 recorded it as inherent
# to the mesh_to_sdf -> volumeToMesh chain. It is not: it was the QUAD-DOMINANT
# volumeToMesh output meeting the render's GPU fan triangulator. `triangulate=True`
# (sdf_to_mesh_clean unwrap, i.e. xatlas) makes it PASS — 0 folded polys of 99768,
# and the shell count drops 3 -> 1. Cost: ~1.65x faces (60k -> 100k).
#
#   ork.hypermesh.viewer.py bld_firepit --msaa 2
#   _ork.hypermesh.validate.py bld_firepit -o /tmp/bld_firepit.obj
###############################################################################
import math
import random
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, HmMaterial, S, replace, group, POLY
from ork.hypergraph.assets.materials.terrain.solid import Solid

_B_TAIL = 6                       # scratch tag bit for degenerate-sliver deletion
_B_MTL  = 5                       # scratch tag bit the per-block material selects reuse

GID_STONE0 = 0                    # the four quarried-stone variants (gid 0 = the base draw)
GID_ANCHOR = 4                    # the oversized anchor megaliths
GID_SOOT   = 5                    # hearth floor + inner curb faces (fire-blackened)


def _tangent_yaw(a):
  """Y-yaw (degrees) laying a box's LOCAL +X along the ring TANGENT at azimuth
  `a` (position (R cos a, 0, R sin a)). orkid's axis-angle Y rotation is
  right-handed (+X -> (cos t, 0, -sin t)); solving cos t = -sin a and
  -sin t = cos a gives t = -(a + 90deg)."""
  return -(math.degrees(a) + 90.0)


def _oriented_box(hm, size, yaw_deg, at, roll_deg=0.0):
  """Place a box of world SIZE `size`, yawed `yaw_deg` about +Y (and optionally
  rolled `roll_deg` about its OWN length axis — the drystone tip), centred at `at`.

  TWO TRANSFORMS, NOT ONE — and this is load-bearing. `Hypermesh.transform`
  composes its convenience args as `Tt * Tp * mtx4.composed(0, q, scale) * Tpi`,
  and `mtx4.composed` builds T*S*R: the NON-UNIFORM SCALE IS APPLIED IN WORLD
  AXES *AFTER* THE ROTATION. So a one-shot `transform(scale=..., rotate=...)`
  does not produce a rotated box — it produces a world-axis-aligned box whose
  extents are the ROTATED unit box's AABB times the scale, and the yaw's SIGN
  has no effect at all.

  Measured on a 3.0 x 0.4 x 0.3 box yawed 135deg at radius 4 (scratch probe,
  both yaw signs):
      one-shot : X span 4.24 (= 3.0*sqrt2)  Z span 0.42 (= 0.3*sqrt2)  <- axis-aligned
      two-stage: X span 2.33               Z span 2.33   max radius 4.413
      (2.33 / 4.413 are the exact analytic values for a correctly tangent box)

  bld_kiva uses the one-shot form for its stones, which is why its ring reads as
  a starburst of slabs fanning outward instead of courses lying along the wall
  (/tmp/kiva_ref.png) — at 1.15 m stones that passes for rubble texture; at 2.1 m
  megaliths on an 11 m ring it is unmissable, and it also threw ~6.9 m-radius
  geometry past the SDF brick wall. Scaling FIRST (rotation identity) and then
  applying a PURE RIGID rotate+translate (scale left at 1) is the fix; it is the
  same chaining bld_kiva already uses for its ladder poles."""
  b = hm.box(size=0.5)
  b = hm.transform(b, scale=size)                                   # world-axis scale, no rotation
  # rotate list composes LEFT-TO-RIGHT (q = q_yaw * q_roll), so the roll acts in the
  # box's own frame (about local +X = its length) and the yaw then swings it onto the ring.
  return hm.transform(b, rotate=[(vec3(0, 1, 0), yaw_deg), (vec3(1, 0, 0), roll_deg)],
                      translate=at)                                 # pure rigid


class FirePit(Hypermesh):
  VET = dict(allow_self_intersect=True)

  def __init__(self, seed=7, *,
               # ---- primary massing (meters) ----------------------------------
               radius=5.5,            # OUTER radius of the ring -> 11.0 m outer dia
               wall_t=1.2,            # wall thickness -> 8.6 m inner clear dia
               rim_h=1.25,            # stone proud of EXTERIOR grade. Measured against the
                                      # 1.735 m scale figure: navel ~1.05, nipple line ~1.28,
                                      # so 1.02 sits at waist/hip and 1.15 at armpit only in
                                      # a flipped render. 1.25 is true mid-chest.
                                      # Raising this occludes more of the fire's core from a
                                      # 1.7 m eye — re-seat the core's base_y, not the rim.
               hearth_depth=0.65,     # exterior grade above the hearth floor
               skirt_m=0.40,          # bedding course buried below the hearth floor
               # ---- coursing --------------------------------------------------
               body_courses=2,        # megalithic body courses (register budget lives here)
               course_h=0.0,          # >0 OVERRIDES body_courses (derives it from the span)
               coping_h=0.48,         # capstone course height (0 = no distinct coping)
               block_len=2.10,        # body block chord length
               coping_len=2.80,       # capstone chord length (long slabs = the "finished" read)
               joint_az=0.15,         # azimuthal joint allowance (sets blocks-per-course)
               batter_m=0.42,         # basal outward swell (lower courses thicken)
               course_step=0.07,      # per-course radius alternation -> horizontal course lines
               coping_lip=1.10,       # capstone thickness x wall_t (>1 = a projecting cornice)
               # ---- relief (the joint-makers) ---------------------------------
               bond_step=0.13,        # DETERMINISTIC alternating face projection, +/- m.
                                      # every vertical joint is a guaranteed 2*bond_step
                                      # step (0.26 m = ~6 voxels) — the primary relief
               jitter_r=0.06,         # RANDOM radial step on top, +/- m (breaks the metronome)
               tilt_deg=2.5,          # per-block roll about its own length axis
               len_var=0.34,          # block length spread: each block's share of the
                                      # circumference is 1 +/- len_var (quarried, not cast)
               height_var=0.16,       # per-block top variation as a fraction of course_h
               chord_lap=0.10,        # azimuthal chord overlap (closes the wall)
               course_lap=0.12,       # m each block drops into the course below (closes it
                                      # vertically, so tops are free to wander)
               jitter=1.0,            # global scale on the size/yaw/height wobbles
               # ---- hearth interior -------------------------------------------
               floor=True,            # hearth floor slab
               floor_analytic=True,   # ROUND 2: ONE sdf_eval CYLINDER union'd into the brick —
                                      # zero registers and no faceting. False = the legacy
                                      # crossed-plank 2N-gon (floor_planks registers, faceted).
               floor_planks=6,        # legacy path only: N crossed planks -> a regular 2N-gon
               floor_top=0.15,        # floor surface height above y=0
               floor_over=0.35,       # floor radius overshoot INTO the masonry (closes the shell)
               firering=True,         # raised inner curb framing the blaze
               fire_r=3.00,           # curb centreline radius -> 6.0 m curb dia (5 m fire fits)
               fire_h=0.52,           # curb height above the hearth floor
               fire_t=0.68,           # curb thickness
               fire_n=10,             # curb blocks
               # ---- silhouette breakers (round 2) -----------------------------
               anchor_azs=(38.0, 262.0),   # azimuths (deg) of the oversized ANCHOR megaliths
               anchor_rise=0.55,      # m the anchor tops stand PROUD of the rim
               anchor_len=2.20,       # anchor (butt) chord length
               anchor_t_mul=1.34,     # anchor butt thickness x wall_t
               anchor_r_off=0.30,     # anchor centre radius = ring midline + this (proud outward)
               anchor_yaw_off=5.0,    # deg off-tangent (hand-placed, not machined)
               anchor_lean=4.0,       # deg the megalith leans (roll about its own length axis)
               anchor_cap_frac=0.62,  # butt/head split as a fraction of the anchor's full height
               anchor_cap_lap=0.20,   # m the head laps back down over the butt (a real joint)
               anchor_cap_len=0.76,   # head length x anchor_len
               anchor_cap_t=0.84,     # head thickness x the butt's
               anchor_cap_r=-0.09,    # head radius offset vs the butt (a shoulder, not a wall)
               entry=True,            # a low ENTRY GAP / threshold in the ring (great kivas have one)
               entry_az_deg=200.0,    # azimuth of the gap centre
               entry_w_deg=30.0,      # angular width of the gap
               entry_drop=0.55,       # m of wall removed at the gap (rim -> sill)
               entry_sill_h=0.16,     # threshold stone thickness lying across the sill
               # ---- material variation (round 2) ------------------------------
               mtl_variants=4,        # per-block stone variants (gids 0..N-1); 1 = uniform
               soot=True,             # gid the hearth floor + inner curb faces as fire-blackened
               soot_r_pad=0.12,       # soot band reaches this far outside the curb centreline
               soot_h_pad=0.06,       # soot band reaches this far above the curb top
               # the quarry palette — one (albedo, roughness) per stone variant, then the
               # anchor megaliths and the soot. Beds of the same sandstone weather to
               # different values; the ROUGHNESS spread is what stops the ring reading as
               # one moulded plastic surface under a raking sun.
               # SPREAD DISCIPLINE: the first pass ran 0.40..0.61 (a 1.5x value ratio) and
               # read as camouflage patchwork, not one quarry. Real bedded sandstone varies
               # ~1.2x in value and much more in HUE and SHEEN, so the spread lives mostly
               # in hue + roughness now.
               stone_albedos=((0.52, 0.46, 0.38),     # 0 base bed
                              (0.60, 0.54, 0.44),     # 1 pale, sun-bleached
                              (0.43, 0.37, 0.30),     # 2 dark, damp/lichened
                              (0.55, 0.44, 0.32)),    # 3 warm iron-stained
               stone_roughs=(0.96, 0.90, 0.99, 0.86),
               anchor_albedo=(0.47, 0.42, 0.36),      # the megaliths: a different quarry
               anchor_rough=0.99,
               soot_albedo=(0.17, 0.155, 0.145),      # fire-blackened hearth
               soot_rough=0.97,
               # ---- solve -----------------------------------------------------
               sdf_dim=288,
               sign_mode=1,           # mesh_to_sdf sign: 1 winding / 0 pseudonormal / -1 auto
               isovalue=0.0,          # volumeToMesh iso offset (m): <0 shaves the surface INWARD
               sdf_margin=2.00,       # ext = 2*radius + sdf_margin (cubic brick)
               adaptivity=0.18,       # openvdb volumeToMesh: LOW, or it flattens the joints
               triangulate=True,      # xatlas-triangulate the remesh instead of shipping
                                      # n-gons to the render's GPU fan triangulator: turns
                                      # the `fanfold` vet FAIL into a PASS (see the remesh
                                      # note) at ~1.65x faces. False = round-1 behaviour.
               reg_budget=64,         # hm_mesh register pool (hmdflow.cpp:507)
               hard_shade=True,       # face normals: flat block faces (stone), not melted
               scale_figure=False,    # RENDER-VALIDATION ONLY: a 1.75 m human at the rim
               figure_gap=0.85,       # figure standoff from the OUTER radius (must stay
                                      # inside the SDF brick: gap < sdf_margin/2 + slack)
               figure_az_deg=25.0,    # where round the ring the figure stands
               sectioned=False, section_padding=4, section_max_layers=8,
               **_overrides):
    super().__init__()
    rng = random.Random(int(seed))

    Rm    = radius - wall_t * 0.5                 # ring mid-line radius
    y_lo  = -float(skirt_m)                       # bedding bottom
    y_top = float(hearth_depth) + float(rim_h)    # rim top (above the hearth floor)
    cop_h = max(0.0, float(coping_h))
    body_span = (y_top - cop_h) - y_lo
    if body_span <= 0.0:
      raise ValueError("bld_firepit: coping_h %.2f swallows the whole wall span" % cop_h)
    n_body = (max(1, int(round(body_span / float(course_h)))) if float(course_h) > 0.0
              else max(1, int(body_courses)))
    ch      = body_span / n_body                  # derived body course height
    n_course = n_body + (1 if cop_h > 0.0 else 0)

    # ---- REGISTER BUDGET: count before we build, fail loudly rather than corrupt.
    def _course_geo(c):
      """(Rc, block_len, height, y_center) for course index c — the ONE place the
      coursing grammar lives, so the budget count and the build cannot diverge."""
      is_cop = (cop_h > 0.0 and c == n_course - 1)
      t   = c / max(1, n_course - 1)              # 0 at the base .. 1 at the rim
      Rc  = Rm + float(batter_m) * (1.0 - t) \
               + (float(course_step) if (c % 2) else -float(course_step))
      L   = float(coping_len) if is_cop else float(block_len)
      h   = cop_h if is_cop else ch
      y0  = (y_top - cop_h) if is_cop else (y_lo + c * ch)
      return Rc, L, h, y0, is_cop            # y0 = course BOTTOM (blocks size themselves off it)

    counts = []
    for c in range(n_course):
      Rc, L, _h, _cy, _ic = _course_geo(c)
      n = max(8, int(round(2.0 * math.pi * Rc / (L + float(joint_az)))))
      counts.append(n + (n & 1))     # FORCE EVEN: the alternating bond step below
                                     # relies on i%2 parity, and an odd count wraps
                                     # two same-parity blocks into one another with
                                     # no face step -> one joint silently missing.
    # the ANALYTIC floor is an sdf_eval brick union'd downstream — it costs NO register
    # (and no 12-gon faceting); only the legacy plank path spends them.
    n_floor  = (max(2, int(floor_planks)) if (floor and not floor_analytic) else 0)
    n_curb   = int(fire_n) if firering else 0
    n_anchor = 2 * len(tuple(anchor_azs or ()))     # each megalith = a butt + a lapped head
    n_entry  = 1 if (entry and float(entry_sill_h) > 0.0) else 0
    n_fig    = 3 if scale_figure else 0
    # NB the count is an UPPER BOUND: the entry gap SKIPS the coping blocks inside its
    # window, so the live leaf count is a few under. Budget against the bound.
    n_regs  = sum(counts) + n_floor + n_curb + n_anchor + n_entry + n_fig
    self.register_arithmetic = dict(courses=counts, floor=n_floor, curb=n_curb,
                                    anchors=n_anchor, entry=n_entry,
                                    figure=n_fig, total=n_regs, budget=int(reg_budget))
    if n_regs > int(reg_budget):
      raise ValueError(
        "bld_firepit: %d leaf boxes exceeds the %d-register hm_mesh pool "
        "(courses %s + floor %d + curb %d + anchors %d + entry %d + figure %d). Raise "
        "block_len/coping_len, drop a course, or lower fire_n." % (n_regs, int(reg_budget),
        counts, n_floor, n_curb, n_anchor, n_entry, n_fig))

    # ---- THE QUARRY LEDGER — every leaf block records the wedge/band it occupies so the
    # POST-REMESH material selects can find it again (gids do not survive the SDF round
    # trip). One list, appended by every placer below; read once, at the re-gid.
    # entry: (mid_azimuth_rad, half_width_rad, y_lo, y_hi, r_lo, r_hi, variant_gid)
    self._blocks = []
    n_var = max(1, int(mtl_variants))
    ent_a = math.radians(float(entry_az_deg))
    ent_h = math.radians(float(entry_w_deg)) * 0.5

    def _daz(a):
      """signed angular distance a - entry_az, wrapped to [-pi, pi]."""
      d = (a - ent_a) % (2.0 * math.pi)
      return d - 2.0 * math.pi if d > math.pi else d

    # ---- CYCLOPEAN COURSED DRYSTONE.
    # CONTACT LAW (inherited from bld_kiva, hard-won 07-22): blocks TOUCH. Chords
    # OVERLAP and courses stack at full pitch — a literal air gap at this voxel size
    # survives the SDF as a see-through HOLE, not a joint. So relief has to come from
    # FACE OFFSETS, and the round-3 render taught the rest: PURELY RANDOM offsets are
    # not enough. Two neighbours can draw similar radii, their joint disappears, and
    # an 11 m ring of 2.1 m blocks reads as one smooth moulded tub.
    #
    # The fix is the mason's own trick and the standard games idiom for stone walls:
    # ALTERNATE THE PROJECTION. Every odd block steps out by +bond_step, every even
    # one steps in by -bond_step, so EVERY vertical joint is a guaranteed 2*bond_step
    # face step (26 cm at defaults = ~6 voxels — it cannot be smoothed away); the
    # random jitter_r then rides on top so the wall is not a metronome. Blocks also
    # ROLL a couple of degrees about their own length axis (real drystone tips), which
    # breaks the shared face plane the SDF would otherwise weld flat.
    # Round 4 taught the last lesson: guaranteed steps at UNIFORM pitch read as
    # PRECAST CONCRETE — every block the same length, the in/out alternation a
    # metronome. Real cyclopean masonry is quarried block by block, so two more
    # things vary, and both are the standard idiom:
    #   * BLOCK LENGTH — the course is a RANDOM PARTITION of the circumference
    #     (each block draws a share in [1-len_var, 1+len_var], normalised to 2*pi),
    #     not n identical chords. Long/short neighbours are what sell "quarried".
    #   * COURSE LINE — each block's TOP varies while its BOTTOM drops `course_lap`
    #     into the course beneath. The wall stays closed (the overlap guarantees it)
    #     but the horizontal lines WANDER, and a proud block's exposed top becomes a
    #     real ledge against the recessed block above it.
    ring = None
    for c in range(n_course):
      Rc, L, h, y0c, is_cop = _course_geo(c)
      n_st  = counts[c]
      # capstones are DELIBERATELY PLACED: less wobble than the body, and they sit
      # PROUD of the course below (coping_lip) so a continuous shadow line runs under
      # the rim — the cornice read that separates "monument" from "pile of rock".
      jscale = float(jitter) * (0.55 if is_cop else 1.0)
      jr     = float(jitter_r) * (0.55 if is_cop else 1.0)
      bs     = float(bond_step) * (0.6 if is_cop else 1.0)
      th     = wall_t * (float(coping_lip) if is_cop else 1.0)
      lv     = float(len_var) * (0.6 if is_cop else 1.0)
      hv     = float(height_var) * (0.45 if is_cop else 1.0)
      # random partition of the circumference -> per-block angular widths
      wts   = [1.0 + rng.uniform(-lv, lv) for _ in range(n_st)]
      norm  = (2.0 * math.pi) / sum(wts)
      widths = [w * norm for w in wts]
      phase = (c % 2) * 0.5 * widths[0]                    # running bond stagger
      acc   = phase
      prev_v = -1
      for i in range(n_st):
        w  = widths[i]
        a  = acc + w * 0.5                                 # block centre azimuth
        acc += w
        chord = 2.0 * Rc * math.sin(w * 0.5)               # exact chord for this share
        sl = chord * (1.0 + float(chord_lap))              # OVERLAP: the wall stays closed
        y1 = y0c + h * (1.0 + rng.uniform(-hv, hv) * jscale)      # wandering top
        yb = y0c - float(course_lap)                              # bite into the course below
        st = th * (1.0 + rng.uniform(-0.10, 0.14) * jscale)
        rr = Rc + (bs if (i % 2) else -bs) * (1.0 + rng.uniform(-0.45, 0.45) * jscale) \
                + rng.uniform(-jr, jr)                            # THE joint-maker
        v  = rng.randrange(n_var - 1) if n_var > 1 else 0         # per-block stone variant:
        if n_var > 1 and v >= prev_v:                             # draw from the OTHERS so two
          v += 1                                                  # neighbours never share (the
        prev_v = v                                                # sampling-without-repeat idiom)
        # ---- ENTRY GAP: inside the window the coping is OMITTED and the body course
        # tops are CLIPPED to the sill, so the ring dips to a threshold the eye can
        # enter through — the great kiva's own antechamber opening, at bonfire scale.
        if entry and abs(_daz(a)) < ent_h:
          if is_cop:
            continue                                       # capstone omitted -> the notch
          y1 = min(y1, y_top - float(entry_drop))
          if y1 <= yb + 0.05:
            continue
        sh = y1 - yb
        b = _oriented_box(self, (sl, sh, st),
                          _tangent_yaw(a) + rng.uniform(-2.2, 2.2) * jscale,
                          (rr * math.cos(a), (yb + y1) * 0.5, rr * math.sin(a)),
                          roll_deg=rng.uniform(-1.0, 1.0) * float(tilt_deg) * jscale)
        ring = b if ring is None else self.merge(ring, b, gid_a=None, gid_b=None)
        # ledger: the wedge this block owns (radial gate keeps the hearth floor and the
        # fire curb — which live at much smaller radii — out of the wall's wedges).
        self._blocks.append((a, w * 0.5, yb - 0.03, y1 + 0.05,
                             (radius - wall_t) - 0.60, 99.0, v))

    # ---- HEARTH FLOOR: a flagged 2N-gon of crossed PLANKS filling the ring from the
    # bedding up to floor_top. Inradius is sized to the BATTERED inner face at the
    # base (the inner radius is WIDEST there), so the shell closes with no annular
    # gap and the plank ends embed in the masonry (allow_self_intersect).
    #
    # PLANK WIDTH IS NOT FREE (bug caught by the first bbox read): crossed SQUARES —
    # the bld_kiva floor idiom — have circumradius = 1.414 x inradius. At the kiva's
    # 2.6 m that overshoot still hid inside the wall; at 5.5 m it puts the corners at
    # 7.0 m, OUTSIDE both the ring AND the 12.8 m SDF brick, and the mesher seals the
    # clipped slab against the brick wall (bbox read exactly 12.800 m). The fix is the
    # regular-polygon half-width: for N planks at 180/N deg apart, half-width =
    # inradius * tan(90/N) makes the union a REGULAR 2N-gon with circumradius only
    # inradius/cos(90/N). N=4 -> a 16-gon, 1.082x — comfortably buried in the wall.
    #
    # ROUND 2 — THE ANALYTIC FLOOR (default). The plank union is a 2N-gon, and at 10 m
    # a 12-gon READS: the establishing shot showed the hearth as a faceted dodecagon,
    # not a round pit. The fix costs ZERO registers and zero faceting: bake ONE cylinder
    # EXPRESSION into its own brick (same framing as the ring's) and CSG-UNION the two
    # before the redistance. The expression is a FLAT one-liner —
    #     max(length(p.xz) - R, abs(p.y - cy) - hh)
    # — deliberately NOT a nested SdfExpr tree: shadlang's PEG parser backtracks
    # exponentially on nested union chains (the _building.py bank: 3 unions -> 68 GB).
    # One flat max() of two terms parses in the noise.
    f_r = (radius - wall_t) + float(batter_m) + float(floor_over)
    floor_sdf_glsl = None
    if floor and floor_analytic:
      f_cy = 0.5 * (y_lo + float(floor_top))
      f_hh = 0.5 * (float(floor_top) - y_lo)
      floor_sdf_glsl = "max(length(p.xz) - %.6f, abs(p.y - %.6f) - %.6f)" % (f_r, f_cy, f_hh)
      self.floor_circumradius = f_r                           # a true circle: R == circumradius
    if floor and not floor_analytic:
      f_y0, f_y1 = y_lo, float(floor_top)
      N = max(2, int(floor_planks))
      f_w = f_r * math.tan(math.radians(90.0 / N))            # plank HALF-width
      self.floor_circumradius = f_r / math.cos(math.radians(90.0 / N))
      for k in range(N):
        f = _oriented_box(self, (2.0 * f_r, f_y1 - f_y0, 2.0 * f_w),
                          k * 180.0 / N,
                          (0.0, (f_y0 + f_y1) * 0.5, 0.0))
        ring = self.merge(ring, f, gid_a=None, gid_b=None)

    # ---- FIRE CURB: the raised inner ring that holds the blaze — the great kiva's
    # masonry firebox, scaled to a bonfire. Same coursing grammar, one course.
    if firering and n_curb > 0:
      pitch = 2.0 * math.pi / n_curb
      prev_v = -1
      for i in range(n_curb):
        cjr = float(jitter_r) * 0.8      # curb's OWN relief scales (not the
        cbs = float(bond_step) * 0.7     # coursing loop's leaked loop variables)
        a  = i * pitch + rng.uniform(-0.05, 0.05) * pitch * float(jitter)
        sl = pitch * float(fire_r) * (1.0 + rng.uniform(0.10, 0.20) * float(jitter))
        rr = float(fire_r) + (cbs if (i % 2) else -cbs) + rng.uniform(-cjr, cjr)
        cy = float(floor_top) + float(fire_h) * 0.5 - 0.06   # seated INTO the floor slab
        b = _oriented_box(self, (sl, float(fire_h) + 0.12, float(fire_t)),
                          _tangent_yaw(a) + rng.uniform(-3.0, 3.0) * float(jitter),
                          (rr * math.cos(a),
                           cy + rng.uniform(-0.04, 0.04) * float(jitter),
                           rr * math.sin(a)),
                          roll_deg=rng.uniform(-1.0, 1.0) * float(tilt_deg) * float(jitter))
        ring = self.merge(ring, b, gid_a=None, gid_b=None)
        v = rng.randrange(n_var - 1) if n_var > 1 else 0
        if n_var > 1 and v >= prev_v:
          v += 1
        prev_v = v
        # curb ledger: gated to the curb's OWN radial band so its wedges never claim
        # the hearth floor (r < fire_r - fire_t) or the wall (r > fire_r + fire_t).
        self._blocks.append((a, pitch * 0.5, float(floor_top) - 0.02,
                             float(floor_top) + float(fire_h) + 0.06,
                             float(fire_r) - float(fire_t), float(fire_r) + float(fire_t), v))

    # ---- ANCHOR MEGALITHS (round 2): one or two DELIBERATELY oversized stones set
    # through the full wall height and standing `anchor_rise` PROUD of the rim. The
    # ring silhouette at establishing distance was a perfect flat annulus — a machined
    # read. Real megalithic enclosures (Avebury, the kiva's own bench pilasters) are
    # punctuated by one or two stones that dwarf the coursing, and the eye uses them to
    # scale the whole ring. They are placed by the SAME two-stage scale-then-rigid-rotate
    # chain as the coursework and get their own gid, so the material can differ too.
    # TWO STONES, NOT ONE (first-pass fix): a single full-height box read as a smooth
    # CONCRETE PIER — 2.6 m of unbroken flat face has no quarry information in it. A
    # BUTT + a lapped HEAD, each with its own yaw, radius and lean, puts a real joint
    # and a shoulder step across that face and the megalith reads as set stone. Cost:
    # 2 registers per anchor instead of 1.
    anchor_top_y = y_top + (float(anchor_rise) if anchor_azs else 0.0)
    for k, adeg in enumerate(tuple(anchor_azs or ())):
      a    = math.radians(float(adeg))
      sgn  = 1.0 if (k % 2) else -1.0
      ar   = Rm + float(anchor_r_off)
      at   = wall_t * float(anchor_t_mul)
      a_y1 = y_top + float(anchor_rise)
      y_sp = y_lo + float(anchor_cap_frac) * (a_y1 - y_lo)        # butt top / head bottom
      for (bl, bt, br, by0, by1, yaw_s, lean_s) in (
          (float(anchor_len), at, ar, y_lo, y_sp, 1.0, 1.0),      # butt: seated on the bedding
          (float(anchor_len) * float(anchor_cap_len),              # head: lapped, stepped back,
           at * float(anchor_cap_t), ar + float(anchor_cap_r),     #       leaning the other way
           y_sp - float(anchor_cap_lap), a_y1, -0.6, -1.3)):
        b = _oriented_box(self, (bl, by1 - by0, bt),
                          _tangent_yaw(a) + sgn * float(anchor_yaw_off) * yaw_s,
                          (br * math.cos(a), (by0 + by1) * 0.5, br * math.sin(a)),
                          roll_deg=sgn * float(anchor_lean) * lean_s)
        ring = b if ring is None else self.merge(ring, b, gid_a=None, gid_b=None)
      a_half = math.asin(min(0.999, (float(anchor_len) * 0.5) / max(0.1, ar)))
      self._blocks.append((a, a_half, y_lo - 0.03, a_y1 + 0.05,
                           (radius - wall_t) - 0.60, 99.0, GID_ANCHOR))

    # ---- ENTRY THRESHOLD: one flat stone lying ACROSS the sill of the entry gap, top
    # flush-proud of the clipped course. Without it the gap reads as damage; with it,
    # as a doorway. Sized off the SAME window the coursing clipped itself to.
    if entry and n_entry:
      s_y1 = y_top - float(entry_drop) + float(entry_sill_h)
      s_l  = 2.0 * Rm * math.sin(ent_h) * 1.04
      b = _oriented_box(self, (s_l, float(entry_sill_h) + 0.14, wall_t * 1.18),
                        _tangent_yaw(ent_a),
                        (Rm * math.cos(ent_a), s_y1 - (float(entry_sill_h) + 0.14) * 0.5,
                         Rm * math.sin(ent_a)))
      ring = self.merge(ring, b, gid_a=None, gid_b=None)
      self._blocks.append((ent_a, ent_h, s_y1 - (float(entry_sill_h) + 0.14) - 0.03, s_y1 + 0.05,
                           (radius - wall_t) - 0.60, 99.0, GID_ANCHOR))

    # ---- SCALE FIGURE (render-validation aid; OFF by default, physics-excluded).
    # A 1.75 m blocked-out human standing on EXTERIOR GRADE (y = hearth_depth) just
    # outside the wall. Eye-level renders of an 11 m ring are worthless without one —
    # a monumental pit and a garden planter are the same picture otherwise. It goes
    # through the SAME SDF as the stone, so it costs no extra solve; turn it on only
    # for judgment renders. NOT part of the physics proxy, and never on in a scene.
    if scale_figure:
      # MUST stand INSIDE the SDF brick (half-extent = radius + sdf_margin/2). The
      # first attempt at radius+1.15 = 6.65 sat past the 6.5 m brick wall and the
      # figure was silently clipped out of the mesh entirely — the same class of bug
      # the hearth-floor octagon hit. figure_gap is measured from the OUTER radius.
      # ROUND 2 CONVENTION FIX: the figure used to be placed at (R sin a, R cos a) —
      # a COMPASS azimuth — while every other placer in this file uses (R cos a,
      # R sin a). The 90-deg disagreement meant `figure_az_deg=103` put the figure at
      # ring azimuth -13 and judgment renders aimed at it kept coming back empty.
      # It is now the SAME azimuth convention as the coursing, anchors and entry.
      fa = math.radians(float(figure_az_deg))
      fR = float(radius) + float(figure_gap)
      gx, gz = fR * math.cos(fa), fR * math.sin(fa)
      g = float(hearth_depth)                                 # feet on EXTERIOR grade
      for size, ycen in (((0.34, 0.86, 0.26), g + 0.43),      # legs
                         ((0.48, 0.62, 0.28), g + 1.17),      # torso
                         ((0.21, 0.25, 0.23), g + 1.61)):     # head  -> top 1.735 m
        fig = _oriented_box(self, size, _tangent_yaw(fa) + 90.0, (gx, ycen, gz))   # facing the fire
        ring = self.merge(ring, fig, gid_a=None, gid_b=None)

    # ---- ONE voxelization at joint-preserving fidelity (explicit extent, one
    # framing, winding sign — the banked bld_kiva gotchas). See the header for the
    # ext/dim decision: 13.0 m @ 288 = 4.5 cm voxels.
    ext = 2.0 * radius + float(sdf_margin)
    self.voxel_m = ext / float(sdf_dim)
    sd = self.mesh_to_sdf(ring, dim=int(sdf_dim), extent=ext, center=(0.0, 0.0, 0.0), sign_mode=int(sign_mode))
    if floor_sdf_glsl is not None:            # the ROUND hearth floor, union'd in analytically
      fl = self.sdf_eval(floor_sdf_glsl, dim=int(sdf_dim), extent=ext, center=(0.0, 0.0, 0.0))
      sd = self.csg(sd, fl, op="union")       # SAME framing as the voxelized ring (csg rides A's)
    rd = self.redistance(sd)
    # TRIANGULATE (round 2). unwrap=True routes the remesh through xatlas, which
    # triangulates the quad-dominant volumeToMesh output properly instead of leaving
    # n-gons for the renderer's GPU FAN triangulator. Measured effect on the vet:
    #     fanfold  FAIL 36-51 folded polys  ->  PASS (0 of 99768)
    #     shells   3  ->  1        collapse 2 edges + 10 slivers -> 0 + 1
    # Round 1 had recorded the fanfold FAIL as inherent to mesh_to_sdf -> volumeToMesh;
    # it is not, it was the n-gons. Cost: ~1.65x faces (60k -> 100k) + UV0 for free.
    #
    # WHAT IT DOES **NOT** FIX — THE LEDGE SAWTOOTH. Every horizontal ledge grows a row
    # of ~1-voxel triangular teeth. They are REAL GEOMETRY out of the adaptive mesher at
    # the concave crease where an overlapping block buries a face one voxel under the
    # exposed surface, not a render artifact. Probed and RULED OUT this round (identical
    # teeth in every case): tilt_deg 0, adaptivity 0.05 / 0.18 / 0.35, sign_mode 0 vs 1,
    # course_lap 0.12 -> 0.30 / chord_lap 0.10 -> 0.24, and triangulate itself.
    # isovalue -0.035 (shaving the extraction 3.5 cm inside the zero set) visibly softens
    # the coping edge but leaves the lower ledges — hence the knob, defaulted OFF.
    # The only clean lever left is voxel size (sdf_dim), which scales them linearly.
    rm = self.sdf_to_mesh_clean(rd, adaptivity=float(adaptivity), unwrap=bool(triangulate),
                                isovalue=float(isovalue))
    rm = self.select(rm, S.area < 1e-9, domain=POLY, op=replace(group(_B_TAIL)))
    rm = self.delete_faces(rm, slot=_B_TAIL)             # degenerate-sliver insurance

    # ---- PER-BLOCK MATERIAL (the round-2 headline). gids do not survive the remesh, so
    # each block is re-found by the EXACT wedge it was built in (self._blocks). No atan
    # in the predicate: a point is inside the wedge (m, half) iff
    #     dot(P.xz, (cos m, sin m)) >= cos(half) * |P.xz|
    # which is 4 flat ops. Each variant gets ONE select (an OR over its blocks) + one
    # assign_gid; they share `_B_MTL` because `replace` makes the group EXACTLY the
    # matched set, so the bit is clean for the next variant. Shared leaves (the radius,
    # the position components) are the SAME SelExpr objects, so emit_block's identity
    # memo emits each of them once for the whole predicate.
    rm = self._apply_block_gids(rm, n_var)
    if soot and firering and n_curb > 0:
      # ---- SOOT: the hearth floor and the INNER curb faces are fire-blackened. This is
      # the "proximity mask" idiom — a radial band + a height cap round the blaze — and
      # it is what makes the pit read as USED rather than newly built.
      Px, Py, Pz = S.P.x, S.P.y, S.P.z
      rxz = S.sqrt(Px * Px + Pz * Pz)
      band = (rxz < (float(fire_r) + float(soot_r_pad))) \
             & (Py < (float(floor_top) + float(fire_h) + float(soot_h_pad))) \
             & (Py > (y_lo - 0.05))
      rm = self.select(rm, band, domain=POLY, op=replace(group(_B_MTL)))
      rm = self.assign_gid(rm, gid=GID_SOOT, slot=_B_MTL)

    # FLAT-SHADE the blocks. sdf_to_mesh_clean leaves smooth (averaged) normals, which
    # on a curvature-adaptive mesh smears every quarried face into its neighbours — the
    # "melted wax" read of rounds 3-5. Face normals keep each block face flat and let
    # the SDF's own edge rounding show up as a crisp weathered arris instead.
    if hard_shade:
      rm = self.face_normals(rm)

    # O3 STORED-MODE (opt-in): per-section UV unwrap as the LAST op (bld_kiva pattern).
    if sectioned:
      rm = self.section_unwrap(rm, padding=int(section_padding),
                               max_layers=int(section_max_layers))
    self.output(rm)

    # ---- the palette materials() hands back, sized to the variants actually used
    # (A8: every colour/roughness above is a live ctor kwarg, nothing inlined here).
    n_pal = max(1, min(int(n_var), len(stone_albedos), len(stone_roughs)))
    self._palette = dict(stone=[(tuple(stone_albedos[i]), float(stone_roughs[i]))
                                for i in range(n_pal)],
                         anchor=(tuple(anchor_albedo), float(anchor_rough)),
                         soot=(tuple(soot_albedo), float(soot_rough)))

    # ---- PHYSICS PROXY — GENERATING PARAMS ONLY (law). The bld_kiva-compatible
    # single box is a solid drum (y = FULL wall height; centered btBoxShape).
    self.collider_half_extents = (radius + batter_m + jitter_r + 0.35,
                                  anchor_top_y + 0.25,      # the anchors are the tallest thing
                                  radius + batter_m + jitter_r + 0.35)
    # ...and the RING grammar a scene needs for a walkable annulus compound — the
    # same numbers, derived the same way, so a collider built from this can never
    # drift from the mesh. No render-mesh inspection anywhere. ROUND 2 adds the
    # silhouette/entry grammar so a scene's compound proxy (and any spawn/approach
    # logic) can key off the SAME numbers that placed the stones.
    self.ring_proxy = dict(outer_r=max(radius + batter_m + jitter_r,
                                       Rm + anchor_r_off + wall_t * anchor_t_mul * 0.5),
                           inner_r=(radius - wall_t) + batter_m,   # narrowest (battered base)
                           rim_y=y_top, hearth_y=0.0,
                           hearth_depth=hearth_depth, floor_top=floor_top,
                           curb_r=fire_r if firering else 0.0,
                           curb_t=fire_t, curb_y=floor_top + fire_h,
                           anchor_azs=tuple(float(x) for x in (anchor_azs or ())),
                           anchor_top_y=anchor_top_y,
                           entry_az_deg=(float(entry_az_deg) if entry else None),
                           entry_w_deg=(float(entry_w_deg) if entry else 0.0),
                           entry_sill_y=(y_top - float(entry_drop)) if entry else y_top)

  def _apply_block_gids(self, rm, n_var):
    """Re-identify every quarried block AFTER the SDF remesh and gid it by variant.
    ONE select+assign_gid per variant; gid 0 needs neither (it IS the default draw).
    The wedge test is exact against the build's own azimuthal partition — see the
    header's PER-BLOCK MATERIAL note for why this beats a position-hash band."""
    if not self._blocks:
      return rm
    Px, Py, Pz = S.P.x, S.P.y, S.P.z
    rxz = S.sqrt(Px * Px + Pz * Pz)          # shared leaves: emit_block dedups by identity
    by_gid = {}
    for (a, half, y0, y1, r_lo, r_hi, v) in self._blocks:
      by_gid.setdefault(int(v), []).append((a, half, y0, y1, r_lo, r_hi))
    for gid in sorted(by_gid):
      if gid == 0:
        continue                              # gid 0 = the primary draw, nothing to tag
      pred = None
      for (a, half, y0, y1, r_lo, r_hi) in by_gid[gid]:
        ch = math.cos(min(float(half), 1.5533))            # clamp: a ~pi/2 wedge is a half-plane
        wedge = ((Px * math.cos(a) + Pz * math.sin(a)) - rxz * ch) > 0.0
        t = wedge & (Py > float(y0)) & (Py < float(y1)) & (rxz > float(r_lo))
        if float(r_hi) < 90.0:
          t = t & (rxz < float(r_hi))
        pred = t if pred is None else (pred | t)
      rm = self.select(rm, pred, domain=POLY, op=replace(group(_B_MTL)))
      rm = self.assign_gid(rm, gid=int(gid), slot=_B_MTL)
    return rm

  def materials(self):
    """One HmMaterial per gid: the four quarried-stone variants, the anchor megaliths,
    the sooted hearth. A scene may override per gid; these are the standalone preview."""
    pal = self._palette
    out = [HmMaterial(Solid, albedo=vec3(*pal["stone"][0][0]), roughness=pal["stone"][0][1])]
    for gid in range(1, len(pal["stone"])):
      alb, rgh = pal["stone"][gid]
      out.append(HmMaterial(Solid, gid=gid, albedo=vec3(*alb), roughness=rgh))
    out.append(HmMaterial(Solid, gid=GID_ANCHOR, albedo=vec3(*pal["anchor"][0]),
                          roughness=pal["anchor"][1]))
    out.append(HmMaterial(Solid, gid=GID_SOOT, albedo=vec3(*pal["soot"][0]),
                          roughness=pal["soot"][1]))
    return out


__all__ = ["FirePit"]
