###############################################################################
# _pueblo — ancient-Southwest PUEBLO generator library (scn_swest / P2): the
# Taos / Chaco / Acoma vocabulary as a parameterized recipe —
#
#   AGGREGATED STEPPED MASSING  contiguous cellular room-blocks; upper storeys
#                               SET BACK so each roof is the next level's terrace
#   FLAT ROOFS                  parapet rings + VIGAS (projecting roof-beam ends)
#   ADOBE                       thick battered walls, ROUNDED ERODED corners,
#                               basal flare, earthen surface (ground palette)
#   OPENINGS not windows        small / sparse / irregular, NO glass; timber
#                               lintels; the Chaco T-SHAPED DOORWAY as signature
#
# TECHNIQUE (named per the tech-artist contract): CityEngine/CGA-style mass
# modeling (box massing + boolean opening carve) fused with the VDB idiom for
# organic edges — voxelize -> CSG -> redistance -> ADAPTIVE volumeToMesh
# (sdf_to_mesh_clean, O2 adoption 07-22: openvdb curvature-adaptive extraction
# replaces the marching-tets + Taubin-fairing triple — flat adobe walls become
# few big quads, detail survives at the eroded corners; ~40x fewer tris). The
# carve machinery is inherited from _building.py (GR-B) with ONE deviation:
# the WHOLE massing goes through the SDF (solid, no interior), because rounding
# the massing itself IS the look. _building's shell trick existed to keep a
# crisp massing out of the SDF — a pueblo wants the opposite.
#
# Inherited banked gotchas (violations have bitten before — see _building.py):
#   * ZERO SdfEval — cutters are MESH boxes voxelized by mesh_to_sdf (the
#     shadlang backtracking parser is EXPONENTIAL in SdfExpr nesting).
#   * mesh_to_sdf: EXPLICIT extent= always; ONE shared framing for every brick.
#   * sign_mode=1 (winding) — the massing/cutter meshes are UNIONS of
#     overlapping closed boxes; pseudonormal sign checkerboards on buried faces.
#   * merge the remeshed output ONCE, LAST (capacity-count concat).
#   * gid does NOT survive the SDF round trip -> decks re-assign_gid'd after.
#   * capacity-tail drop (zero-area select -> delete) before CPU consumers.
#
# HARD NUMBERS (human-metric law, measured via OBJ bbox before look iteration):
#   storey 2.4-2.6 m | T-door ~1.5 m tall (bottom ~0.6 w, head ~0.9 w) |
#   parapet 0.3-0.5 m above deck | room module 3-5 m/side | block 8-20 m |
#   reveals 0.4-0.6 m deep (thick walls) | nothing over ~8 m tall
#
# GID MAP (A1 law — GidAssign is the ONLY writer; scenes bind per gid):
#   0  WALL  adobe (the default; the carved massing re-reads as wall)
#   1  —     RETIRED (was GLASS; pueblo openings are dark recesses, no glass)
#   2  DOOR  timber — door slabs + the dark recess plates behind every opening
#   3  ROOF  adobe (earthen roof decks + terraces; same palette as wall)
#   4  TRIM  timber — vigas, lintels, ladders
#
# Variants: bld_pueblo_row (1 storey) / bld_pueblo_stepped (2 storeys, terrace)
# / bld_kiva (standalone, not from this base).
###############################################################################
import math
import random
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import (Hypermesh, HmMaterial, S,
                                            replace, group, POLY)
from ork.hypergraph.assets.materials.terrain.solid import Solid

GID_WALL, GID_DOOR, GID_ROOF, GID_TRIM = 0, 2, 3, 4

# __tags scratch bits (free region [0:20); gid rides the locked [20:32) band)
_B_DECK1, _B_DECK2, _B_TAIL = 1, 2, 6


class Pueblo(Hypermesh):
  """One pueblo room-block. `rooms` sets the cellular module count (3-5 m each);
  `storeys` 1 or 2 — 2 adds a set-back upper block whose absent footprint is the
  terrace. `seed` jitters proportions + opening placement (style variation, no
  topology change). The result is ONE mesh, four gid material buckets."""

  VET = dict(allow_self_intersect=True)   # kit-bash: vigas/lintels/plates EMBED by design

  def __init__(self, storeys=1, rooms=4, *,
               room_w=3.6, depth=5.2, storey_h=2.5,
               parapet_h=0.4, parapet_t=0.35,
               setback=2.2, upper_rooms=None,
               door_h=1.5, door_w_lo=0.6, door_w_hi=0.9, door_head=0.55,
               n_doors=None, win_w=0.5, win_h=0.55, win_keep=0.55,
               reveal_d=0.5, door_reveal=0.8,
               viga_d=0.15, viga_out=0.35, viga_spacing=0.8,
               flare_m=0.35, flare_h=0.9, skirt_m=0.5,
               ladder=True,
               sdf_dim=192, adaptivity=0.7, smooth_k=0.18,
               sectioned=False, section_padding=4, section_max_layers=8,
               seed=0):
    super().__init__()
    rng      = random.Random(int(seed))
    storey_h = storey_h * rng.uniform(0.98, 1.02)      # proportion jitter ONLY
    W, D     = rooms * room_w, depth
    deck1    = storey_h                                 # storey-1 roof/terrace deck
    H1       = deck1 + parapet_h                        # lower box outer top
    stepped  = int(storeys) >= 2
    if stepped:
      ur   = min(int(upper_rooms) if upper_rooms is not None else max(1, rooms - 1), rooms)
      Wu   = ur * room_w
      side = (rng.choice((-1.0, 1.0)) if ur < rooms else 0.0)   # which end the upper block hugs
      xu   = side * (W - Wu) * 0.5
      D2   = max(2.4, D - setback)                     # upper depth; >= a livable room
      zu_lo, zu_hi = -D * 0.5, -D * 0.5 + D2           # back-aligned -> terrace at FRONT (+Z)
      zu    = 0.5 * (zu_lo + zu_hi)
      deck2 = deck1 + storey_h
      H2    = deck2 + parapet_h
    Htop = H2 if stepped else H1

    cols  = [(c - (rooms - 1) * 0.5) * room_w for c in range(rooms)]
    nd    = int(n_doors) if n_doors is not None else (2 if rooms >= 4 else 1)
    door_bays = sorted(rng.sample(range(rooms), min(nd, rooms)))

    # ---- massing: solid boxes (lower block + basal flare [+ upper block]).
    # The flare is the adobe move — walls thicken into the ground; it also carries
    # the BELOW-GRADE SKIRT (terrain bedding on slopes is the massing's job, never
    # a scatter lift — the _building plinth lesson, owner-caught 2026-07-22).
    lo = self.box(size=0.5)
    lo = self.transform(lo, scale=(W, H1, D), translate=(0, H1 * 0.5, 0))
    # B3 ADOBE MELT (07-22): the basal flare is a 3-step TALUS stack — widest at
    # grade, stepping in as it rises; the SDF voxel rounding melts the steps into
    # a proper batter (walls visibly THICKEN into the ground, the Taos/Acoma
    # read). The widest step carries the BELOW-GRADE SKIRT (terrain bedding on
    # slopes stays the massing's job, never a scatter lift). Overlapping solids
    # -> winding sign (banked gotcha).
    lower = lo
    for fo, ft in ((1.0, 0.45), (0.60, 0.75), (0.28, 1.0)):
      fl = self.box(size=0.5)
      fl = self.transform(fl,
                          scale=(W + 2 * flare_m * fo, flare_h * ft + skirt_m,
                                 D + 2 * flare_m * fo),
                          translate=(0, (flare_h * ft - skirt_m) * 0.5, 0))
      lower = self.merge(lower, fl, gid_a=None, gid_b=None)

    ext = max(W + 2 * flare_m, Htop + skirt_m, D + 2 * flare_m) + 1.2   # ONE cubic framing
    ctr = (0.0, Htop * 0.5, 0.0)
    sd  = self.mesh_to_sdf(lower, dim=sdf_dim, extent=ext, center=ctr, sign_mode=1)
    if stepped:
      uy0 = deck1 - 0.3                                # embed into the lower block
      up  = self.box(size=0.5)
      up  = self.transform(up, scale=(Wu, H2 - uy0, D2), translate=(xu, (uy0 + H2) * 0.5, zu))
      sdu = self.mesh_to_sdf(up, dim=sdf_dim, extent=ext, center=ctr, sign_mode=1)
      # smooth_union k -> the coved adobe junction where the upper wall meets the terrace
      sd  = self.csg(sd, sdu, op="smooth_union", k=smooth_k)

    # ---- cutters (all MESH boxes, merged into ONE voxelization; overlaps are fine
    # under winding sign). Roof recesses make the parapets; T-doors + windows make
    # the openings. `openings` records (axis, face, x_or_z, ymid, w, h, recess) so
    # the lintels + dark recess plates merge back on the SAME grammar afterwards.
    cut = [None]
    openings = []

    def CUT(sx, sy, sz, tx, ty, tz):
      b = self.box(size=0.5)
      b = self.transform(b, scale=(sx, sy, sz), translate=(tx, ty, tz))
      cut[0] = b if cut[0] is None else self.merge(cut[0], b, gid_a=None, gid_b=None)

    def t_door(xd, ybase, face_z, reveal):
      # the Chaco T: two overlapping boxes — narrow leg + wide head, ~1.5 m total
      h_lo   = door_h - door_head
      z_len  = reveal + flare_m + 0.25                 # punches past the flare, stops mid-wall
      z_mid  = face_z + (flare_m + 0.25 - reveal) * 0.5
      CUT(door_w_lo, h_lo + 0.18, z_len, xd, ybase + (h_lo + 0.18) * 0.5 - 0.12, z_mid)
      CUT(door_w_hi, door_head + 0.08, z_len, xd, ybase + door_h - (door_head + 0.08) * 0.5 + 0.04, z_mid)
      openings.append(("z", face_z, xd, ybase + door_h * 0.5, door_w_hi, door_h, reveal, ybase + door_h))

    def window(xw, yw, face_z, reveal):
      ww = win_w * rng.uniform(0.85, 1.2)              # irregular, hand-made openings
      wh = win_h * rng.uniform(0.85, 1.15)
      CUT(ww, wh, reveal + flare_m + 0.2, xw, yw, face_z + (flare_m + 0.2 - reveal) * 0.5)
      openings.append(("z", face_z, xw, yw, ww, wh, reveal, yw + wh * 0.5))

    # roof recesses -> parapet rings + ROOF decks
    if stepped:
      CUT(Wu - 2 * parapet_t, 1.2, D2 - 2 * parapet_t, xu, deck2 + 0.6, zu)
      tz_lo, tz_hi = zu_hi + 0.02, D * 0.5 - parapet_t          # front terrace
      CUT(W - 2 * parapet_t, 1.4, tz_hi - tz_lo, 0.0, deck1 + 0.7, 0.5 * (tz_lo + tz_hi))
      if ur < rooms:                                             # side terrace strip
        if side > 0: sx_lo, sx_hi = -W * 0.5 + parapet_t, xu - Wu * 0.5 - 0.02
        else:        sx_lo, sx_hi = xu + Wu * 0.5 + 0.02, W * 0.5 - parapet_t
        if sx_hi - sx_lo > 0.5:
          CUT(sx_hi - sx_lo, 1.4, D - 2 * parapet_t, 0.5 * (sx_lo + sx_hi), deck1 + 0.7, 0.0)
    else:
      CUT(W - 2 * parapet_t, 1.2, D - 2 * parapet_t, 0.0, deck1 + 0.6, 0.0)

    # ground-storey T-doors (front facade, +Z)
    for b in door_bays:
      t_door(cols[b], 0.0, D * 0.5, door_reveal)

    # terrace-level T-door (stepped): opens onto the terrace — the pueblo circulation
    if stepped:
      ucols  = [xu + (c - (ur - 1) * 0.5) * room_w for c in range(ur)]
      ub     = rng.randrange(ur)
      t_door(ucols[ub], deck1, zu_hi, door_reveal * 0.8)
      for c in range(ur):                              # sparse upper openings
        if c == ub or rng.random() > win_keep:
          continue
        window(ucols[c] + rng.uniform(-0.3, 0.3), deck1 + storey_h * 0.6, zu_hi, reveal_d)

    # ground-storey windows: sparse, irregular, never in a door bay; back facade
    # stays BLANK (defensive), sides get at most one small opening each
    for c in range(rooms):
      if c in door_bays or rng.random() > win_keep:
        continue
      window(cols[c] + rng.uniform(-0.3, 0.3), storey_h * 0.72 + rng.uniform(-0.08, 0.08),
             D * 0.5, reveal_d)
    for sgn in (1.0, -1.0):
      if rng.random() > 0.5:
        continue
      ww = win_w * rng.uniform(0.85, 1.15)
      wh = win_h * rng.uniform(0.85, 1.15)
      zw = rng.uniform(-(D * 0.5 - 1.0), D * 0.5 - 1.0)
      yw = storey_h * 0.72
      CUT(reveal_d + flare_m + 0.2, wh, ww,
          sgn * (W * 0.5 + (flare_m + 0.2 - reveal_d) * 0.5), yw, zw)
      openings.append(("x", sgn * W * 0.5, zw, yw, ww, wh, reveal_d, yw + wh * 0.5))

    # ---- carve: voxelize cutters (same framing) -> subtract -> redistance ->
    # ADAPTIVE clean remesh (O2). The voxel grid itself still rounds the corners
    # (the eroded-adobe read comes from the SDF, not the fairing); adaptivity
    # collapses the flat wall interiors into big quads while the extractor keeps
    # detail at curvature. unwrap=False: quads preserved, UVs wait for the O3
    # bake path (adobe/timber ptex3d sample object position today). The op bakes
    # smooth area-weighted vertex normals — no smooth_normals pass needed.
    cutn   = self.mesh_to_sdf(cut[0], dim=sdf_dim, extent=ext, center=ctr, sign_mode=1)
    carved_sdf = self.csg(sd, cutn, op="subtract")
    rd     = self.redistance(carved_sdf)               # true |grad|=1 -> accurate iso-crossings
    carved = self.sdf_to_mesh_clean(rd, adaptivity=float(adaptivity), unwrap=False)

    # ---- post-carve re-gid: gid did NOT survive the SDF round trip. Up-facing
    # faces in the deck bands -> ROOF (earthen decks); everything else stays WALL
    # (reveals INCLUDED — adobe runs through the openings; timber is the lintel's job).
    carved = self.select(carved, (S.N.y > 0.75) & (S.P.y > deck1 - 0.20) & (S.P.y < deck1 + 0.15),
                         domain=POLY, op=replace(group(_B_DECK1)))
    carved = self.assign_gid(carved, gid=GID_ROOF, slot=_B_DECK1)
    if stepped:
      carved = self.select(carved, (S.N.y > 0.75) & (S.P.y > deck2 - 0.20) & (S.P.y < deck2 + 0.15),
                           domain=POLY, op=replace(group(_B_DECK2)))
      carved = self.assign_gid(carved, gid=GID_ROOF, slot=_B_DECK2)

    # ---- degenerate-tail drop (SdfToMeshClean fills exact CSR counts, but the
    # drop stays as cheap insurance for zero-area slivers before CPU consumers —
    # the _building capacity-tail ordering, re-run AFTER the remesh per plan)
    carved = self.select(carved, S.area < 1e-9, domain=POLY, op=replace(group(_B_TAIL)))
    carved = self.delete_faces(carved, slot=_B_TAIL)

    # ---- trims (small parts chained FIRST; the capacity-inflated carved mesh
    # folds in exactly ONCE, LAST — the merge-order memory decision)
    bld = [None]

    def TRIM(node, gid):
      if bld[0] is None:
        bld[0] = self.assign_gid(node, gid=gid)        # whole-mesh stamp on the chain base
      else:
        bld[0] = self.merge(bld[0], node, gid_a=None, gid_b=gid)

    def viga_row(x0, x1, y, z_mid, z_len):
      n  = max(2, int((x1 - x0) // viga_spacing))
      xs = [x0 + (x1 - x0) * (i + 0.5) / n for i in range(n)]
      for x in xs:
        v = self.box(size=0.5)
        v = self.transform(v, scale=(viga_d, viga_d, z_len), translate=(x, y, z_mid))
        TRIM(v, GID_TRIM)

    # vigas: beam ends protrude ~viga_out under EVERY roofline (the iconic detail).
    # Row 1 spans the FULL lower block (ends emerge from both long facades — under
    # the upper block they emerge at terrace level, as at Taos).
    viga_row(-W * 0.5 + 0.5, W * 0.5 - 0.5, deck1 - 0.10, 0.0, D + 2 * viga_out)
    if stepped:
      viga_row(xu - Wu * 0.5 + 0.5, xu + Wu * 0.5 - 0.5, deck2 - 0.10, zu, D2 + 2 * viga_out)

    # timber lintels (proud ~5 cm) + dark recess plates (the "no glass" dark
    # opening read) for every recorded opening
    for (axis, face, a, ymid, w, h, recess, ytop) in openings:
      if axis == "z":
        sgn = 1.0 if face > 0 else -1.0
        li = self.box(size=0.5)
        li = self.transform(li, scale=(w + 0.30, 0.12, 0.55),
                            translate=(a, ytop + 0.08, face - sgn * 0.23))
        TRIM(li, GID_TRIM)
        pl = self.box(size=0.5)
        pl = self.transform(pl, scale=(w + 0.25, h + 0.25, 0.06),
                            translate=(a, ymid, face - sgn * (recess - 0.06)))
        TRIM(pl, GID_DOOR)
      else:
        sgn = 1.0 if face > 0 else -1.0
        li = self.box(size=0.5)
        li = self.transform(li, scale=(0.55, 0.12, w + 0.30),
                            translate=(face - sgn * 0.23, ytop + 0.08, a))
        TRIM(li, GID_TRIM)
        pl = self.box(size=0.5)
        pl = self.transform(pl, scale=(0.06, h + 0.25, w + 0.25),
                            translate=(face - sgn * (recess - 0.06), ymid, a))
        TRIM(pl, GID_DOOR)

    # terrace ladder (stepped): two leaning poles + rungs — the pueblo silhouette
    # prop. Physics-excluded embellishment (the box proxy covers the massing only).
    if stepped and ladder:
      free = [c for c in range(rooms) if c not in door_bays] or list(range(rooms))
      xlad = cols[rng.choice(free)]
      # REAL LEANED LADDER (owner HMD finding jul30, 4:1 rule): pivot at the FEET,
      # top rail against the parapet edge, feet out by contact_height x tan(tilt).
      # Rotation is standard RIGHT-HANDED (kiva ladder proves it): about +X a
      # NEGATIVE angle tips the +Y pole toward -Z (the wall). The old +11 tipped
      # the top AWAY from the facade — the 07-22 sign note here was wrong.
      tilt = 14.0                                      # 4:1 setback rule (tan ~ 0.25)
      L    = (deck1 + 0.9) / math.cos(math.radians(tilt))   # poles protrude past the rim
      zlad = D * 0.5 + H1 * math.tan(math.radians(tilt))    # feet: contact at (z=D/2, y=H1)
      for dx in (-0.25, 0.25):
        p = self.box(size=0.5)
        p = self.transform(p, scale=(0.07, L, 0.07), translate=(0, L * 0.5, 0))
        p = self.transform(p, rotate=(vec3(1, 0, 0), -tilt), translate=(xlad + dx, 0.0, zlad))
        TRIM(p, GID_TRIM)
      for s in (0.5, 1.1, 1.7, 2.3, 2.9):              # top rung near the roof edge
        r = self.box(size=0.5)
        r = self.transform(r, scale=(0.6, 0.055, 0.055), translate=(0, s, 0))
        r = self.transform(r, rotate=(vec3(1, 0, 0), -tilt), translate=(xlad, 0.0, zlad))
        TRIM(r, GID_TRIM)

    bld[0] = self.merge(bld[0], carved, gid_a=None, gid_b=None)   # the ONE big copy, last

    # O3 STORED-MODE (opt-in): PER-SECTION UV unwrap as the LAST op on the fully-
    # merged, fully-gid'd mesh -> one layer per gid bucket ([0,2,3,4]) for the baked
    # texture-ARRAY path. Costs ~30% verts (xatlas seam duplication) and only pays in
    # stored mode, so the proc path leaves it OFF (byte-identical mesh). padding /
    # max_layers ride through as kwargs (A8).
    out = bld[0]
    if sectioned:
      out = self.section_unwrap(out, padding=int(section_padding),
                                max_layers=int(section_max_layers))
    self.output(out)

    # physics-proxy grammar (kept in sync with the scatter sink's colliders= —
    # swestvale.py quotes these): box half-extents derived from the SAME massing
    # params that built the mesh; y = FULL height (centered btBoxShape convention).
    self.collider_half_extents = (W * 0.5 + flare_m + 0.05, Htop, D * 0.5 + flare_m + 0.05)

  def materials(self):
    # standalone viewer/validator preview only — scenes bind their own ptex3d
    # materials per gid (adobe / timber) via drawable_data(materials={...})
    return [HmMaterial(Solid,               albedo=vec3(0.62, 0.47, 0.32), roughness=0.95),
            HmMaterial(Solid, gid=GID_DOOR, albedo=vec3(0.16, 0.11, 0.07), roughness=0.85),
            HmMaterial(Solid, gid=GID_ROOF, albedo=vec3(0.58, 0.44, 0.30), roughness=0.95),
            HmMaterial(Solid, gid=GID_TRIM, albedo=vec3(0.33, 0.24, 0.15), roughness=0.85)]


__all__ = ["Pueblo", "GID_WALL", "GID_DOOR", "GID_ROOF", "GID_TRIM"]
