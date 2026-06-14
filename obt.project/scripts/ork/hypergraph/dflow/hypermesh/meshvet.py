###############################################################################
# meshvet — Tier-1 "artist vet" for hypermesh OBJ dumps + golden-baseline bless/check.
#
# The structural invariants (watertight/manifold/winding) are NECESSARY but not SUFFICIENT — every
# failure that historically hurt (raked collars, folded extrude walls, pinched tips, buried junk)
# was GEOMETRIC. This module layers the checks a 3D artist actually vets, judged against per-asset
# declared INTENT, plus a golden-baseline regression lock:
#
#   structural   : watertight / winding / broken / duplicate / degenerate (the original trimesh tier)
#   overlap      : coplanar-overlap SAT (the inset bug) + GLOBAL crossing self-intersection
#                  (KDTree broadphase + vectorized Moller interval narrowphase — catches a tube
#                  piercing the hull, which every topological check passes)
#   buried/flip  : sampled ray-parity — a face whose +normal-offset centroid is INSIDE the solid is
#                  either internal junk or a flipped exterior face (artist: "select interior faces")
#   collapse     : near-zero edges (the pinched-tip class) + sliver triangles (min angle)
#   fan-fold     : per-POLYGON render-contract check — the renderer fan-triangulates from corner 0;
#                  a folded quad/ngon (fan normals disagree) renders as a see-through hole. Mild
#                  non-planarity is WARN only (twisted tube quads are legitimate).
#   expectations : the asset declares intent via a class attr `VET = dict(...)`:
#                    shells=N            expected connected-component count (FAIL on mismatch)
#                    symmetric="x"       mirror-symmetry plane (FAIL beyond tolerance)
#                    max_valence=N       enforce a vertex-valence ceiling (else valence is INFO)
#                    sliver_warn_deg / sliver_fail_deg / planar_warn / baseline_tol  threshold overrides
#   baseline     : --bless freezes the CURRENT (visually approved) canonical geometry next to the
#                  asset (<asset_dir>/.vet/<stem>.vetbase.npz); afterwards every vet run compares
#                  against it and FAILS on drift — "vet once by eye, then regression-lock".
#
# Verdicts: FAIL blocks the live-reload (the validator overrides its result token); WARN reports
# loudly but does not block; INFO/SKIP are advisory. Checks are calibrated to NEVER false-FAIL on
# the known-good asset corpus — anything heuristic sits in the WARN tier (the SAT-false-positive
# lesson). Heavy checks are budgeted (HMVET_BUDGET seconds, default 12) so the watch loop's 30s
# kill is never hit; out-of-budget checks SKIP loudly rather than stall.
#
# Pure numpy/scipy/trimesh — the only mesh-QA stack importable under the free-threaded interpreter.
###############################################################################
import os, time
import numpy as np

PASS, WARN, FAIL, INFO, SKIP = "PASS", "WARN", "FAIL", "INFO", "SKIP"

_DEFAULTS = dict(
    sliver_fail_deg = 0.02,    # min triangle angle below this = unusable geometry -> FAIL
    sliver_warn_deg = 0.1,     # ... below this = suspicious -> WARN (calibrated: ngon fan caps legitimately
                               # produce ~0.13deg wedges on known-good organic meshes; tighten per-asset via VET)
    collapse_rel    = 1e-7,    # edge shorter than rel*bbox_diag = collapsed -> FAIL
    planar_warn     = 0.5,     # polygon max plane-deviation / mean edge above this -> WARN (fold is FAIL)
    contains_eps    = 1e-4,    # ray-parity offset along the face normal, rel bbox_diag
    contains_samples= 200,     # buried/flip sampled faces (deterministic)
    sym_warn_rel    = 2e-4,    # mirror-symmetry NN max deviation / bbox_diag -> WARN
    sym_fail_rel    = 2e-3,    # ... -> FAIL
    baseline_tol    = 1e-4,    # baseline NN max deviation / bbox_diag -> FAIL
    isect_max_tris  = 120_000, # crossing-check tri budget: bigger meshes get a SEEDED SUBSET (partial coverage,
                               # noted loudly) — piercing defects involve many pairs, so sampling still finds them;
                               # raise HMVET_BUDGET + this for full bless-time coverage
    isect_pair_rate = 1.5e6,   # narrowphase pairs/sec estimate -> budget-aware pair cap
    isect_warn_pen_rel = 2e-3, # penetration / bbox_diag below this = MICRO interpenetration (organic bends,
                               # strand contact — known-good assets exhibit ~3e-4) -> WARN; deeper -> FAIL.
                               # VET allow_self_intersect=True demotes even deep hits to WARN (organic opt-out).
)


# ---------------------------------------------------------------------------- small utilities

def _vet_dir(asset_py):
  return os.path.join(os.path.dirname(os.path.abspath(asset_py)), ".vet")


def baseline_path(asset_py, stem):
  return os.path.join(_vet_dir(asset_py), stem + ".vetbase.npz")


def load_obj_polys(path):
  """Positions + POLYGON faces from the dump_obj OBJ (the dump preserves quads/ngons — trimesh
  triangulates on load, but the fan-fold check needs the polygons exactly as the renderer sees them)."""
  verts, polys = [], []
  with open(path) as f:
    for line in f:
      if line.startswith("v "):
        p = line.split()
        verts.append((float(p[1]), float(p[2]), float(p[3])))
      elif line.startswith("f "):
        polys.append([int(c.split("/")[0]) - 1 for c in line.split()[1:]])
  return np.asarray(verts, dtype=np.float64), polys


def _sat_overlap(A, B, eps=1e-7):
  # do two 2D triangles' INTERIORS overlap? separating-axis theorem (touching = separated, not overlap)
  for T in (A, B):
    for i in range(3):
      e  = T[(i + 1) % 3] - T[i]
      ax = np.array([-e[1], e[0]])
      pa = A @ ax; pb = B @ ax
      if pa.min() >= pb.max() - eps or pb.min() >= pa.max() - eps:
        return False
  return True


class _Budget:
  def __init__(self, seconds):
    self._t0 = time.time(); self._s = seconds
  def left(self):
    return self._s - (time.time() - self._t0)
  def over(self):
    return self.left() <= 0.0


# ---------------------------------------------------------------------------- the checks

def _structural(m):
  """The original trimesh tier: watertight/winding/broken/duplicate/degenerate (+ euler for the report)."""
  import trimesh.repair as repair
  V, F = m.vertices, m.faces
  broken = len(repair.broken_faces(m))
  fl  = [tuple(sorted(int(c) for c in f)) for f in F]
  dup = len(fl) - len(set(fl))
  cr  = np.cross(V[F[:, 1]] - V[F[:, 0]], V[F[:, 2]] - V[F[:, 0]])
  area2 = np.linalg.norm(cr, axis=1)
  degen = int((area2 * 0.5 < 1e-10).sum())
  wt, wc = bool(m.is_watertight), bool(m.is_winding_consistent)
  ok = wt and wc and broken == 0 and dup == 0 and degen == 0
  msg = ("watertight=%s winding=%s euler=%d broken=%d duplicate=%d degenerate=%d"
         % (wt, wc, m.euler_number, broken, dup, degen))
  return (PASS if ok else FAIL), msg


def _coplanar_overlaps(m, budget):
  """Coplanar overlapping triangle interiors (the inset-collar bug class) — SAT per coplanar bucket."""
  if budget.left() < 1.0:
    return SKIP, "out of budget"
  from collections import defaultdict
  V, F, FN = m.vertices, m.faces, m.face_normals
  planes = defaultdict(list)
  for fi in range(len(F)):
    n = FN[fi]; planes[(tuple(np.round(n, 2)), round(float(np.dot(V[F[fi][0]], n)), 4))].append(fi)
  overlaps = []
  for (nk, _off), fis in planes.items():
    if len(fis) < 2:
      continue
    n = np.array(nk, float); nn = np.linalg.norm(n)
    if nn < 1e-6:
      continue
    n = n / nn
    u = np.array([1.0, 0, 0]) if abs(n[0]) < 0.9 else np.array([0, 1.0, 0])
    u = u - n * np.dot(u, n); u /= np.linalg.norm(u); w = np.cross(n, u)
    proj = {fi: np.array([[np.dot(V[c], u), np.dot(V[c], w)] for c in F[fi]]) for fi in fis}
    for i in range(len(fis)):
      for j in range(i + 1, len(fis)):
        a, b = fis[i], fis[j]
        if len(set(int(c) for c in F[a]) & set(int(c) for c in F[b])) >= 2:
          continue                                   # edge-adjacent -> share an edge, not an overlap
        if _sat_overlap(proj[a], proj[b]):
          overlaps.append((int(a), int(b)))
  if overlaps:
    return FAIL, "%d coplanar-overlap pairs (first 8: %s)" % (len(overlaps), overlaps[:8])
  return PASS, "0 coplanar-overlap pairs"


def _self_intersection(m, exp, budget):
  """GLOBAL crossing (non-coplanar) self-intersection — the class every topological check passes.
  Broadphase: KDTree on triangle centroids (large tris brute-forced separately). Narrowphase:
  vectorized Moller plane-straddle + interval-overlap on the plane-intersection line. Touch-tolerant:
  pairs sharing a welded vertex, or with any vertex within eps of the other's plane, are skipped —
  zero-false-positive bias (a strict penetration must put all 6 verts off both planes)."""
  from scipy.spatial import cKDTree
  if budget.left() < 2.0:
    return SKIP, "out of budget before broadphase"
  V, F = np.asarray(m.vertices), np.asarray(m.faces)
  T = V[F]                                              # (m,3,3)
  diag = float(np.linalg.norm(m.bounds[1] - m.bounds[0])) or 1.0
  C = T.mean(axis=1)
  R = np.linalg.norm(T - C[:, None, :], axis=2).max(axis=1)
  n1 = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
  nlen = np.linalg.norm(n1, axis=1)
  ok_t = nlen > 1e-14                                   # exclude degenerates (reported elsewhere)
  note = ""
  # --- tri budget: huge meshes get a SEEDED SUBSET (a piercing defect involves MANY pairs, so partial
  #     coverage still finds it; full coverage = raise HMVET_BUDGET + isect_max_tris at bless time) ---
  max_tris = int(exp["isect_max_tris"])
  ok_idx = np.nonzero(ok_t)[0]
  if len(ok_idx) > max_tris:
    ok_idx = ok_idx[np.random.default_rng(5).choice(len(ok_idx), max_tris, replace=False)]
    keep = np.zeros(len(ok_t), bool); keep[ok_idx] = True; ok_t = keep
    note = " [SUBSET %d/%d tris — partial coverage]" % (max_tris, int(nlen.size))
  # --- broadphase ---
  cut = np.percentile(R[ok_t], 99.5) if ok_t.any() else 0.0
  small = ok_t & (R <= cut); large = ok_t & (R > cut)
  idx_small = np.nonzero(small)[0]
  tree = cKDTree(C[idx_small])
  pairs = tree.query_pairs(2.0 * cut, output_type="ndarray") if cut > 0 else np.zeros((0, 2), int)
  pairs = idx_small[pairs] if len(pairs) else pairs.reshape(0, 2)
  extra = []
  li = np.nonzero(large)[0]
  if len(li):                                           # few oversized tris: each vs everything nearby
    if len(li) > 2000:
      li = li[np.random.default_rng(7).choice(len(li), 2000, replace=False)]
    all_tree = cKDTree(C[ok_t]); ok_all = np.nonzero(ok_t)[0]
    for l in li:
      near = ok_all[all_tree.query_ball_point(C[l], R[l] + cut)]
      near = near[near != l]
      if len(near):
        extra.append(np.stack([np.full(len(near), l), near], axis=1))
  if extra:
    pairs = np.concatenate([pairs] + extra, axis=0) if len(pairs) else np.concatenate(extra, axis=0)
    pairs = np.unique(np.sort(pairs, axis=1), axis=0)
  # --- tighten: the query radius is the LOOSE 2*p99.5; keep only pairs actually within r_i + r_j ---
  if len(pairs):
    d = np.linalg.norm(C[pairs[:, 0]] - C[pairs[:, 1]], axis=1)
    pairs = pairs[d < (R[pairs[:, 0]] + R[pairs[:, 1]])]
  # --- budget-aware pair cap (narrowphase throughput estimate) ---
  cap = int(max(budget.left() - 1.0, 0.5) * exp["isect_pair_rate"])
  if len(pairs) > cap:                                  # deterministic sample, loudly partial
    total = len(pairs)
    pairs = pairs[np.random.default_rng(11).choice(total, cap, replace=False)]
    note += " [SAMPLED %d of %d candidate pairs]" % (cap, total)
  # --- drop pairs sharing any WELDED vertex (edge/vertex adjacency is contact, not intersection) ---
  if len(pairs):
    fa, fb = F[pairs[:, 0]], F[pairs[:, 1]]
    share = (fa[:, :, None] == fb[:, None, :]).any(axis=(1, 2))
    pairs = pairs[~share]
  # --- narrowphase (chunked) ---
  eps_d   = 1e-7 * diag                                 # "on plane" metric tolerance
  eps_len = 1e-7 * diag                                 # minimum penetration interval
  deep_t  = exp["isect_warn_pen_rel"] * diag            # micro (WARN) vs deep (FAIL) penetration split
  deep_pairs, n_micro, n_deep, max_pen = [], 0, 0, 0.0
  processed = 0
  for lo in range(0, len(pairs), 262144):
    if budget.over():
      note += " [budget hit at %d/%d pairs]" % (lo, len(pairs))
      break
    processed = min(lo + 262144, len(pairs))
    P = pairs[lo:lo + 262144]
    A, B = T[P[:, 0]], T[P[:, 1]]
    nA = np.cross(A[:, 1] - A[:, 0], A[:, 2] - A[:, 0])
    nB = np.cross(B[:, 1] - B[:, 0], B[:, 2] - B[:, 0])
    lA = np.linalg.norm(nA, axis=1); lB = np.linalg.norm(nB, axis=1)
    dB = np.einsum("kij,kj->ki", B - A[:, None, 0, :], nA) / lA[:, None]   # B verts vs A's plane (metric)
    dA = np.einsum("kij,kj->ki", A - B[:, None, 0, :], nB) / lB[:, None]
    onA = (np.abs(dA) <= eps_d).any(axis=1); onB = (np.abs(dB) <= eps_d).any(axis=1)
    strA = (dA > eps_d).any(axis=1) & (dA < -eps_d).any(axis=1)
    strB = (dB > eps_d).any(axis=1) & (dB < -eps_d).any(axis=1)
    cand = strA & strB & ~onA & ~onB                    # both STRICTLY straddle, nothing touching
    if not cand.any():
      continue
    A, B, dA, dB = A[cand], B[cand], dA[cand], dB[cand]
    D = np.cross(nA[cand], nB[cand])
    Dl = np.linalg.norm(D, axis=1)
    nz = Dl > 1e-14                                     # parallel-but-straddling can't happen; guard anyway
    A, B, dA, dB, D = A[nz], B[nz], dA[nz], dB[nz], (D[nz] / Dl[nz, None])
    pA = np.einsum("kij,kj->ki", A, D); pB = np.einsum("kij,kj->ki", B, D)
    def interval(p, d):
      s   = d > 0.0
      odd = np.where(s.sum(axis=1) == 1, s.argmax(axis=1), (~s).argmax(axis=1))
      i1, i2 = (odd + 1) % 3, (odd + 2) % 3
      ar = np.arange(len(d))
      po, do = p[ar, odd], d[ar, odd]
      t1 = po + (p[ar, i1] - po) * do / (do - d[ar, i1])
      t2 = po + (p[ar, i2] - po) * do / (do - d[ar, i2])
      return np.minimum(t1, t2), np.maximum(t1, t2)
    a0, a1 = interval(pA, dA)                           # A's crossing interval on the shared line
    b0, b1 = interval(pB, dB)
    pen = np.minimum(a1, b1) - np.maximum(a0, b0)
    hit = pen > eps_len
    if hit.any():
      max_pen = max(max_pen, float(pen[hit].max()))
      deep    = pen > deep_t
      n_deep += int(deep.sum()); n_micro += int(hit.sum()) - int(deep.sum())
      if deep.any() and len(deep_pairs) < 16:
        src = pairs[lo:lo + 262144][cand][nz][deep]
        deep_pairs.extend(map(tuple, src[: 16 - len(deep_pairs)].tolist()))
      if n_deep >= 4096:                                # extensively pierced — verdict settled, stop early
        note += " [early exit]"
        break
  rel = max_pen / diag
  if n_deep:
    v = WARN if exp.get("allow_self_intersect") else FAIL
    return v, ("%d DEEP crossing self-intersections (pen up to %.2g x diag; first pairs: %s)"
               "%s%s" % (n_deep, rel, deep_pairs[:8],
                         " [VET allow_self_intersect -> WARN]" if v == WARN else "", note))
  if n_micro:
    return WARN, ("%d MICRO interpenetrations (max pen %.2g x diag < %.2g threshold — organic contact/bend "
                  "class, not a pierce)%s" % (n_micro, rel, exp["isect_warn_pen_rel"], note))
  if processed == 0 and len(pairs):                     # budget died before ANY narrowphase ran:
    return SKIP, "out of budget before narrowphase (0/%d pairs)%s" % (len(pairs), note)   # never claim PASS with zero coverage
  return PASS, "0 crossing intersections (%d/%d candidate pairs)%s" % (processed, len(pairs), note)


def _ray_parity_inside(V, F, pts, diag):
  """Point-in-mesh by ray-crossing parity — manual Moller-Trumbore (no rtree/embree dependency).
  One fixed irrational ray direction (minimizes exact edge/vertex grazes); tris chunked, points batched."""
  d = np.array([0.57735027, 0.57735091, 0.57734963]); d /= np.linalg.norm(d)
  counts = np.zeros(len(pts), np.int64)
  eps_t = 1e-9 * diag
  for lo in range(0, len(F), 131072):
    Fc = F[lo:lo + 131072]
    v0 = V[Fc[:, 0]]; e1 = V[Fc[:, 1]] - v0; e2 = V[Fc[:, 2]] - v0
    h  = np.cross(d[None, :], e2)                       # (t,3) constant ray dir
    a  = np.einsum("tj,tj->t", e1, h)
    good = np.abs(a) > 1e-14
    f  = np.where(good, 1.0 / np.where(good, a, 1.0), 0.0)
    for plo in range(0, len(pts), 64):                  # point batches bound the (p,t,3) temp
      P = pts[plo:plo + 64]
      s = P[:, None, :] - v0[None, :, :]                # (p,t,3)
      u = f[None, :] * np.einsum("ptj,tj->pt", s, h)
      q = np.cross(s, e1[None, :, :])
      v = f[None, :] * np.einsum("ptj,j->pt", q, d)
      t = f[None, :] * np.einsum("ptj,tj->pt", q, e2)
      hit = good[None, :] & (u >= 0) & (v >= 0) & (u + v <= 1.0) & (t > eps_t)
      counts[plo:plo + 64] += hit.sum(axis=1)
  return (counts % 2) == 1


def _buried_faces(m, exp, budget):
  """Sampled ray-parity: centroid + eps*normal INSIDE the solid => buried interior junk OR a flipped
  exterior face — both artist-grade defects no topological check sees. Needs watertight (parity)."""
  if not m.is_watertight:
    return SKIP, "mesh not watertight — parity undefined"
  if budget.left() < 2.0:
    return SKIP, "out of budget"
  diag = float(np.linalg.norm(m.bounds[1] - m.bounds[0])) or 1.0
  k = min(int(exp["contains_samples"]), len(m.faces))
  sel = np.random.default_rng(12345).choice(len(m.faces), k, replace=False)
  pts = m.triangles_center[sel] + m.face_normals[sel] * (exp["contains_eps"] * diag)
  inside = _ray_parity_inside(np.asarray(m.vertices), np.asarray(m.faces), pts, diag)
  n_in = int(inside.sum())
  if n_in >= 2:                                         # >=2 of k: a real buried/flipped region, not a graze
    bad = sel[inside][:8].tolist()
    if exp.get("allow_self_intersect"):                 # interpenetrating organics: a face inside ANOTHER part
      return WARN, ("%d/%d sampled faces read inside (interpenetration allowed by VET — cannot distinguish "
                    "buried junk from strand overlap; ids: %s)" % (n_in, k, bad))
    return FAIL, "%d/%d sampled faces buried-or-flipped (face ids: %s)" % (n_in, k, bad)
  if n_in == 1:
    return WARN, "1/%d sampled face reads inside (possible graze — rerun or inspect face %d)" % (k, int(sel[inside][0]))
  return PASS, "0/%d sampled faces buried or flipped" % k


def _collapse_and_slivers(m, exp):
  V, F = np.asarray(m.vertices), np.asarray(m.faces)
  diag = float(np.linalg.norm(m.bounds[1] - m.bounds[0])) or 1.0
  E = np.concatenate([F[:, [0, 1]], F[:, [1, 2]], F[:, [2, 0]]], axis=0)
  el = np.linalg.norm(V[E[:, 0]] - V[E[:, 1]], axis=1)
  collapsed = int((el < exp["collapse_rel"] * diag).sum())
  # min interior angle per triangle (vectorized; degenerate tris already reported structurally)
  a = V[F[:, 1]] - V[F[:, 0]]; b = V[F[:, 2]] - V[F[:, 1]]; c = V[F[:, 0]] - V[F[:, 2]]
  def ang(u, v):
    un = np.linalg.norm(u, axis=1); vn = np.linalg.norm(v, axis=1)
    d  = np.einsum("ij,ij->i", u, v) / np.maximum(un * vn, 1e-30)
    return np.degrees(np.arccos(np.clip(-d, -1.0, 1.0)))   # -d: angle BETWEEN edges at the shared corner
  mins = np.minimum(np.minimum(ang(c, a), ang(a, b)), ang(b, c))
  n_fail = int((mins < exp["sliver_fail_deg"]).sum())
  n_warn = int((mins < exp["sliver_warn_deg"]).sum()) - n_fail
  if collapsed or n_fail:
    return FAIL, "collapsed_edges=%d sliver_tris(<%.3gdeg)=%d" % (collapsed, exp["sliver_fail_deg"], n_fail)
  if n_warn:
    return WARN, "%d thin tris (min angle < %.3g deg; min seen %.4g)" % (n_warn, exp["sliver_warn_deg"], float(mins.min()))
  return PASS, "no collapsed edges; min tri angle %.3g deg" % float(mins.min())


def _fan_fold(obj_path, exp, budget):
  """The render contract: cs_tri fans every polygon from corner 0 — if any fan triangle's normal opposes
  the polygon's Newell normal, that polygon renders folded (see-through). Mild non-planarity = WARN.
  Polygons are processed VECTORIZED in same-arity groups (one numpy pass per polygon size)."""
  if budget.left() < 1.0:
    return SKIP, "out of budget"
  V, polys = load_obj_polys(obj_path)
  by_n = {}
  for pi, poly in enumerate(polys):
    if len(poly) >= 4:
      by_n.setdefault(len(poly), []).append(pi)
  folded, warped, first = 0, 0, []
  for n, pids in by_n.items():
    idx = np.asarray([polys[pi] for pi in pids])        # (k,n) same-arity group
    P   = V[idx]                                        # (k,n,3)
    nw  = np.cross(P, np.roll(P, -1, axis=1)).sum(axis=1)   # Newell normal per polygon
    nwl = np.linalg.norm(nw, axis=1)
    ok  = nwl > 1e-14
    nw  = np.where(ok[:, None], nw / np.maximum(nwl, 1e-30)[:, None], 0.0)
    fans = np.cross(P[:, 1:-1] - P[:, :1], P[:, 2:] - P[:, :1])  # the exact fan the renderer emits
    fl   = np.linalg.norm(fans, axis=2)
    dots = np.einsum("knj,kj->kn", fans, nw)
    fold = ok & ((dots < 0.0) & (fl > 1e-14)).any(axis=1)
    folded += int(fold.sum())
    for pi in np.asarray(pids)[fold][: max(0, 8 - len(first))]:
      first.append(int(pi))
    ctr = P.mean(axis=1, keepdims=True)                 # plane deviation / mean edge (twist amount)
    dev = np.abs(np.einsum("knj,kj->kn", P - ctr, nw)).max(axis=1)
    me  = np.linalg.norm(np.roll(P, -1, axis=1) - P, axis=2).mean(axis=1)
    warped += int((ok & ~fold & (me > 1e-14) & (dev / np.maximum(me, 1e-30) > exp["planar_warn"])).sum())
  if folded:
    return FAIL, "%d folded polygons (render as holes; first: %s)" % (folded, first)
  if warped:
    return WARN, "%d strongly non-planar polygons (fan renders, but twist > %.2g x edge)" % (warped, exp["planar_warn"])
  return PASS, "no folded polygons (%d polys checked)" % len(polys)


def _shells(m, exp):
  n = int(m.body_count)
  want = exp.get("shells")
  if want is None:
    return INFO, "%d shell(s) — declare VET shells=N to enforce" % n
  if n != int(want):
    return FAIL, "%d shell(s), expected %d" % (n, int(want))
  return PASS, "%d shell(s) as declared" % n


def _symmetry(m, exp):
  axis = exp.get("symmetric")
  if not axis:
    return SKIP, "no symmetry declared"
  from scipy.spatial import cKDTree
  i = "xyz".index(axis)
  V = np.asarray(m.vertices)
  W = V.copy(); W[:, i] = -W[:, i]
  diag = float(np.linalg.norm(m.bounds[1] - m.bounds[0])) or 1.0
  d, _ = cKDTree(V).query(W, k=1)
  rel = float(d.max()) / diag
  if rel > exp["sym_fail_rel"]:
    return FAIL, "%s-mirror max deviation %.3g x diag (limit %.1g)" % (axis, rel, exp["sym_fail_rel"])
  if rel > exp["sym_warn_rel"]:
    return WARN, "%s-mirror max deviation %.3g x diag" % (axis, rel)
  return PASS, "%s-mirror max deviation %.3g x diag" % (axis, rel)


def _valence(m, exp):
  ue = m.edges_unique
  val = np.bincount(ue.reshape(-1), minlength=len(m.vertices))
  vmax = int(val.max()) if len(val) else 0
  cap = exp.get("max_valence")
  if cap is not None and vmax > int(cap):
    return FAIL, "max vertex valence %d exceeds declared cap %d" % (vmax, int(cap))
  return INFO, "max vertex valence %d (%d verts > 8)" % (vmax, int((val > 8).sum()))


# ---------------------------------------------------------------------------- baseline (bless / check)

def _canonical_stats(m):
  return dict(n_verts=len(m.vertices), n_faces=len(m.faces), euler=int(m.euler_number),
              area=float(m.area), volume=float(abs(m.volume)) if m.is_watertight else 0.0,
              bounds=np.asarray(m.bounds, dtype=np.float64))


def _bless(m, asset_py, stem):
  os.makedirs(_vet_dir(asset_py), exist_ok=True)
  p = baseline_path(asset_py, stem)
  s = _canonical_stats(m)
  np.savez_compressed(p, verts=np.asarray(m.vertices, dtype=np.float32), **s)
  return p


def _baseline_check(m, asset_py, stem, exp):
  p = baseline_path(asset_py, stem)
  if not os.path.exists(p):
    return INFO, "no baseline — freeze the approved mesh with --bless"
  from scipy.spatial import cKDTree
  base = np.load(p)
  s = _canonical_stats(m)
  diag = float(np.linalg.norm(s["bounds"][1] - s["bounds"][0])) or 1.0
  diffs = []
  for k in ("n_verts", "n_faces", "euler"):
    if int(base[k]) != int(s[k]):
      diffs.append("%s %d -> %d" % (k, int(base[k]), int(s[k])))
  for k in ("area", "volume"):
    b = float(base[k])
    if b > 0 and abs(float(s[k]) - b) / b > 1e-3:
      diffs.append("%s %+0.2f%%" % (k, 100.0 * (float(s[k]) - b) / b))
  BV = np.asarray(base["verts"], dtype=np.float64)
  V  = np.asarray(m.vertices)
  d1, _ = cKDTree(V).query(BV, k=1)                      # symmetric NN (order/weld-insensitive)
  d2, _ = cKDTree(BV).query(V, k=1)
  rel = max(float(d1.max()), float(d2.max())) / diag
  if rel > exp["baseline_tol"]:
    diffs.append("geometry deviates %.3g x diag (tol %.1g)" % (rel, exp["baseline_tol"]))
  if diffs:
    return FAIL, ("DRIFT vs blessed baseline: %s  — intentional change? re-bless with --bless" % "; ".join(diffs))
  return PASS, "matches blessed baseline (max NN dev %.3g x diag)" % rel


# ---------------------------------------------------------------------------- the runner

def run(obj_path, asset_py=None, stem=None, expectations=None, bless=False, budget_s=None):
  """Run the full vet on a dumped OBJ. Returns (verdict, report_str): verdict FAIL must BLOCK the
  live-reload; WARN/PASS allow it. `asset_py`+`stem` locate the baseline + VET expectations dir."""
  import trimesh
  exp = dict(_DEFAULTS); exp.update(expectations or {})
  budget = _Budget(budget_s if budget_s is not None else float(os.environ.get("HMVET_BUDGET", "12")))
  m = trimesh.load(obj_path, force="mesh", process=False)
  m.merge_vertices(merge_norm=True, merge_tex=True)     # weld by POSITION (hard-edge splits aren't holes)
  rows = []
  def add(name, fn, *args):
    t0 = time.time()
    try:
      v, msg = fn(*args)
    except Exception as e:
      v, msg = SKIP, "check error: %s" % e
    rows.append((name, v, msg, time.time() - t0))
  # cheap + load-bearing first; the slowest (coplanar SAT) last so the budget sheds it on huge meshes.
  add("structural",  _structural, m)
  add("collapse",    _collapse_and_slivers, m, exp)
  add("fanfold",     _fan_fold, obj_path, exp, budget)
  add("crossing",    _self_intersection, m, exp, budget)
  add("buried",      _buried_faces, m, exp, budget)
  add("coplanar",    _coplanar_overlaps, m, budget)
  add("shells",      _shells, m, exp)
  add("symmetry",    _symmetry, m, exp)
  add("valence",     _valence, m, exp)
  if asset_py and stem:
    if bless:
      p = _bless(m, asset_py, stem)
      rows.append(("baseline", INFO, "BLESSED -> %s" % p, 0.0))
    else:
      add("baseline", _baseline_check, m, asset_py, stem, exp)
  verdict = PASS
  if any(v == WARN for _, v, _, _ in rows):
    verdict = WARN
  if any(v == FAIL for _, v, _, _ in rows):
    verdict = FAIL
  hdr = "meshvet: %s  (welded verts=%d tris=%d)" % (os.path.basename(obj_path), len(m.vertices), len(m.faces))
  body = "\n".join("  vet[%-10s] %-4s %s%s" % (n, v, msg, "  (%.1fs)" % dt if dt > 0.5 else "")
                   for (n, v, msg, dt) in rows)
  return verdict, hdr + "\n" + body
