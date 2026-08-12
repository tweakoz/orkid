###############################################################################
# bld_pyre — THE FUEL. A built stack of substantial timber for the ceremonial
# hearth of bld_firepit: 5.30 x 5.24 m across the base, 4.58 m to the crown
# (MEASURED OBJ bbox at defaults), burning hard. Beltane / Up Helly Aa / Burning
# Man read: DARK TIMBER SILHOUETTED AGAINST A BRIGHT CORE, with real chimneys for
# the flame to vent through.
#
# THE STACKING GRAMMAR — a HOLLOW CRIB (log-cabin lattice) inside a SKIRT-AND-SPAR
# mantle. Every part of that was chosen against a render, not assumed:
#
#   * The CRIB (square courses, each laid at 90 deg to the one below, the two
#     EDGE BEARERS notched out past the corners) is what makes the stack read as
#     BUILT BY SOMEONE rather than scattered, and it is the only grammar that
#     gives chimneys for free. A pure teepee has one central void and reads as a
#     wigwam; a solid pile has none and reads as a rock.
#   * It is HOLLOW above `crib_hollow_from` — only the two bearers per course,
#     no interior fill. That is how a crib is really built (box first, kindling
#     in the bottom) and it is also the fix for a measured problem: with fill on
#     every course the timber sat directly over the fire axis and DEPTH-TESTED
#     AWAY the brightest 3 m of the flame. See the ctor note on that kwarg for
#     the A/B.
#   * Only the two EDGE logs of a course get the full `crib_over` overhang; the
#     interior fill is set back to `crib_fill_over`. Giving every log the bearer
#     length puts four projecting ends on each face instead of two and PLUGS the
#     horizontal slots between courses — and those slots are the chimneys.
#   * The MANTLE is TWO classes, not one. A single uniform radial teepee of 14
#     leaners buried the crib completely and the whole stack read as a haystack
#     (round-1 render). What real pyres get, and what reads, is a dense SKIRT of
#     short cordwood against the crib's lower flank (it carries the 5 m footprint
#     the flame's 4.7 m plume disc wants) plus a few long SPARS crossed over the
#     crown — with the crib's top third left NAKED, because that is where the
#     dark-bars-against-bright-core silhouette actually happens. The spar heads
#     land at `spar_head_r` on the crib's top corners, NOT on the axis, so the
#     central flue stays clear all the way out.
#
# THE TWO BUDGETS, designed against (both MEASURED, not assumed):
#
#  1. REGISTER POOL — 64 GpuMesh regs. hmdflow.cpp:504 `bakeMesh` creates a FRESH
#     `dgctx` per call and stocks it with createRegisters<GpuMeshData>("hm_mesh",
#     64); materialize_live (hmdflow.cpp:707) does the same per live instance.
#     The pool is therefore PER-GRAPH, NOT GLOBAL — this asset gets its own 64
#     and bld_firepit's 55 do not come out of it. Live regs ~= LEAF BOX count
#     (the sorter schedules the input-free primitives first, so every leaf's
#     output is live at once; the transform/merge chain downstream recycles).
#     MEASURED arithmetic at defaults (self.register_arithmetic, and confirmed by
#     the vet's shell count, which is exactly the leaf count on this crisp path):
#         crib    2 solid courses x 4 + 9 hollow x 2  = 26
#         skirt   12 short leaners                    = 12
#         spars    5 long ones over the crown         =  5
#         bed      6 collapsed billets + 1 ash dome   =  7
#                                                 total 50 of 64
#     `reg_budget` is a live kwarg and the ctor RAISES with the arithmetic rather
#     than let the pool overflow silently. NOTE that going hollow BUYS registers
#     rather than spending them — that is why the crib is 11 courses tall.
#
#  2. SHADER PROGRAMS — the scene this ships into is AT the engine's 255 global
#     program ceiling (vulkan_fxi_DBread.cpp:857), i.e. there is room for ZERO
#     new ptex3d materials. So this asset declares TWO gids and no more, and both
#     are chosen to land on materials scn_swest_fire ALREADY declares:
#         gid 0 GID_TIMBER — fresh cut timber      -> the vale's `sw_timber`
#         gid 1 GID_CHAR   — burnt-through / char  -> the vale's `sw_door`
#                            (Timber at early 0.24/0.17/0.11, silver 0.12 —
#                             a dark scorched wood by declaration already)
#     Nothing here needs a material the vale has not already paid for.
#
# MESH ORIGIN CONTRACT: y = 0 is the HEARTH FLOOR PLANE — bld_firepit's own
# datum — so this asset rides the SAME instance matrix as the pit (the
# `firepit` scatter sink, type 0) with no lift and no second placement. The
# stack sits ON the hearth SLAB, whose surface is `hearth_top` (= bld_firepit's
# floor_top, 0.15). Nothing here is authored in world space.
#
# SIZED TO THE FLAME, not to the pit: bonfire's plume rings run to 2.35 m (a
# 4.7 m base disc) and the core releases at y ~ 0.85. `lean_r` 2.45 puts the
# skirt butts at a ~5.2 m footprint, so the flame's base and the stack's base are
# the same object; the crib top at ~3.4 m means the release point is INSIDE the
# stack and the column is BORN IN THE CRIB and vents out through it, which is the
# whole reason the fuel exists. The butts sit just inside the pit's fire curb
# (inner radius 2.66 m) — leaning on the curb is how you would actually stack it.
#
# WHAT THE FUEL COSTS THE FLAME, measured, because it is a real trade and the
# owner should see the number rather than discover it: with SWEST_FIRE_PYRE=0 the
# column is a clean ~10 m plume off a bare slab; with the pyre in, the visible
# column is roughly two thirds of that, because the timber occludes sprites that
# spawn behind it. The hollow crib recovered most of what a solid one took. If
# more is wanted the lever is on the FIRE side (raise the plume release toward the
# crib top, or narrow the plume base disc so it comes up the flue) — not here.
#
# NO SDF BY DEFAULT, and that is a look decision. bld_firepit voxelizes because
# it wants WEATHERED stone — every arris rounded. Cut timber is the opposite: it
# wants crisp split faces and sharp sawn ends, and the SDF round trip would also
# (a) close the chimneys by 2x the extraction offset, (b) throw away the per-log
# gids, and (c) cost a 24 M-voxel bake per iteration. Merged boxes with a random
# ROLL about each log's own length axis read as SPLIT CORDWOOD, which is what a
# ceremonial pyre is actually built of. `sdf_round` > 0 opts into the rounded
# path (rounded-box-by-positive-isovalue: shrink every box by r, extract the
# redistanced field at +r, and every arris comes back with an r fillet) for a
# whole-round-timber look; the logs are pre-shrunk so the DIMENSIONS do not move.
#
# PHYSICS-PROXY (law): `collider_half_extents` / `pyre_proxy` derive from the
# GENERATING params — footprint radius, crown height, bed radius — never from the
# render mesh. The ember bed and the mantle are in the proxy because the same
# params place them; nothing is measured off a vertex.
#
# VET: allow_self_intersect — logs REST ON each other, so they interpenetrate by
# construction (the same contact law bld_kiva/bld_firepit run under).
#
#   ork.hypermesh.viewer.py bld_pyre --msaa 2
#   _ork.hypermesh.validate.py bld_pyre -o /tmp/bld_pyre.obj
###############################################################################
import math
import random
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, HmMaterial, S, replace, group, POLY
from ork.hypergraph.assets.materials.terrain.solid import Solid

_B_MTL = 5                        # scratch tag bit the post-remesh char select reuses

GID_TIMBER = 0                    # fresh cut timber  (scene: sw_timber)
GID_CHAR   = 1                    # burnt-through/charred + the ember bed (scene: sw_door)


def _log(hm, size, butt, head, roll_deg=0.0, yaw_skew_deg=0.0):
  """Place ONE timber of cross-section (size[1] x size[2]) running from world
  point `butt` to world point `head`, rolled `roll_deg` about its OWN length
  axis. size[0] is ignored — the length comes from |head - butt|.

  TWO TRANSFORMS, NOT ONE, and this is load-bearing (the trap bld_kiva shipped
  and bld_firepit documents): `Hypermesh.transform` composes its convenience
  args as Tt * Tp * mtx4.composed(0, q, scale) * Tpi, and mtx4.composed builds
  T*S*R — the NON-UNIFORM SCALE IS APPLIED IN WORLD AXES *AFTER* THE ROTATION.
  A one-shot transform(scale=, rotate=) therefore yields an AXIS-ALIGNED box
  whose extents are the rotated unit box's AABB times the scale, and the
  rotation's SIGN does not even matter (measured: a 3.0x0.4x0.3 box "yawed" 135
  deg comes out 4.24 m in X = 3*sqrt2). A log stack is nothing but rotated
  boxes, so: SCALE FIRST (rotation identity), THEN a PURE RIGID rotate+translate.

  ORIENTATION. The box's local +X is its length. `rotate=` composes
  LEFT-TO-RIGHT (q = q0*q1*q2) and v_world = q * v_local, so listing
  [(Y,yaw), (Z,pitch), (X,roll)] applies roll in the log's own frame, then
  pitches it in the vertical plane, then swings that plane to the azimuth.
  orkid's axis-angle rotations are right-handed: +X -> (cos t, 0, -sin t) about
  Y and +X -> (cos t, sin t, 0) about Z, so the yaw that lays +X along a
  horizontal direction (hx, hz) is atan2(-hz, hx) and a positive Z-pitch raises
  the +X (head) end. (Cross-check: for a ring tangent at azimuth a the horizontal
  is (-sin a, cos a) and this gives -(a + 90 deg) — bld_firepit's _tangent_yaw.)"""
  d = vec3(head[0] - butt[0], head[1] - butt[1], head[2] - butt[2])
  L = math.sqrt(d.x * d.x + d.y * d.y + d.z * d.z)
  if L < 1e-4:
    raise ValueError("bld_pyre: degenerate log (butt == head)")
  hxz = math.sqrt(d.x * d.x + d.z * d.z)
  pitch = math.degrees(math.atan2(d.y, hxz))
  yaw   = (math.degrees(math.atan2(-d.z, d.x)) if hxz > 1e-6 else 0.0) + float(yaw_skew_deg)
  b = hm.box(size=0.5)
  b = hm.transform(b, scale=(L, float(size[1]), float(size[2])))       # world-axis scale, NO rotation
  return hm.transform(b,                                              # pure rigid from here
                      rotate=[(vec3(0, 1, 0), yaw),
                              (vec3(0, 0, 1), pitch),
                              (vec3(1, 0, 0), float(roll_deg))],
                      translate=(0.5 * (butt[0] + head[0]),
                                 0.5 * (butt[1] + head[1]),
                                 0.5 * (butt[2] + head[2])))


class Pyre(Hypermesh):
  VET = dict(allow_self_intersect=True)

  def __init__(self, seed=19, *,
               # ---- datum ------------------------------------------------------
               hearth_top=0.15,       # bld_firepit floor_top: the SLAB surface the
                                      # stack is built on, in the pit's own frame
                                      # (y=0 = hearth floor plane). Pass the pit's
                                      # value; never restate it as a literal.
               # ---- the crib core (log-cabin lattice) ---------------------------
               crib_w=3.35,           # span between the OUTERMOST log axes, base course
               crib_courses=11,        # alternating courses (register budget lives here)
               crib_hollow_from=2,    # THE CENTRAL FLUE. Above this course index only the
                                      # TWO EDGE BEARERS are laid and the interior fill is
                                      # omitted, so the crib becomes a hollow square frame
                                      # with a clear shaft straight up its axis.
                                      # MEASURED, and this is the round-4 headline: with
                                      # fill logs on every course the stack OCCLUDED the
                                      # brightest 3 m of the flame — the A/B against
                                      # SWEST_FIRE_PYRE=0 showed a 10 m column collapse to
                                      # about half its height, because bonfire's plume rings
                                      # (0.6/1.5/2.35 m) release at y~0.85, i.e. INSIDE the
                                      # crib, and opaque timber depth-tests the sprites away.
                                      # A real crib IS hollow — you build the box and fill
                                      # only the bottom with kindling — so the fix is the
                                      # honest construction, and it BUYS registers instead
                                      # of spending them. Set >= crib_courses for a solid
                                      # stack (and expect the column back at half height).
               crib_per_course=4,     # parallel logs per course -> the chimney pitch:
                                      # 3.35/(4-1) = 1.12 m pitch, 0.80 m clear flue.
                                      # DELIBERATELY WIDE: these logs are the BARS the
                                      # flame is silhouetted against, and bars need gaps
                                      # between them or the stack reads as a solid mound.
               crib_taper=0.22,       # fraction the span narrows from base to crown —
                                      # a stepped pyramid, so the courses read as courses
               crib_over=0.48,        # m the two EDGE BEARERS of a course overhang the
                                      # course beneath (the log-cabin corner — the single
                                      # detail that most says "built", not "piled")
               crib_fill_over=0.05,   # ...and how far the INTERIOR FILL logs overhang.
                                      # KEEP THIS SMALL. The gap between the projecting
                                      # bearer ends and the set-back fill ends is what
                                      # leaves the horizontal SLOTS between courses open,
                                      # and those slots are the chimneys the flame vents
                                      # through. crib_fill_over == crib_over plugs them
                                      # and the stack goes back to reading as a woodpile.
               crib_settle=0.95,      # course pitch as a fraction of log diameter (the
                                      # logs bed into each other; 1.0 = a floating stack)
               # ---- the mantle: a low SKIRT + a few tall SPARS ------------------
               # ROUND 1 TAUGHT THIS SPLIT. One uniform radial teepee of 14 leaners
               # BURIED the crib completely — the render read as a haystack with sticks
               # in it, no built structure anywhere, no legible flue. The way a real
               # ceremonial pyre is stacked (and photographs) is two distinct classes:
               # short cordwood leaned DENSELY against the crib's lower half, and a few
               # long spars crossed OVER the crown. The crib's upper courses are then
               # NAKED against the sky, which is exactly where the dark-bars-against-
               # bright-core silhouette has to live.
               lean_n=12,             # SKIRT: short cordwood against the crib's flank
               lean_r=2.45,           # butt radius -> a ~5.1 m footprint (the flame's
                                      # plume disc is 4.7 m: same object)
               lean_head_r=1.18,      # radius the skirt heads rest at (on the crib face)
               lean_head_y=0.46,      # skirt head height as a FRACTION of the crib top
               spar_n=5,              # SPARS: long timbers crossed over the crown
               spar_r=2.30,           # their butt radius
               spar_head_r=1.05,      # radius the spar heads cross at
               spar_over=0.62,        # m the spar heads stand proud of the crib top
               lean_r_jit=0.20,       # butt radius jitter (hand-stacked, not machined)
               lean_az_jit=0.40,      # butt azimuth jitter, as a fraction of the pitch
               lean_head_jit=0.26,    # head scatter (m) — the crown is a tangle, not a cone
               lean_skew=4.0,         # deg of off-plane yaw skew per leaner
               # ---- the ember bed ----------------------------------------------
               bed=True,
               bed_r=1.85,            # ash/coal dome radius
               bed_h=0.16,            # dome height above the slab (LOW: round 1's 0.30
                                      # over 2.30 read as a mound the stack sat on top of)
               bed_n=6,               # collapsed billets lying in the bed
               bed_len=(0.70, 1.45),  # their length range (m) — burnt-through stubs
               bed_seg=14, bed_rings=6,   # ash-dome tessellation (chunky on purpose)
               # ---- the timber itself ------------------------------------------
               log_d=0.31,            # mean diameter (m). Structural bonfire timber
                                      # is 0.15-0.35; the spread below stays inside it.
               log_d_var=0.22,        # +/- fraction on the diameter
               log_len_var=0.07,      # +/- fraction on each log's length
               log_pos_jit=0.05,      # m of lateral/vertical slop per log
               log_yaw_jit=2.2,       # deg of yaw wobble per crib log
               log_roll=True,         # RANDOM roll about each log's own length axis:
                                      # a square section rolled to an arbitrary angle
                                      # reads as SPLIT CORDWOOD, and it costs nothing.
                                      # This is the whole reason boxes are enough here.
               jitter=1.0,            # global scale on every wobble above
               # ---- charring (the dark half of the silhouette) -------------------
               char=True,
               char_courses=2,        # crib courses gid'd CHAR from the bottom up
               char_scatter=0.28,     # probability a log ABOVE that line is char too
                                      # (a hard char line reads as a paint stripe)
               char_y=0.0,            # >0 OVERRIDES the per-log rule with a FIELD rule
                                      # (needed by the sdf_round path, which cannot
                                      # keep per-log gids through the remesh); 0 =
                                      # derive it from char_courses.
               char_r=0.95,           # field rule: the inner column is char this high
               char_y2=1.90,          #             ...up to here
               # ---- solve -------------------------------------------------------
               sdf_round=0.0,         # >0: whole-round-timber path. Every box is built
                                      # 2r THINNER and the redistanced field extracted
                                      # at +r, so each arris comes back with an r fillet
                                      # and the finished dimensions are unchanged.
                                      # Costs the voxel bake and flattens gids to the
                                      # FIELD char rule. 0 = crisp split timber.
               sdf_dim=256,
               sdf_margin=1.20,       # ext = 2*(lean_r + log_d) + this (CUBIC brick —
                                      # anything outside half-ext is SILENTLY CLIPPED)
               sign_mode=1,
               adaptivity=0.14,
               triangulate=True,      # xatlas: the fanfold vet PASS + UV0 (bld_firepit note)
               reg_budget=64,         # hm_mesh pool, PER GRAPH (hmdflow.cpp:504)
               hard_shade=True,       # face normals — flat split faces, not melted wax
               # ---- standalone preview palette (A8: live, never inlined) ---------
               timber_albedo=(0.36, 0.27, 0.19), timber_rough=0.88,
               char_albedo=(0.075, 0.062, 0.056), char_rough=0.94,
               sectioned=False, section_padding=4, section_max_layers=8,
               **_overrides):
    super().__init__()
    rng = random.Random(int(seed))

    J        = float(jitter)
    y0       = float(hearth_top)                    # the slab surface: everything sits on it
    n_course = max(1, int(crib_courses))
    n_per    = max(2, int(crib_per_course))
    n_lean   = max(0, int(lean_n))
    n_spar   = max(0, int(spar_n))
    n_bed    = (max(0, int(bed_n)) if bed else 0)
    n_dome   = (1 if (bed and float(bed_r) > 0.0 and float(bed_h) > 0.0) else 0)
    r        = max(0.0, float(sdf_round))
    shrink   = 2.0 * r                              # rounded-box pre-shrink (see sdf_round)

    # ---- REGISTER BUDGET: count before we build; fail loudly rather than corrupt.
    n_solid  = max(0, min(int(crib_hollow_from), n_course))    # courses that keep their fill
    n_crib   = n_solid * n_per + (n_course - n_solid) * 2      # the rest are 2-bearer frames
    n_regs = n_crib + n_lean + n_spar + n_bed + n_dome
    self.register_arithmetic = dict(crib=n_crib, courses=n_course, per_course=n_per,
                                    solid_courses=n_solid, hollow_courses=n_course - n_solid,
                                    skirt=n_lean, spars=n_spar, bed_billets=n_bed,
                                    ash_dome=n_dome, total=n_regs, budget=int(reg_budget))
    if n_regs > int(reg_budget):
      raise ValueError(
        "bld_pyre: %d leaf boxes exceeds the %d-register hm_mesh pool (crib %d over %d "
        "courses + skirt %d + spars %d + bed %d + dome %d). Drop a course, lower "
        "crib_per_course/crib_hollow_from, or thin the mantle."
        % (n_regs, int(reg_budget), n_crib, n_course, n_lean, n_spar, n_bed, n_dome))

    # ---- THE CHAR RULE. Two forms of the same intent (the fire burns from the
    # bottom and from the inside out), because the two solve paths can carry
    # different amounts of information:
    #   PER-LOG   (crisp path) — a log is char if its course is below char_courses,
    #             or if it draws char_scatter. The scatter is what keeps the char
    #             line from reading as a painted stripe across the stack.
    #   FIELD     (sdf_round path) — gids do not survive sdf_to_mesh_clean, so the
    #             char is re-selected AFTER the remesh from position alone: low, or
    #             inner-and-lowish. Same intent, no per-log grain.
    self.char_field_y = (float(char_y) if float(char_y) > 0.0
                         else y0 + float(char_courses) * float(log_d) * 1.05)

    def _diam():
      return max(0.10, float(log_d) * (1.0 + rng.uniform(-1.0, 1.0) * float(log_d_var) * J))

    def _sec(d):
      """(unused_len, thickness, thickness) with the rounded-box pre-shrink applied."""
      return (0.0, max(0.02, d - shrink), max(0.02, d - shrink))

    def _roll():
      return rng.uniform(0.0, 90.0) if log_roll else 0.0

    # TWO FOLDS, not one. Every log lands in either the TIMBER tree or the CHAR
    # tree, and the two are merged ONCE at the end with gid_a/gid_b — so the
    # per-log gid is carried by the merge itself (no selects, no predicates, no
    # post-hoc masking) and it costs zero extra LEAF registers, because merge
    # nodes are interior and their inputs die immediately.
    trees = {False: None, True: None}
    counts = {False: 0, True: 0}

    def _emit(node, is_char):
      k = bool(char) and bool(is_char)
      trees[k] = node if trees[k] is None else self.merge(trees[k], node,
                                                          gid_a=None, gid_b=None)
      counts[k] += 1

    # =========================================================================
    # 1. THE CRIB — a log-cabin lattice. Course c runs along X when c is even and
    # along Z when c is odd; the logs of a course are spread evenly across the
    # perpendicular span so the CLEAR GAP BETWEEN THEM IS A CONTINUOUS FLUE from
    # the bed to the crown. That flue is the whole point: a bonfire finds the
    # chimneys between the logs, and a stack with no holes reads as a rock.
    #
    # Each course's logs are LONGER than the span by 2*crib_over so their ends
    # project past the course beneath — the log-cabin corner, and the single
    # detail that most says "someone built this". They also DROP into the course
    # below by a fraction of a diameter (contact law: a literal air gap between
    # courses reads as a floating stack, and the render's own shadowing needs the
    # logs to touch).
    # =========================================================================
    cy       = y0
    prev_spn = float(crib_w)
    crib_top = y0
    for c in range(n_course):
      t    = c / max(1, n_course - 1)                          # 0 base .. 1 crown
      spn  = float(crib_w) * (1.0 - float(crib_taper) * t)     # this course's span
      d_c  = _diam()
      cy  += d_c * (1.0 if c == 0 else float(crib_settle))     # the logs bed into each other
      along_x = (c % 2 == 0)
      # HOLLOW ABOVE crib_hollow_from: only the two edge bearers, so the axis is a
      # clear shaft for the flame to come up (see the ctor note — this was measured
      # against the fire, not guessed).
      idxs = range(n_per) if c < n_solid else (0, n_per - 1)
      for i in idxs:
        u  = (-0.5 + i / float(n_per - 1)) * spn               # position across the course
        u += rng.uniform(-1.0, 1.0) * float(log_pos_jit) * J
        d  = _diam()
        # ---- ONLY THE TWO EDGE LOGS MAKE THE CORNER. Round 3's fix, and it is
        # the difference between a crib and a woodpile: a real log-cabin course
        # is two long BEARERS notched over the course below (ends projecting) plus
        # shorter FILL logs cut to land on those bearers and no further. Giving
        # every log the full bearer length — which is what this loop did — puts
        # 4 projecting ends on every face instead of 2, and those ends PLUG THE
        # HORIZONTAL SLOTS between the broadside courses. Those slots ARE the
        # chimneys the brief asks the flame to vent through: looking at a crib
        # face you see courses c, c+2, c+4 broadside with a clear log-diameter
        # gap between them, and you see the fire through that gap. Setting the
        # fill logs back by (crib_over - crib_fill_over) re-opens them.
        edge = (i == 0 or i == n_per - 1)
        over = float(crib_over) if edge else float(crib_fill_over)
        L    = prev_spn + 2.0 * over                           # reach past the course below
        Lg = L * (1.0 + rng.uniform(-1.0, 1.0) * float(log_len_var) * J)
        sh = rng.uniform(-1.0, 1.0) * over * 0.45 * J          # slide along its own axis
        ly = cy + rng.uniform(-0.35, 0.35) * float(log_pos_jit) * J
        if along_x:
          butt, head = (-0.5 * Lg + sh, ly, u), (0.5 * Lg + sh, ly, u)
        else:
          butt, head = (u, ly, -0.5 * Lg + sh), (u, ly, 0.5 * Lg + sh)
        is_char = (c < int(char_courses)) or (rng.random() < float(char_scatter))
        _emit(_log(self, _sec(d), butt, head, roll_deg=_roll(),
                   yaw_skew_deg=rng.uniform(-1.0, 1.0) * float(log_yaw_jit) * J),
              char and is_char)
      crib_top = cy + d_c * 0.5
      prev_spn = spn

    # =========================================================================
    # 2. THE MANTLE — TWO CLASSES, and the split is round 1's lesson (see the
    # ctor block): a dense SKIRT of short cordwood leaned against the crib's lower
    # flank, and a handful of long SPARS crossed over the crown. One uniform
    # radial teepee buries the crib and the whole stack reads as a haystack; this
    # way the skirt carries the 5 m footprint the flame's plume disc wants, the
    # spars put a few steep dark diagonals across the column, and the crib's top
    # third stays NAKED — which is where the dark-bars-against-bright-core read
    # actually happens.
    #
    # Every leaner is placed by the SAME butt/head chain as the crib, so the
    # scale-then-rigid-rotate discipline holds for the pitched ones too.
    # =========================================================================
    head_y = crib_top + float(spar_over)

    def _mantle(n, r_butt, r_head, y_head, char_p):
      pitch_az = (2.0 * math.pi / n) if n else 0.0
      for k in range(n):
        a  = k * pitch_az + rng.uniform(-1.0, 1.0) * pitch_az * float(lean_az_jit) * J
        rb = float(r_butt) + rng.uniform(-1.0, 1.0) * float(lean_r_jit) * J
        d  = _diam()
        by = y0 + d * 0.5 + rng.uniform(0.0, 0.10) * J
        # the head lands NEAR its nominal spot, not ON it — a stack of leaners that
        # all converge exactly reads as a machined wigwam frame.
        ha = a + rng.uniform(-0.30, 0.30) * J
        hr = max(0.05, float(r_head) + rng.uniform(-1.0, 1.0) * float(lean_head_jit) * J)
        hy = float(y_head) + rng.uniform(-1.0, 1.0) * float(lean_head_jit) * J
        _emit(_log(self, _sec(d),
                   (rb * math.cos(a), by, rb * math.sin(a)),
                   (hr * math.cos(ha), hy, hr * math.sin(ha)),
                   roll_deg=_roll(),
                   yaw_skew_deg=rng.uniform(-1.0, 1.0) * float(lean_skew) * J),
              char and (rng.random() < float(char_p)))

    # SKIRT — heads on the crib's flank at lean_head_y of its height, so the crib
    # rises out of it instead of disappearing into it.
    _mantle(n_lean, lean_r, lean_head_r,
            y0 + float(lean_head_y) * (crib_top - y0), float(char_scatter) * 1.4)
    # SPARS — the few long ones, crossed above the crown.
    _mantle(n_spar, spar_r, spar_head_r, head_y, float(char_scatter) * 0.4)

    # =========================================================================
    # 3. THE EMBER BED — the collapsed, burnt-through material the stack stands
    # in. A low ash/coal DOME (one squashed uvsphere, deliberately coarse so it
    # reads as clinker rather than as a balloon) with a handful of stub billets
    # lying half-buried in it. All of it is gid CHAR: at night this is the
    # near-black bed the fire's own near-field lights rake across, and it is what
    # stops the stack from looking like it was set down on a clean floor.
    # =========================================================================
    if n_dome:
      dome = self.uvsphere(radius=1.0, segments=int(bed_seg), rings=int(bed_rings))
      # no rotation here, so ONE transform is safe (the T*S*R trap only bites when
      # a non-uniform scale and a rotation are asked for in the same call).
      dome = self.transform(dome, scale=(float(bed_r), float(bed_h), float(bed_r)),
                            translate=(0.0, y0, 0.0))          # equator ON the slab
      _emit(dome, True)
    for k in range(n_bed):
      bl = rng.uniform(float(bed_len[0]), float(bed_len[1]))
      ba = rng.uniform(0.0, 2.0 * math.pi)
      br = rng.uniform(0.35, max(0.4, float(bed_r) * 0.92))
      d  = _diam() * rng.uniform(0.85, 1.15)
      # sunk into the dome: the bed is what is LEFT of burnt timber, not timber
      # resting on top of ash.
      cyb = y0 + float(bed_h) * max(0.0, 1.0 - (br / max(0.1, float(bed_r))) ** 2) * 0.75
      ya  = rng.uniform(0.0, 2.0 * math.pi)
      dx, dz = math.cos(ya) * bl * 0.5, math.sin(ya) * bl * 0.5
      tilt = rng.uniform(-0.10, 0.10) * bl
      cx, cz = br * math.cos(ba), br * math.sin(ba)
      _emit(_log(self, _sec(d), (cx - dx, cyb - tilt, cz - dz), (cx + dx, cyb + tilt, cz + dz),
                 roll_deg=_roll()),
            True)

    # =========================================================================
    # 4. SOLVE + GIDS.
    # =========================================================================
    self.gid_counts = dict(timber=counts[False], char=counts[True])
    if r > 0.0:
      # the SDF path needs ONE input mesh; the gids are re-derived from the field
      # rule below, so the two folds can be joined with their gids discarded.
      stack = trees[False] if trees[True] is None else (
              trees[True] if trees[False] is None else
              self.merge(trees[False], trees[True], gid_a=None, gid_b=None))
      # ---- WHOLE-ROUND-TIMBER PATH. The standard rounded-box construction: build
      # each box 2r thinner, redistance, and extract the isosurface at +r — the
      # positive offset restores the r it lost on every face and fillets every
      # arris with radius r, so a 0.29 m square section comes back as a 0.29 m
      # section that is very nearly a cylinder. WATCH THE BRICK: it is CUBIC and
      # clips silently (symptom: a bbox reading EXACTLY the extent).
      ext = 2.0 * (float(lean_r) + float(log_d)) + float(sdf_margin)
      self.voxel_m = ext / float(sdf_dim)
      sd = self.mesh_to_sdf(stack, dim=int(sdf_dim), extent=ext,
                            center=(0.0, crib_top * 0.5, 0.0), sign_mode=int(sign_mode))
      rd = self.redistance(sd)
      out = self.sdf_to_mesh_clean(rd, adaptivity=float(adaptivity),
                                   unwrap=bool(triangulate), isovalue=r)
      if char:
        # gids do not survive the remesh -> the FIELD char rule (see char_field_y).
        Px, Py, Pz = S.P.x, S.P.y, S.P.z
        rxz  = S.sqrt(Px * Px + Pz * Pz)
        mask = (Py < self.char_field_y) | ((rxz < float(char_r)) & (Py < float(char_y2)))
        out  = self.select(out, mask, domain=POLY, op=replace(group(_B_MTL)))
        out  = self.assign_gid(out, gid=GID_CHAR, slot=_B_MTL)
    else:
      # ---- CRISP SPLIT-TIMBER PATH (default). No voxels: the merge tree IS the
      # mesh, so the chimneys stay exactly the width they were built at, the sawn
      # ends stay sharp, and — the reason this path exists at all — the PER-LOG
      # gids survive, because merge stamps them: the two folds join here, one gid
      # per side. No selects, no predicates, no extra leaf registers.
      if trees[True] is None:
        out = self.assign_gid(trees[False], gid=GID_TIMBER)
      elif trees[False] is None:
        out = self.assign_gid(trees[True], gid=GID_CHAR)
      else:
        out = self.merge(trees[False], trees[True], gid_a=GID_TIMBER, gid_b=GID_CHAR)
    if hard_shade:
      out = self.face_normals(out)
    if sectioned:
      out = self.section_unwrap(out, padding=int(section_padding),
                                max_layers=int(section_max_layers))
    self.output(out)

    self._palette = dict(timber=(tuple(timber_albedo), float(timber_rough)),
                         char=(tuple(char_albedo), float(char_rough)))

    # ---- PHYSICS PROXY — GENERATING PARAMS ONLY (law). Nothing below reads a
    # vertex: the footprint is lean_r + a log radius, the crown is the crib top
    # plus the mantle overshoot, the bed is its own authored radius.
    mantle_r = max(float(lean_r) if n_lean else 0.0, float(spar_r) if n_spar else 0.0)
    foot_r = (mantle_r + float(lean_r_jit) + float(log_d) * 0.5) if mantle_r > 0.0 else \
             (0.5 * float(crib_w) + float(crib_over) + float(log_d) * 0.5)
    top_y  = (max(crib_top, head_y) if n_spar else crib_top) \
             + float(lean_head_jit) + float(log_d) * 0.5
    self.collider_half_extents = (foot_r, 0.5 * (top_y - y0), foot_r)
    self.collider_center_y     = 0.5 * (top_y + y0)
    self.pyre_proxy = dict(hearth_top=y0,
                           foot_r=foot_r, top_y=top_y,
                           crib_w=float(crib_w), crib_top=crib_top,
                           crib_courses=n_course, crib_per_course=n_per,
                           flue_clear=(float(crib_w) / max(1, n_per - 1)) - float(log_d),
                           lean_n=n_lean, lean_r=float(lean_r),
                           spar_n=n_spar, spar_r=float(spar_r),
                           bed_r=(float(bed_r) if n_dome else 0.0),
                           bed_top=y0 + (float(bed_h) if n_dome else 0.0),
                           char_field_y=self.char_field_y)

  # -------------------------------------------------------------------------
  def materials(self):
    """STANDALONE PREVIEW ONLY — one HmMaterial per gid. In scn_swest_fire these
    two gids bind to materials the vale ALREADY declares (sw_timber / sw_door),
    because that scene has zero shader-program headroom for new ones; these are
    what the hypermesh viewer/validator draws with."""
    pal = self._palette
    return [HmMaterial(Solid, albedo=vec3(*pal["timber"][0]), roughness=pal["timber"][1]),
            HmMaterial(Solid, gid=GID_CHAR, albedo=vec3(*pal["char"][0]),
                       roughness=pal["char"][1])]


__all__ = ["Pyre", "GID_TIMBER", "GID_CHAR"]
