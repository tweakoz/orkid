#!/bin/sh
""":"
# trampoline: exec the first python3 that has numpy+trimesh (mesh vet needs both).
for _py in \
    "$OBT_STAGE/pyvenv/bin/python3" \
    "$HOME"/.staging*/pyvenv/bin/python3 \
    "$(command -v python3)"; do
    [ -x "$_py" ] && "$_py" -c 'import numpy, trimesh' 2>/dev/null && exec "$_py" "$0" "$@"
done
echo "ork.vet.mesh: no python3 with numpy+trimesh found on this host" >&2
exit 3
":"""
"""
Mesh vet (OBJ via trimesh): diagnose from the MESH, not a render
(feedback_read_objs_not_renders). Porcelain verdicts:

  - topology: watertight, winding-consistency, connected-component count class
  - degenerate-triangle count/fraction (collapsed / zero-area faces)
  - duplicate-vertex fraction (unwelded seams)
  - self-intersection sample (rtree-free spatial hash + segment-triangle test)
  - buried-face heuristic (face whose outward side is INSIDE the solid, via a
    pure-numpy generalized-winding-number point-in-mesh test)
  - bbox / volume sanity, optionally gated against a reference
  - A/B: pass two OBJs -> metric deltas emitted as gated checks

Loaded with process=False so raw topology defects (unwelded verts, degenerate
faces) survive to be measured.
"""
import sys
import os
import argparse

import numpy as np
import trimesh


def load_mesh(path):
    m = trimesh.load(path, process=False, force='mesh')
    if not isinstance(m, trimesh.Trimesh):
        raise RuntimeError(f"'{path}' did not load as a single triangle mesh (got {type(m).__name__})")
    return m


def degenerate_fraction(m):
    areas = m.area_faces
    med = float(np.median(areas)) if len(areas) else 0.0
    atol = max(1e-14, 1e-8 * med)
    n = int((areas <= atol).sum())
    return n, (float(n) / len(areas) if len(areas) else 0.0)


def duplicate_vertex_fraction(m):
    if len(m.faces) == 0:
        return 0, 0.0
    used = np.unique(m.faces.ravel())
    coords = np.asarray(m.vertices)[used]
    diag = float(np.linalg.norm(coords.max(0) - coords.min(0))) or 1.0
    quant = np.round(coords / (1e-6 * diag)).astype(np.int64)
    n_unique = len(np.unique(quant, axis=0))
    dup = len(used) - n_unique
    return int(dup), float(dup) / len(used)


def _winding_inside(verts, faces, pts):
    """Generalized winding number (van Oosterom-Strackee solid angle); ~1 inside, ~0 outside."""
    tri = verts[faces]
    out = np.empty(len(pts))
    for i, p in enumerate(pts):
        a, b, c = tri[:, 0, :] - p, tri[:, 1, :] - p, tri[:, 2, :] - p
        la = np.linalg.norm(a, axis=1)
        lb = np.linalg.norm(b, axis=1)
        lc = np.linalg.norm(c, axis=1)
        num = np.einsum('ij,ij->i', a, np.cross(b, c))
        den = (la * lb * lc + np.einsum('ij,ij->i', a, b) * lc
               + np.einsum('ij,ij->i', b, c) * la + np.einsum('ij,ij->i', c, a) * lb)
        out[i] = (2.0 * np.arctan2(num, den)).sum()
    return out / (4.0 * np.pi)


def buried_face_fraction(m, sample=96, seed=0):
    """Fraction of sampled faces whose outward side lies INSIDE the solid volume."""
    V = np.asarray(m.vertices, dtype=np.float64)
    F = np.asarray(m.faces)
    if len(F) == 0:
        return 0.0, 0
    rng = np.random.default_rng(seed)
    idx = rng.choice(len(F), size=min(sample, len(F)), replace=False)
    cent = V[F[idx]].mean(axis=1)
    normals = np.asarray(m.face_normals)[idx]
    diag = float(np.linalg.norm(V.max(0) - V.min(0))) or 1.0
    probe = cent + normals * (1e-3 * diag)
    w = _winding_inside(V, F, probe)
    buried = int((np.abs(w) > 0.5).sum())
    return float(buried) / len(idx), int(len(idx))


def _seg_tri_hit(p0, p1, v0, v1, v2, eps=1e-9):
    d = p1 - p0
    e1, e2 = v1 - v0, v2 - v0
    h = np.cross(d, e2)
    a = np.dot(e1, h)
    if abs(a) < eps:
        return False
    f = 1.0 / a
    s = p0 - v0
    u = f * np.dot(s, h)
    if u < -eps or u > 1 + eps:
        return False
    q = np.cross(s, e1)
    v = f * np.dot(d, q)
    if v < -eps or u + v > 1 + eps:
        return False
    t = f * np.dot(e2, q)
    return eps < t < 1 - eps


def _tris_intersect(T1, T2):
    for i in range(3):
        if _seg_tri_hit(T1[i], T1[(i + 1) % 3], T2[0], T2[1], T2[2]):
            return True
    for i in range(3):
        if _seg_tri_hit(T2[i], T2[(i + 1) % 3], T1[0], T1[1], T1[2]):
            return True
    return False


def self_intersection_sample(m, budget=500000):
    """rtree-free broadphase (grid hash on AABBs) + exact segment-triangle test.

    Returns (intersecting_pairs, pairs_tested, truncated)."""
    V = np.asarray(m.vertices, dtype=np.float64)
    F = np.asarray(m.faces)
    if len(F) < 2:
        return 0, 0, False
    tri = V[F]
    tmin, tmax = tri.min(1), tri.max(1)
    ext = tmax - tmin
    diag = float(np.linalg.norm(V.max(0) - V.min(0))) or 1.0
    h = max(float(np.median(ext[ext > 0])) if np.any(ext > 0) else diag / 64.0, diag / 512.0)
    cmin = (tmin / h).astype(np.int64)
    cmax = (tmax / h).astype(np.int64)
    grid = {}
    for fi in range(len(F)):
        for cx in range(cmin[fi, 0], cmax[fi, 0] + 1):
            for cy in range(cmin[fi, 1], cmax[fi, 1] + 1):
                for cz in range(cmin[fi, 2], cmax[fi, 2] + 1):
                    grid.setdefault((cx, cy, cz), []).append(fi)
    fverts = [set(f) for f in F]
    tested = set()
    hits = 0
    n_tested = 0
    truncated = False
    for members in grid.values():
        for ai in range(len(members)):
            for bi in range(ai + 1, len(members)):
                i, j = members[ai], members[bi]
                if i > j:
                    i, j = j, i
                if (i, j) in tested:
                    continue
                if fverts[i] & fverts[j]:
                    continue  # adjacent faces legitimately touch
                if tmin[i, 0] > tmax[j, 0] or tmax[i, 0] < tmin[j, 0] or \
                   tmin[i, 1] > tmax[j, 1] or tmax[i, 1] < tmin[j, 1] or \
                   tmin[i, 2] > tmax[j, 2] or tmax[i, 2] < tmin[j, 2]:
                    continue
                tested.add((i, j))
                n_tested += 1
                if n_tested > budget:
                    truncated = True
                    return hits, n_tested, truncated
                if _tris_intersect(tri[i], tri[j]):
                    hits += 1
    return hits, n_tested, truncated


def emit_metrics(r, m, prefix, ref_volume=None, ref_tol=0.05):
    r.info(f'{prefix}.vertices', len(m.vertices))
    r.info(f'{prefix}.faces', len(m.faces))
    r.info(f'{prefix}.components', m.body_count)
    bbox = m.bounds
    diag = float(np.linalg.norm(bbox[1] - bbox[0]))
    r.info(f'{prefix}.bbox_diag', f"{diag:.5f}")
    try:
        vol = float(m.volume)
    except Exception:
        vol = float('nan')
    r.info(f'{prefix}.volume', f"{vol:.5f}")
    return vol, diag


def main():
    import _ork_vet_common as vet
    p = argparse.ArgumentParser(description='Vet an OBJ mesh (trimesh); porcelain verdicts')
    p.add_argument('mesh')
    p.add_argument('other', nargs='?', help='second OBJ for A/B metric-delta comparison')
    p.add_argument('--ref-volume', type=float, help='expected volume (sanity gate)')
    p.add_argument('--vol-tol', type=float, default=0.05, help='volume ratio tolerance for --ref-volume / A/B')
    p.add_argument('--max-degenerate-frac', type=float, default=0.001)
    p.add_argument('--max-duplicate-frac', type=float, default=0.001)
    p.add_argument('--max-buried-frac', type=float, default=0.02)
    p.add_argument('--isect-budget', type=int, default=500000)
    args = p.parse_args()

    m = load_mesh(args.mesh)
    r = vet.Report()

    vol, diag = emit_metrics(r, m, 'mesh', args.ref_volume, args.vol_tol)
    r.gate('topo.watertight', m.is_watertight, '==True', bool(m.is_watertight), warn_only=True)
    r.gate('topo.winding_consistent', m.is_winding_consistent, '==True',
           bool(m.is_winding_consistent), warn_only=True)

    dcount, dfrac = degenerate_fraction(m)
    r.info('mesh.degenerate_count', dcount)
    r.gate('mesh.degenerate_frac', f"{dfrac:.5f}", f"<{args.max_degenerate_frac:g}",
           dfrac < args.max_degenerate_frac)

    ucount, ufrac = duplicate_vertex_fraction(m)
    r.info('mesh.duplicate_count', ucount)
    r.gate('mesh.duplicate_vertex_frac', f"{ufrac:.5f}", f"<{args.max_duplicate_frac:g}",
           ufrac < args.max_duplicate_frac)

    hits, tested, trunc = self_intersection_sample(m, args.isect_budget)
    r.info('mesh.isect_pairs_tested', f"{tested}{'+trunc' if trunc else ''}")
    r.gate('mesh.selfintersect_pairs', hits, '==0', hits == 0)

    bfrac, bsample = buried_face_fraction(m)
    r.info('mesh.buried_sample', bsample)
    r.gate('mesh.buried_face_frac', f"{bfrac:.4f}", f"<{args.max_buried_frac:g}",
           bfrac < args.max_buried_frac)

    if args.ref_volume is not None and np.isfinite(vol):
        ratio = vol / args.ref_volume if args.ref_volume else float('inf')
        r.gate('mesh.volume_ratio', f"{ratio:.4f}", f"1+/-{args.vol_tol:g}",
               abs(ratio - 1.0) <= args.vol_tol)

    if args.other:
        m2 = load_mesh(args.other)
        vol2, diag2 = emit_metrics(r, m2, 'other')
        r.gate('ab.vertex_count_equal', f"{len(m.vertices)} vs {len(m2.vertices)}", 'equal',
               len(m.vertices) == len(m2.vertices))
        r.gate('ab.face_count_equal', f"{len(m.faces)} vs {len(m2.faces)}", 'equal',
               len(m.faces) == len(m2.faces))
        dr = (diag2 / diag) if diag else float('inf')
        r.gate('ab.bbox_diag_ratio', f"{dr:.4f}", f"1+/-{args.vol_tol:g}",
               abs(dr - 1.0) <= args.vol_tol)
        if np.isfinite(vol) and np.isfinite(vol2) and vol:
            vr = vol2 / vol
            r.gate('ab.volume_ratio', f"{vr:.4f}", f"1+/-{args.vol_tol:g}",
                   abs(vr - 1.0) <= args.vol_tol)

    n_fail = r.emit()
    sys.exit(1 if n_fail else 0)


if __name__ == '__main__':
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    main()
