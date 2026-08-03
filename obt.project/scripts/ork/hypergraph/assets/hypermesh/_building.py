###############################################################################
# _building — the GRAMMAR-BUILDING generator library (GR-B): parameterized hamlet
# buildings from the ratified verb recipe — every step is an existing hypermesh op,
# this file only COMPOSES them:
#
#   box -> extrude_faces storeys -> roof (vertex-extrude + ridge collapse | inset
#   parapet) -> facade SHELL as closed PRIMITIVE slab boxes riding 1cm proud of the
#   massing's facade planes (which stay: interior backing walls — see the gotchas) ->
#   mesh_to_sdf (EXPLICIT extent) -> CSG-subtract the window/door reveals ->
#   sdf_to_mesh(weld) -> position-band re-assign_gid -> merge trims (glass panes /
#   door slab / plinth) back on.
#
# GID MAP (A1 law: gid = __tags[20:32), GidAssign is the ONLY writer, classes are
# SEMANTIC — bind materials per gid via drawable_data(materials={gid: mat})):
#   0  WALL   plaster/stone body (the default; carved shell re-reads as wall)
#   1  GLASS  window panes    (A2C glass in scenes: surface(opacity=, alpha_to_coverage=True))
#   2  DOOR   door slab
#   3  ROOF   gable planes + ridge cap / recessed flat cap + parapet inner walls
#   4  TRIM   carved reveals (position-band re-gid post-SDF) + plinth base course
#
# The banked gotchas this file encodes (violations have bitten before):
#   * NEVER bake a nested SdfExpr union chain into one SdfEval — shadlang's
#     backtracking parser is EXPONENTIAL in expression nesting (~5x per union
#     level; 3 unions = 68GB/63s, OOM-killed the whole JUL18 fleet gate) — and
#     even a SINGLE baked box expression parses at ~3.5GB. This recipe uses ZERO
#     SdfEval: cutters are MESH boxes voxelized by one extra mesh_to_sdf (stock
#     flat kernels, ~1GB cold), identically sharp at the shared brick resolution.
#   * merge the sdf_to_mesh output ONCE, LAST: SdfToMesh reports CAPACITY counts
#     (2x live, pow2) to the CPU, and MergeMesh concats at the reported counts —
#     chaining it early re-copies the inflated mesh through every merge.
#   * mesh_to_sdf: EXPLICIT extent= always (AUTO refits + pads 0.12 -> unstable brick).
#   * carve only a facade SHELL: closed primitive slab boxes proud of the massing,
#     carved, merged back. The whole massing NEVER goes through the SDF — and the
#     shell is NEVER solidified via extrude keep_base: keep_base re-closes the
#     footprint with the PARENT'S winding (both big faces same direction) -> the
#     voxelizer sign checkerboards per voxel -> ghost terrace sheets every voxel
#     plane (the JUL18 stairstep re-gate defect).
#   * never merge AFTER a delete_faces mesh (RECIPE DEVIATION from the ratified
#     'separate' step, flagged for adjudication): deleted faces leave orphan corner
#     slots in the CSR, and MergeMesh fuses a trailing orphan run into the NEXT
#     part's first face — a folded 12/20/44-gon (the JUL18 fanfold FAIL; engine
#     seam). Hence the massing keeps its facade planes as interior BACKING walls,
#     1cm behind the shell slabs (never coplanar); the carved mesh's own tail-drop
#     delete is safe because carved merges LAST (its orphans trail the final CSR).
#   * gid survives merge but NOT sdf_to_mesh -> reveals re-assign_gid'd via a
#     position/normal-band SelExpr AFTER the carve.
#   * trims deliberately EMBED into the walls (kit-bash assembly) -> variants declare
#     VET = dict(allow_self_intersect=True); buried-parity demotes to WARN likewise.
#   * facade slabs sit PROUD (outward extrude) so their end rims land BESIDE the
#     side-wall planes (edge contact) instead of coplanar-overlapping them (z-fight
#     + the meshvet coplanar-SAT check).
#
# Variants (the runnable assets): bld_cottage / bld_longhouse / bld_tower.
###############################################################################
import random
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import (Hypermesh, HmMaterial, S, sel_normal_dir,
                                            replace, isolate, group, POLY)
from ork.hypergraph.assets.materials.terrain.solid import Solid

GID_WALL, GID_GLASS, GID_DOOR, GID_ROOF, GID_TRIM = 0, 1, 2, 3, 4

# __tags scratch bits (free region [0:20) — gid rides the locked [20:32) band)
_B_TOP, _B_ROOFCAP, _B_ROOFSEL, _B_REVEAL, _B_TAIL = 0, 1, 2, 5, 6

# GABLE-TAPER kernel: linear z-taper over the prism height — z' = z*(1 - u*(1-rf)),
# u = clamp((y - y0)/(y1 - y0)). Per-VERTEX, so coincident per-face vertex copies
# transform identically and seams stay closed (transform(slot) can NOT build a gable:
# primitives are per-face-unwelded, so it moves only the tagged face — the JUL18
# open-ring roof). EXPRP[0] = (y0, y1, ridge_frac, unused).
_GABLE_KERNEL = """
fxconfig fxcfg_default {}
storage_interface gif_iP (descriptor_set 0) { buffer layout(std430) gipb { vec4 iP[]; }; }
storage_interface gif_oP (descriptor_set 0) { buffer layout(std430) gopb { vec4 oP[]; }; }
storage_interface gif_pm (descriptor_set 0) { buffer layout(std430) gpmb { vec4 EXPRP[]; }; }
storage_interface gif_ct (descriptor_set 0) { buffer layout(std430) gctb { uint p_nv; uint p_mode; uint p0; uint p1; }; }
compute_interface iface_gable { storage { gif_iP gif_oP gif_pm gif_ct } inputs { layout(local_size_x = 64); } }
compute_shader cs_gable_taper : iface_gable {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  float y0 = EXPRP[0].x;
  float y1 = EXPRP[0].y;
  float rf = EXPRP[0].z;
  vec3 p = iP[v].xyz;
  float u = clamp((p.y - y0) / max(y1 - y0, 0.000001), 0.0, 1.0);
  p.z = p.z * (1.0 - u * (1.0 - rf));
  oP[v] = vec4(p, 1.0);
}
"""

# LATTICE-SNAP kernel (see the call site): snap any vertex within EXPRP[1].x lattice
# units of a brick lattice point exactly onto it, per axis. EXPRP[0] = (origin.xyz,
# voxel). Follows the GpuCompute storage contract (iP@0 oP@1 EXPRP@2 control@3).
_SNAP_KERNEL = """
fxconfig fxcfg_default {}
storage_interface gif_iP (descriptor_set 0) { buffer layout(std430) gipb { vec4 iP[]; }; }
storage_interface gif_oP (descriptor_set 0) { buffer layout(std430) gopb { vec4 oP[]; }; }
storage_interface gif_pm (descriptor_set 0) { buffer layout(std430) gpmb { vec4 EXPRP[]; }; }
storage_interface gif_ct (descriptor_set 0) { buffer layout(std430) gctb { uint p_nv; uint p_mode; uint p0; uint p1; }; }
compute_interface iface_snap { storage { gif_iP gif_oP gif_pm gif_ct } inputs { layout(local_size_x = 64); } }
compute_shader cs_lattice_snap : iface_snap {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  vec3 o    = EXPRP[0].xyz;
  float h   = EXPRP[0].w;
  float eps = EXPRP[1].x;
  vec3 p = iP[v].xyz;
  vec3 gg = (p - o) / h;
  vec3 rr = floor(gg + vec3(0.5, 0.5, 0.5));
  vec3 dd = abs(gg - rr);
  if (dd.x < eps) { p.x = o.x + rr.x * h; }
  if (dd.y < eps) { p.y = o.y + rr.y * h; }
  if (dd.z < eps) { p.z = o.z + rr.z * h; }
  oP[v] = vec4(p, 1.0);
}
"""


class Building(Hypermesh):
  """One hamlet building. `storeys`/`bays` set the window grammar; `seed` jitters the
  proportions (style variation without new topology rules). All trims are merged
  sub-meshes, so the result is ONE mesh -> one drawable, five gid material buckets."""

  VET = dict(allow_self_intersect=True)   # kit-bash: panes/door/plinth embed into walls BY DESIGN

  def __init__(self, storeys=2, bays=3, *,
               bay_w=2.4, storey_h=2.8, depth=5.0, wall_t=0.30,
               win_w=1.1, win_h=1.3, door_w=1.1, door_h=2.1,
               roof="gable", roof_h=1.6, ridge_w=0.12, parapet_drop=0.5,
               plinth_h=0.28, skirt_m=0.4, sdf_dim=128, seed=0):
    super().__init__()
    # style seed: proportion jitter ONLY (deterministic; topology/grammar unchanged)
    rng      = random.Random(int(seed))
    storey_h = storey_h * rng.uniform(0.97, 1.03)
    roof_h   = roof_h   * rng.uniform(0.88, 1.12)
    win_w    = win_w    * rng.uniform(0.94, 1.06)
    W, H, D, t = bays * bay_w, storeys * storey_h, depth, wall_t

    # ---- window/door grammar (plain Python — drives BOTH the SDF cutters and the panes)
    db    = (bays - 1) // 2                                  # the door bay (ground storey, front)
    cols  = [(c - (bays - 1) * 0.5) * bay_w for c in range(bays)]
    cells = [(cols[c], (r + 0.55) * storey_h)                # window centres; the door cell is
             for r in range(storeys) for c in range(bays)    # skipped on BOTH facades (cutters
             if not (r == 0 and c == db)]                    # punch straight through)

    # ---- massing: box -> storey rings (multi-segment extrude of the roof-ward cap)
    n = self.box(size=0.5)                                   # unit cube (half-extent 0.5)
    n = self.transform(n, scale=(W, storey_h, D), translate=(0, storey_h * 0.5, 0))
    n = self.select(n, sel_normal_dir(n=vec3(0, 1, 0), t=0.7), domain=POLY,
                    op=replace(group(_B_TOP)))
    if storeys > 1:                                          # cap keeps _B_TOP (identity masks)
      n = self.extrude_faces(n, distance=storey_h, segments=storeys - 1, slot=_B_TOP)

    # ---- roof (simple gable / flat only — straight-skeleton roofs are OUT of scope)
    roof_part = None
    if roof == "gable":
      # a SEPARATE gable PRISM: box -> per-VERTEX z-taper kernel -> ridge FLAT
      # (ridge_w > 0 avoids a degenerate cap; the taper keeps every quad PLANAR
      # since z' depends on y only). Coincident per-face vertex copies transform
      # identically, so the prism stays geometrically closed — unlike transform
      # (slot) (moves only the tagged face: floating ridge) or a vertex-mode
      # extrude on the massing cap (emitted no side walls at all) — both JUL18
      # broken-edge structural FAILs. Overhangs 3cm, sinks 2cm into the massing
      # top (eave read, never coplanar; crossing is VET-allowed kit-bashing).
      # gid: roof PLANES (|N.x| small, pre-taper orientation is equivalent) ->
      # ROOF; gable-end trapezoids stay WALL (plastered gables).
      yb = H - 0.02
      r = self.box(size=0.5)
      r = self.transform(r, scale=(W + 0.06, roof_h, D + 0.06),
                         translate=(0, yb + roof_h * 0.5, 0))
      r = self.gpu_compute(r, _GABLE_KERNEL, kernel="cs_gable_taper",
                           params=[(yb, yb + roof_h, ridge_w / (D + 0.06), 0.0)])
      r = self.face_normals(r)                               # refresh after the taper
      r = self.select(r, S.abs(S.N.x) < 0.7, domain=POLY, op=replace(group(_B_ROOFSEL)))
      roof_part = self.assign_gid(r, gid=GID_ROOF, slot=_B_ROOFSEL)  # gable ENDS stay wall
    else:
      # flat roof: parapet ring (inset collar) + the inner cap dropped inward
      n = self.inset(n, amount=0.10, slot=_B_TOP, mask_inner=isolate(group(_B_ROOFCAP)))
      n = self.extrude_faces(n, distance=-parapet_drop, slot=_B_ROOFCAP)
    n = self.face_normals(n)                                 # hard-surface flat shading
    if roof != "gable":
      n = self.assign_gid(n, gid=GID_ROOF, slot=_B_ROOFCAP)

    # ---- facade shell (the carve gotcha: never CSG the whole building). The massing
    # KEEPS its facade planes as interior BACKING walls (RECIPE DEVIATION, adjudicate:
    # the ratified text says separate/delete them — but delete_faces leaves orphan
    # corner slots in the CSR and MergeMesh fuses a trailing orphan run into the NEXT
    # merged part's first face: the folded 12/20/44-gon fanfold FAIL of the JUL18
    # re-gate; engine seam reported). The carveable SHELL is two PRIMITIVE slab boxes
    # riding 1cm PROUD of the backing planes — welded, closed, correctly wound, so
    # mesh_to_sdf's AUTO probe takes the exact pseudonormal path. NEVER solidify a
    # lone sheet via extrude keep_base for voxelization: keep_base re-closes the
    # footprint with the PARENT'S winding (surface-modeling semantics) -> both big
    # faces point the SAME way -> the voxelizer's sign checkerboards per voxel ->
    # ghost terrace sheets at every voxel plane (the JUL18 stairstep re-gate defect).
    g    = 0.01                                                 # slab air gap: inner face NEVER
    rest = n                                                    # coplanar with the backing wall
    shell = None
    for zs in (D * 0.5 + g + t * 0.5, -(D * 0.5 + g + t * 0.5)):
      sb = self.box(size=0.5)
      sb = self.transform(sb, scale=(W, H, t), translate=(0, H * 0.5, zs))
      shell = sb if shell is None else self.merge(shell, sb)

    # ---- carve: voxelize (EXPLICIT extent) -> subtract the cutter field -> re-mesh.
    # The cutters are MESH boxes voxelized in ONE second mesh_to_sdf — deliberately
    # ZERO SdfEval in this recipe: a baked box-EXPRESSION costs shadlang's backtracking
    # parser ~3.5GB/2s per kernel (and a NESTED SdfExpr union chain is EXPONENTIAL:
    # 3 unions peaked 68GB/63s and OOM-killed every 30GB fleet node, JUL18). The
    # mesh_to_sdf/csg/march kernels are stock FLAT texts — the whole SDF chain
    # cold-parses in ~1GB — and the voxelized cutter is exactly as sharp as the
    # analytic one (same brick resolution bounds both).
    ext = max(W, H, D) + 1.2                                 # cubic brick side; covers the proud
    ctr = (0.0, H * 0.5, 0.0)                                # slabs (+/-(D/2+t)) with margin
    sd  = self.mesh_to_sdf(shell, dim=sdf_dim, extent=ext, center=ctr)
    cut = self.box(size=0.5)                                 # door: FRONT slab only, dipped 6cm
    cut = self.transform(cut, scale=(door_w, door_h + 0.12, t * 3.0),  # below grade so the sill
                         translate=(cols[db], door_h * 0.5 - 0.03, D * 0.5 + g + t * 0.5))  # opens clean
    for (xc, yc) in cells:                                   # windows: punch THROUGH both slabs
      wb = self.box(size=0.5)
      wb = self.transform(wb, scale=(win_w, win_h, 2.0 * D), translate=(xc, yc, 0.0))
      cut = self.merge(cut, wb)                              # tiny meshes; gid moot (never rendered)
    cutn   = self.mesh_to_sdf(cut, dim=sdf_dim, extent=ext, center=ctr)   # same framing as `sd`
    carved = self.sdf_to_mesh(self.csg(sd, cutn, op="subtract"), weld=True)

    # ---- post-carve re-gid (gid did NOT survive the SDF round trip): the reveal
    # tunnels (faces inside the wall band whose normal left the facade axis) + the
    # slab rims become TRIM — window/door surrounds read as dressed stone.
    carved = self.select(carved, (S.abs(S.N.z) < 0.5) & (S.P.y > 0.05) & (S.P.y < H - 0.05),
                         domain=POLY, op=replace(group(_B_REVEAL)))
    carved = self.assign_gid(carved, gid=GID_TRIM, slot=_B_REVEAL)

    # ---- drop the SdfToMesh CAPACITY TAIL before anything CPU-side consumes this
    # mesh: SdfToMesh reports its ALLOCATED capacity (2x live, pow2 — kMaxFaces-
    # bounded) as its counts; the tail is identity-init zero-area tris piled at the
    # origin. The RENDER early-outs at the GPU live count, but merge's CPU concat,
    # the OBJ dump and meshvet all see the tail as real geometry (JUL18: 1M ghost
    # tris + 1.8M origin verts in the dumped cottage). Zero-area select -> delete
    # packs the surviving corners; the deleted faces stay as zero-LENGTH CSR entries
    # every consumer skips (the delete_demo contract). KNOWN RESIDUE: the VERTEX
    # table keeps carved's capacity count (orphan zero-verts) — compact() can't fix
    # it here (its tight-count readback is 1 eval late; merge latches on the first
    # cascade) — an engine seam (SdfToMesh CPU-count contract), reported upstream.
    carved = self.select(carved, S.area < 1e-9, domain=POLY, op=replace(group(_B_TAIL)))
    carved = self.delete_faces(carved, slot=_B_TAIL)

    # ---- reassemble + merge trims (gid SURVIVES merge: gid_a=None preserves, gid_b stamps).
    # MERGE ORDER IS A MEMORY DECISION: MergeMesh CPU-concats its inputs at their REPORTED
    # counts, and SdfToMesh reports its ALLOCATED CAPACITY (2x live, pow2-rounded) — the
    # render early-outs at the GPU live count but the CPU concat cannot. So chain all the
    # SMALL parts first (every intermediate stays tiny) and fold the capacity-inflated
    # carved mesh in exactly ONCE, LAST. (JUL18: carved-first chaining re-copied the
    # inflated mesh through all 17 merges -> multi-GB churn on the 30GB fleet nodes.)
    bld = rest                                               # the intact massing (small)
    for (xc, yc) in cells:                                   # glass panes, one per facade opening,
      for zc in (D * 0.5 + g + t * 0.5, -(D * 0.5 + g + t * 0.5)):  # oversized 8cm to EMBED
        p = self.box(size=0.5)
        p = self.transform(p, scale=(win_w + 0.08, win_h + 0.08, 0.05), translate=(xc, yc, zc))
        bld = self.merge(bld, p, gid_a=None, gid_b=GID_GLASS)
    d = self.box(size=0.5)                                   # door slab (front, mid-wall depth)
    d = self.transform(d, scale=(door_w + 0.10, door_h + 0.06, 0.07),
                       translate=(cols[db], door_h * 0.5 - 0.02, D * 0.5 + g + t * 0.5))
    bld = self.merge(bld, d, gid_a=None, gid_b=GID_DOOR)
    p = self.box(size=0.5)                                   # plinth base course, proud all round
    # the plinth carries a BELOW-GRADE SKIRT (spans -skirt_m..plinth_h): terrain bedding on
    # slopes is the PLINTH's job, never a scatter lift — sinking the whole building billed
    # 15cm to the door's visible height (owner-caught: "lintel at eye level", 2026-07-22).
    p = self.transform(p, scale=(W + 0.16, plinth_h + skirt_m, D + 2.0 * t + 0.16),
                       translate=(0, (plinth_h - skirt_m) * 0.5, 0))  # (doubles as the doorstep)
    bld = self.merge(bld, p, gid_a=None, gid_b=GID_TRIM)
    if roof_part is not None:                                # the gable prism (gids pre-assigned)
      bld = self.merge(bld, roof_part, gid_a=None, gid_b=None)
    bld = self.merge(bld, carved, gid_a=None, gid_b=None)    # the ONE big copy, last

    # ---- LATTICE SNAP + flap cull (vet hardening): marching tets emits um-scale
    # sliver/flap tris where the iso-surface grazes a tet vertex (the JUL18 vet
    # collapse FAIL: um edges + <0.02deg slivers, and deleting them raw leaves um
    # holes). The snap moves every vertex within SNAP_EPS (lattice units) of a brick
    # lattice point EXACTLY onto it -> grazing flaps become EXACT zero-area (culled
    # below) and their hole perimeters become EXACTLY coincident (position-weldable,
    # so the seams close under meshvet's merge and read watertight). Max facet shift
    # = SNAP_EPS * voxel (~66um at dim 128) — invisible. Runs AFTER the final merge:
    # MergeMesh latches its CPU concat on the FIRST cascade, and an upstream
    # gpu_compute is a passthrough on that eval (its kernel compiles one eval late).
    vox = ext / float(sdf_dim)
    org = ctr[1] - ext * 0.5 + vox * 0.5                     # brick lattice phase (all axes;
    bld = self.gpu_compute(bld, _SNAP_KERNEL,                # x/z share it since ctr.x=ctr.z=0)
                           kernel="cs_lattice_snap",
                           params=[(ctr[0] - ext * 0.5 + vox * 0.5, org,
                                    ctr[2] - ext * 0.5 + vox * 0.5, vox),
                                   (1.0e-3, 0.0, 0.0, 0.0)])
    bld = self.select(bld, S.area < 1e-9, domain=POLY, op=replace(group(_B_TAIL)))
    bld = self.delete_faces(bld, slot=_B_TAIL)               # snapped flaps (post-merge: safe)
    self.output(bld)

  def materials(self):
    # standalone viewer/validator preview: one HmMaterial per gid bucket, so the
    # partition is VISIBLE without a scene (scenes bind their own per-gid ptex3d
    # materials — incl. real A2C glass — via drawable_data(materials={...})).
    return [HmMaterial(Solid,                albedo=vec3(0.82, 0.78, 0.70), roughness=0.90),
            HmMaterial(Solid, gid=GID_GLASS, albedo=vec3(0.45, 0.62, 0.70), roughness=0.10, metallic=1.0),
            HmMaterial(Solid, gid=GID_DOOR,  albedo=vec3(0.30, 0.18, 0.10), roughness=0.80),
            HmMaterial(Solid, gid=GID_ROOF,  albedo=vec3(0.45, 0.20, 0.14), roughness=0.95),
            HmMaterial(Solid, gid=GID_TRIM,  albedo=vec3(0.60, 0.57, 0.52), roughness=0.85)]


__all__ = ["Building", "GID_WALL", "GID_GLASS", "GID_DOOR", "GID_ROOF", "GID_TRIM"]
