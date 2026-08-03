#!/usr/bin/env python3
###############################################################################
# SDF clean-remesh gate (GPU node) — the shape-aware remesh + auto-UV slice END TO END:
# a box -> mesh_to_sdf(dim=96) is extracted BOTH ways through the real hypermesh bake
# machinery, and the two are compared:
#   A) sdf_to_mesh          (marching tetrahedra)      -> the UNIFORM-density baseline
#   B) sdf_to_mesh_clean    (openvdb adaptive + xatlas) -> the CLEAN low-poly UV mesh
# We assert (B) has FAR fewer faces than (A) (the whole point) and that (B) carries
# non-degenerate UVs (a positive UV-triangle-area sum — texture-bakeable). The clean
# mesh is dumped to OBJ for inspection; UV area is measured off that OBJ.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # noqa: F401  (core MUST import before lev2)
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.hypermesh import Hypermesh

OBJ = os.environ.get("SDF_CLEAN_OBJ", "/tmp/sdf_clean.obj")
TETS_OBJ = os.environ.get("SDF_TETS_OBJ", "/tmp/sdf_tets.obj")
DIM = 96
SIZE, EXTENT = 1.2, 2.4


def _obj_live_face_count(objpath):
  """LIVE face count off a dumped OBJ: faces with a non-degenerate (positive-area) 3D triangle.
  A GPU-resident-count marching-tets mesh dumps its pow2 CAPACITY (a degenerate zero-area tail
  piled at the origin — the documented SdfToMesh capacity contract); filtering by area gives the
  true live geometry regardless of warm/cold cook-cache state."""
  verts = []
  live = 0
  with open(objpath) as f:
    for line in f:
      if line.startswith("v "):
        p = line.split()
        verts.append((float(p[1]), float(p[2]), float(p[3])))
      elif line.startswith("f "):
        idx = [int(tok.split("/")[0]) - 1 for tok in line.split()[1:]]
        if len(idx) < 3:
          continue
        a, b, c = verts[idx[0]], verts[idx[1]], verts[idx[2]]
        e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        cx = e1[1] * e2[2] - e1[2] * e2[1]
        cy = e1[2] * e2[0] - e1[0] * e2[2]
        cz = e1[0] * e2[1] - e1[1] * e2[0]
        if (cx * cx + cy * cy + cz * cz) > 1e-18:
          live += 1
  return live


def _bake_tets(ctx):
  m = Hypermesh()
  b = m.box(size=0.5)
  b = m.transform(b, scale=(SIZE, SIZE, SIZE))
  sd = m.mesh_to_sdf(b, dim=DIM, extent=EXTENT, center=(0.0, 0.0, 0.0))
  m.output(m.sdf_to_mesh(sd, weld=True))
  mesh = m.materialize(ctx)
  lev2.hypermesh.dump_obj(mesh, ctx, TETS_OBJ)
  return mesh.num_verts, _obj_live_face_count(TETS_OBJ)  # LIVE faces (num_faces is pow2 capacity)


def _bake_clean(ctx):
  m = Hypermesh()
  b = m.box(size=0.5)
  b = m.transform(b, scale=(SIZE, SIZE, SIZE))
  sd = m.mesh_to_sdf(b, dim=DIM, extent=EXTENT, center=(0.0, 0.0, 0.0))
  m.output(m.sdf_to_mesh_clean(sd, adaptivity=0.5, unwrap=True))
  mesh = m.materialize(ctx)
  lev2.hypermesh.dump_obj(mesh, ctx, OBJ)
  return mesh.num_verts, mesh.num_faces


def _uv_area_sum(objpath):
  """Sum |UV triangle area| over every face (fan-triangulated) of the dumped OBJ."""
  vts = []
  faces = []
  with open(objpath) as f:
    for line in f:
      if line.startswith("vt "):
        p = line.split()
        vts.append((float(p[1]), float(p[2])))
      elif line.startswith("f "):
        idx = []
        for tok in line.split()[1:]:
          # f v/vt/vn -> take the vt index (1-based)
          parts = tok.split("/")
          vt = parts[1] if len(parts) > 1 and parts[1] else parts[0]
          idx.append(int(vt) - 1)
        faces.append(idx)
  total = 0.0
  degen = 0
  for face in faces:
    if len(face) < 3:
      continue
    a = vts[face[0]]
    face_area = 0.0
    for k in range(1, len(face) - 1):
      b = vts[face[k]]
      c = vts[face[k + 1]]
      face_area += abs((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1])) * 0.5
    total += face_area
    if face_area < 1e-12:
      degen += 1
  return total, len(faces), degen


def _signed_volume(objpath):
  """Sum of signed tetra volumes over the OBJ's triangulated faces. POSITIVE = outward-wound
  (the engine's CCW-front convention). The 2026-07-22 winding bug shipped because the original
  battery was orientation-blind — Euler/watertight/UV all pass on an inside-out mesh."""
  verts = []
  vol6 = 0.0
  with open(objpath) as f:
    for line in f:
      if line.startswith("v "):
        _, x, y, z = line.split()[:4]
        verts.append((float(x), float(y), float(z)))
      elif line.startswith("f "):
        idx = [int(t.split("/")[0]) - 1 for t in line.split()[1:]]
        for k in range(1, len(idx) - 1):  # fan
          a, b, c = verts[idx[0]], verts[idx[k]], verts[idx[k + 1]]
          vol6 += (a[0] * (b[1] * c[2] - b[2] * c[1])
                 - a[1] * (b[0] * c[2] - b[2] * c[0])
                 + a[2] * (b[0] * c[1] - b[1] * c[0]))
  return vol6 / 6.0


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  tets_nv, tets_nf = _bake_tets(ctx)
  print(f"marching-tets baseline: verts={tets_nv} faces={tets_nf}", flush=True)

  clean_nv, clean_nf = _bake_clean(ctx)
  print(f"clean remesh:           verts={clean_nv} faces={clean_nf}  -> {OBJ}", flush=True)

  uv_area, obj_nf, uv_degen = _uv_area_sum(OBJ)
  print(f"clean UV: faces={obj_nf} uv_area_sum={uv_area:.5f} degenerate_uv_faces={uv_degen}", flush=True)

  ezapp.mainThreadEnd()
  ecs.headless_exit()

  # verdicts:
  #  - clean mesh must be produced + FAR smaller than the marching-tets soup (the whole point)
  #  - clean mesh must carry non-degenerate UVs (positive area, <1% degenerate faces)
  reduction = (tets_nf / clean_nf) if clean_nf else 0.0
  ok_count = (clean_nf > 0) and (tets_nf > 0) and (clean_nf * 4 < tets_nf)
  ok_uv    = (uv_area > 1e-4) and (obj_nf > 0) and (uv_degen <= max(1, obj_nf // 100))
  svol   = _signed_volume(OBJ)
  ok_orient = svol > 0.0  # outward winding — the box fixture is ~+1.728 m^3
  ok = ok_count and ok_uv and ok_orient
  print(f"face reduction: {reduction:.1f}x (tets={tets_nf} -> clean={clean_nf})", flush=True)
  print(f"signed volume: {svol:+.4f} m^3 (must be POSITIVE = outward-wound)", flush=True)
  print(f"SDF_CLEAN_REMESH_RESULT={'PASS' if ok else 'FAIL'} "
        f"(reduction_ok={ok_count} uv_ok={ok_uv} orient_ok={ok_orient} uv_area={uv_area:.4f})", flush=True)
  sys.exit(0 if ok else 1)


main()
