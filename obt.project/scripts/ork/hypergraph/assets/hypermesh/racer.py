###############################################################################
# Racer — a Wipeout-2048-style anti-grav wedge-ship, BLOCK-OUT from the verbs that exist today
# (box -> transform -> extrude_faces[+per-face expr] -> inset -> face_normals). This is the Phase-2
# acceptance-gate asset: a PERSISTED hard-surface target so the op set stops drifting, and a concrete
# surface for what's still missing. Orientation: +Z forward, +Y up, +X right, centered on x=0.
#
# What it exercises (all working): box hull, non-uniform transform scale, per-face extrude with inset
# (tapered raked nose), extrude along face normals (symmetric side pods, no per-side direction needed),
# inset->octagon + inward extrude (recessed rear thrusters), flat hard-surface normals.
#
# What it makes OBVIOUS is missing (the next two verbs): crisp chamfered EDGES (no bevel -> the hull
# reads blocky, not the origami Wipeout look) and MULTIPLE MATERIALS on one mesh (hull metal vs canopy
# glass vs emissive intakes -> needs the gid multi-material partition). Build those next against this gate.
#
#   ./ork.hypermesh.viewer.py racer        # [W] wireframe, [O] dump OBJ
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import (Hypermesh, S, sel_normal_dir, sel_dihedral_gt,
                                            add, isolate, replace, group, sl_sin, POLY, LINE)


class Racer(Hypermesh):
  def __init__(self):
    super().__init__()
    self.rotang = 0.0
    n = self.box(size=1.0)
    n = self.transform(n, scale=(1.1, 0.42, 2.6))                 # long, low, wide-ish hull

    # NOSE: taper + down-rake the front (+Z) face forward (per-face inset + a down-forward direction).
    n = self.select(n, sel_normal_dir(n=vec3(0, 0, 1), t=0.6, soft=0.05), domain=POLY, op=add(group(0)))
    n = self.extrude_faces(n, distance=3.5, inset=0.95, direction=vec3(0, -0.28, 1.0), slot=0)

    # SIDE PODS: extrude both ±X faces outward — along each face's own normal, so no per-side direction.
    n = self.select(n, sel_normal_dir(n=vec3( 1, 0, 0), t=0.6, soft=0.05), domain=POLY, op=add(group(1)))
    n = self.select(n, sel_normal_dir(n=vec3(-1, 0, 0), t=0.6, soft=0.05), domain=POLY, op=add(group(1)))
    n = self.extrude_faces(n, distance=1.15, inset=0.78, slot=1)

    # REAR THRUSTERS: octagon inset on the back (-Z) face, then extrude that cap INWARD (recessed nozzle).
    n = self.select(n, sel_normal_dir(n=vec3(0, 0, -1), t=0.6, soft=0.05), domain=POLY, op=add(group(2)))
    n = self._inset = self.inset(n, amount=0.08, sides=36, rotate=self.rotang, slot=2, mask_inner=isolate(group(3)))
    n = self.extrude_faces(n,
                           distance=S.select(S.i % 2, -0.1, S.sin(S.time*-3)*0.1+0.1),
                           segments=8,
                           twist = S.select(S.i % 2, 0.0, S.t * 3.14 * S.sin(S.time*-3)*0.05+0.05),
                           scale=S.pow(1.0-S.t,0.75),
                           slot=3,             # negative = into the hull
                           mask_cap=add(group(4)),    # tag the nozzle cap+walls "exhaust" (bit 4) so subdivide skips them
                           mask_wall=add(group(4)))

    n = self.face_normals(n)                                       # hard-surface faceted shading (kept on the un-subdivided exhaust)
    # SUBDIVIDE everything EXCEPT the rear exhaust: collar=bit2, port=bit3, nozzle=bit4. Tag the complement
    # (faces carrying none of those) into bit5 and smooth-subdivide only it -> rounded hull, crisp exhaust,
    # watertight n-gon seam where they meet (the masked Catmull-Clark from subdiv_mask_demo).
    #n = self.select(n, ~(S.tag(2) | S.tag(3) | S.tag(4)), domain=POLY, op=add(group(5)))
    #n = self.subdivide(n, smooth=True, level=3, slot=5)
    #n = self.smooth_normals(n)                                  # WELD coincident verts -> manifold edges (face_normals SPLITS)
    #n = self.smooth_normals(n)                                  # WELD coincident verts -> manifold edges (face_normals SPLITS)
    self.output(n)

  def onUpdate(self, updinfo):
    self.rotang += 0.1
    self._inset.inputs.rotate = self.rotang   # LIVE plug: spins the nozzle ring (runtime, no rebuild)

__all__ = ["Racer"]
