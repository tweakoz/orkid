###############################################################################
# bld_kiva — pueblo variant C: a KIVA — the circular ceremonial chamber, now the
# SUNKEN STONE-LINED form (owner-directed 07-22): coursed DRYSTONE MASONRY AS
# GEOMETRY (individual stones, not a texture) lining a terrain-cut pit (the
# swestvale "kivas" scatter_place sinks the bowl KIVA_SINK_M below court grade
# and drops this mesh onto the pit floor via lift). The visible read: a low
# stone collar above the court, a stone-lined chamber below the rim, and the
# iconic timber ladder rising out of the hatch — the Mesa Verde kiva court.
#
# TECHNIQUE: real kiva masonry is COURSED sandstone in running bond with a
# basal batter. Built literally: N stones per course x M courses of jittered
# boxes (length/thickness/height/radial jitter; half-pitch phase stagger per
# course; radius swelling toward the base) -> ONE winding-sign voxelization at
# HIGH dim (joints must survive: gap >= ~2 voxels) -> redistance -> ADAPTIVE
# volumeToMesh. The SDF pass rounds each stone (weathered edges) while the
# inter-stone gaps reconstruct as deep recessed JOINTS — one connected shell,
# no per-stone shell blowup.
#
# MESH ORIGIN CONTRACT: y=0 is the RING BASE plane. Standalone (no sink) the
# ring stands `height` proud of grade like any building. In scn_swest the
# terrain bowl drops the origin to the pit floor (lift=-KIVA_SINK_M), so the
# collar shows height-KIVA_SINK_M above the court and the courses line the pit.
#
# GID MAP: 0 WALL stone (scene binds a stone-tinted material) 4 TRIM timber
# (ladder). VET: allow_self_intersect — stones/ladder overlap by construction.
#
#   ork.hypermesh.viewer.py bld_kiva --msaa 2
#   _ork.hypermesh.validate.py bld_kiva -o /tmp/bld_kiva.obj
###############################################################################
import math
import random
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, HmMaterial, S, replace, group, POLY
from ork.hypergraph.assets.materials.terrain.solid import Solid
from ork.hypergraph.assets.hypermesh._pueblo import GID_TRIM

_B_TAIL = 6


class Kiva(Hypermesh):
  VET = dict(allow_self_intersect=True)

  # REGISTER BUDGET (engine constraint, hit 07-22): the hypermesh dataflow
  # register pool is 64 GpuMesh regs (hmdflow.cpp createRegisters "hm_mesh")
  # and the sorter schedules leaf boxes breadth-first — live registers ~= total
  # stone count. ~55 stones is the ceiling, so the coursing is CYCLOPEAN
  # (4 courses x ~12 big blocks — the Chacoan great-kiva read) rather than
  # fine Mesa Verde ashlar. Finer coursing wants a bigger hm_mesh pool or an
  # instanced-stone composer (noted for the engine lane; not worth blocking on).
  def __init__(self, seed=5, *, radius=2.6, wall_t=0.45, height=1.0, skirt_m=0.45,
               course_h=0.3625, joint_v=0.07, stone_len=1.15, joint_az=0.09,
               batter_m=0.14, sdf_dim=224, adaptivity=0.5,
               ladder=True, ladder_drop=0.0, floor=True,
               sectioned=False, section_padding=4, section_max_layers=8,
               **_overrides):
    super().__init__()
    rng = random.Random(int(seed))
    Rm  = radius - wall_t * 0.5                       # ring mid-line radius
    y_lo = -skirt_m                                   # below-base skirt (bedding)
    n_courses = max(2, int(math.ceil((height + skirt_m) / course_h)))

    # ---- coursed drystone ring: jittered stone boxes, running bond, basal batter.
    # CONTACT LAW (first-render lesson 07-22): stones TOUCH — azimuthal chords
    # OVERLAP (x1.12+) and courses stack at full pitch — air gaps at this scale
    # survive the SDF as see-through holes, not joints. The masonry read comes
    # from PER-STONE OFFSETS instead: radial jitter (+/-5 cm, > a voxel) steps
    # each stone's face to its own radius, per-stone height/yaw wobble breaks
    # the courses — the redistance rounds every step into a recessed shadow
    # joint while the wall stays a closed solid.
    ring = None
    for c in range(n_courses):
      cy   = y_lo + (c + 0.5) * course_h              # course center height
      # batter: lower courses swell OUTWARD (walls thicken toward the ground)
      Rc   = Rm + batter_m * (1.0 - c / max(1, n_courses - 1)) \
                + (0.028 if (c % 2) else -0.028)     # course faces alternate radius:
                                                     # guaranteed horizontal course line
      circ = 2.0 * math.pi * Rc
      n_st = max(8, int(round(circ / (stone_len + joint_az))))
      pitch = 2.0 * math.pi / n_st
      phase = (c % 2) * 0.5 * pitch                   # running bond stagger
      for i in range(n_st):
        a  = i * pitch + phase + rng.uniform(-0.05, 0.05) * pitch
        sl = pitch * Rc * rng.uniform(1.10, 1.22)     # chord OVERLAP: closed wall
        sh = course_h * rng.uniform(0.98, 1.06)       # full-pitch stack (touching)
        st = wall_t * rng.uniform(0.85, 1.18)
        rr = Rc + rng.uniform(-0.075, 0.075)          # the joint-maker: face steps
        b = self.box(size=0.5)
        b = self.transform(b, scale=(sl, sh, st),
                           rotate=(vec3(0, 1, 0), math.degrees(a) + 90.0 + rng.uniform(-2.5, 2.5)),
                           translate=(rr * math.cos(a), cy + rng.uniform(-0.015, 0.015),
                                      rr * math.sin(a)))
        ring = b if ring is None else self.merge(ring, b, gid_a=None, gid_b=None)

    # ---- CHAMBER FLOOR (owner 07-22 "sink a terrain inside the pit"): the
    # scene terrain DOES sink inside the pit (the kiva sink's grading pads —
    # bake + physics exact), but the terrain RENDER mesh is ~2 m/texel and
    # cannot draw a crisp 5 m chamber floor. So the ASSET carries its own dug
    # floor: an octagon slab (two 45deg-crossed boxes) at the ring base,
    # merged into the pre-SDF shell — a flagged-stone floor reading crisp at
    # any terrain resolution; edges embed in the masonry (allow_self_intersect).
    if floor:
      for rot in (0.0, 45.0):
        f = self.box(size=0.5)
        f = self.transform(f, scale=(3.8, 0.14, 3.8),
                           rotate=(vec3(0, 1, 0), rot),
                           translate=(0.0, 0.03, 0.0))
        ring = self.merge(ring, f, gid_a=None, gid_b=None)

    # ---- ONE voxelization at joint-preserving fidelity (banked gotchas: explicit
    # extent, one framing, winding sign). ext 6.8 m @ dim 224 -> 3.0 cm voxels;
    # 7-9 cm joints span >=2 voxels and reconstruct as recessed masonry lines.
    ext = 2.0 * radius + 1.6
    ctr = (0.0, 0.0, 0.0)
    sd  = self.mesh_to_sdf(ring, dim=sdf_dim, extent=ext, center=ctr, sign_mode=1)
    rd  = self.redistance(sd)
    rm  = self.sdf_to_mesh_clean(rd, adaptivity=float(adaptivity), unwrap=False)
    rm = self.select(rm, S.area < 1e-9, domain=POLY, op=replace(group(_B_TAIL)))
    rm = self.delete_faces(rm, slot=_B_TAIL)          # degenerate-sliver insurance (post-remesh)

    # ---- ladder: rises from the (sunken) ring base over the rim — physics-excluded
    # embellishment. Base at y=0 (ring base = pit floor in-scene); length clears the
    # collar with the classic protruding poles.
    bld = None
    if ladder:
      # LADDER FROM THE PIT FLOOR (owner 07-22): base stands ladder_drop below the
      # ring origin (the shaft floor; the scene passes the kiva sink depth), climbs
      # through the mouth, and PROTRUDES ~1m above the collar — the iconic hatch
      # ladder. Top rests at the inner rim: base x chosen so the 14-degree lean
      # lands the top against the collar's inner face. ladder_drop=0 keeps the
      # legacy at-grade ladder. Rungs at 0.65m (register-pool budget: ~52 stones
      # + 2 poles + rungs must stay under the 64-register hm_mesh cap).
      import math as _m
      tilt  = 14.0
      drop  = float(ladder_drop)
      climb = drop + height + 1.0                     # floor -> collar top + 1m proud
      L     = climb / _m.cos(_m.radians(tilt))
      bx    = max(0.35, (radius - wall_t - 0.30) - climb * _m.tan(_m.radians(tilt)))
      for dz in (-0.22, 0.22):
        p = self.box(size=0.5)
        p = self.transform(p, scale=(0.07, L, 0.07), translate=(0, L * 0.5, 0))
        p = self.transform(p, rotate=(vec3(0, 0, 1), -tilt), translate=(bx, -drop, dz))
        bld = self.assign_gid(p, gid=GID_TRIM) if bld is None else \
              self.merge(bld, p, gid_a=None, gid_b=GID_TRIM)
      s_ = 0.5
      while s_ < L - 0.4:
        r = self.box(size=0.5)
        r = self.transform(r, scale=(0.07, 0.055, 0.58), translate=(0, s_, 0))
        r = self.transform(r, rotate=(vec3(0, 0, 1), -tilt), translate=(bx, -drop, 0.0))
        bld = self.merge(bld, r, gid_a=None, gid_b=GID_TRIM)
        s_ += 0.65
      bld = self.merge(bld, rm, gid_a=None, gid_b=None)   # the ONE big copy, last
    else:
      bld = rm

    # O3 STORED-MODE (opt-in): PER-SECTION UV unwrap as the LAST op on the fully-
    # merged, fully-gid'd mesh -> one layer per gid bucket ([0,4]) for the baked
    # texture-ARRAY path. ~30% more verts (xatlas seams); proc path leaves it OFF
    # so the physics/scatter mesh stays byte-identical. padding/max_layers = kwargs (A8).
    if sectioned:
      bld = self.section_unwrap(bld, padding=int(section_padding),
                                max_layers=int(section_max_layers))
    self.output(bld)

    # physics-proxy grammar (swestvale.py quotes this): ring footprint + the basal
    # batter overhang (+~0.2 incl. jitter), ladder excluded; y = FULL height
    # (centered btBoxShape). In-scene the "kivas" sink drops the box with the mesh.
    self.collider_half_extents = (radius + 0.35, height + 0.25, radius + 0.35)

  def materials(self):
    # standalone preview: dry sandstone + timber (scenes bind their own per gid)
    return [HmMaterial(Solid,               albedo=vec3(0.52, 0.45, 0.36), roughness=0.97),
            HmMaterial(Solid, gid=GID_TRIM, albedo=vec3(0.33, 0.24, 0.15), roughness=0.85)]


__all__ = ["Kiva"]
