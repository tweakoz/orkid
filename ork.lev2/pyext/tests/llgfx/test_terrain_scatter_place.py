#!/usr/bin/env python3
###############################################################################
# ScatterPlaceModule gate — the IN-GRAPH scatter placement + cut-and-fill building
# PAD family (T.scatter_place). One deterministic CPU module owns placement + pad
# emission against its PRE-flatten input fields; stock MaskBlend flattens the height
# under each footprint BEFORE the captures. Oracles (all on a synthetic fbm fixture:
# fbm -> T.scatter_place -> MaskBlend -> captures):
#
#   (a) DETERMINISM     — two cold cook-off runs place BYTE-IDENTICAL .ogeo.
#   (b) COUPLING ORACLE — the post-bake height under PadMask>0.9 matches PadElev
#       within tolerance (the roads O-gate adapted): placement + pads can't disagree.
#       Plus: every placed point has a pad (PadMask@texel>0.5) at PadElev == its P.y.
#   (c) YAW-FROM-FIELD  — base yaw = hash(per-point field sample) -> a heading. A
#       constant field -> all yaws equal (its hash); a two-value field -> two headings.
#       Exact per-point check: recovered yaw == heading_of(sampled field value).
#
# scatter_place v2 (all new params default OFF -> the checks above stay byte-identical):
#   (d) LATTICE         — const weight + lattice_m=4: every P.x/P.z is an EXACT multiple
#       of 4 (village grid); two cold runs byte-identical; lanes deterministic + shift.
#   (e) CLUSTER PADS    — intersecting footprints union to ONE grade plane each (members
#       coplanar, seam pairs 0) with cross-level rejects; vs cluster_pads=False (seams).
#   (f) DIRECT YAW      — yaw_mode="direct" takes the field value AS radians (0.37), not
#       its hash.
#
# Runs standalone (main owns the lifecycle) or under the battery (run(ez, ctx)).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, math
import numpy as np
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

TAU = 6.283185307179586

# ---- the C++ _headingFromField mirror (scatter.py's shared hash chain) -------
from ork.hypergraph.dflow.terrain.scatter import _u01, SS_YAW  # stateless (seed, idx, stream) RNG


def heading_of(v):
    """base yaw for a field value v — the C++ _headingFromField port: hash the raw
    float32 bits through scatterU01(bits, 0, SS_YAW), scale to [0, TAU)."""
    bits = int(np.array([np.float32(v)]).view(np.uint32)[0])
    return float(_u01(bits, 0, SS_YAW)) * TAU


def circdist(a, b):
    d = abs((a - b) % TAU)
    return min(d, TAU - d)


def _read_channel(path):
    from orkengine.lev2 import Image
    img = lev2.Image.createFromFile(str(path))
    a = np.array(img.numpy, dtype=np.float32)
    return a[..., 0] if a.ndim == 3 else a


# ---- fixtures (DSL source; built via the terrain asset wrapper) -------------
COUPLE_DSL = '''
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

class PadHF(HeightField):
  EXTENT_M = 256.0
  def __init__(self):
    super().__init__()
    h = (T.fbm(frequency=2.5, octaves=5) * 0.5 + 0.5) * 120.0   # TRUE METERS, real relief
    w = T.normalize(h, out_lo=0.4, out_hi=1.0)                  # varying weight (mostly keep)
    place = T.scatter_place(h,
        export_name = "buildings",
        count       = 40,
        seed        = 7,
        align       = "up",
        scale       = (1.0, 1.0),
        cutoff      = 0.01,
        jitter      = 1.0,
        apron_m     = 8.0,
        types       = {"bldg": w},
        footprints  = {"bldg": (7.0, 7.0)},
        colliders   = {"bldg": ("box", 7.0, 5.0, 7.0)})
    hf = T.mix(h, place.pad_elev, place.pad_mask)               # flatten under the pads
    self.capture(hf, "height")
    self.capture(place.pad_mask, "padmask")
    self.capture(place.pad_elev, "padelev")
'''

YAW_CONST_DSL = '''
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

class YawConstHF(HeightField):
  EXTENT_M = 256.0
  def __init__(self):
    super().__init__()
    h = (T.fbm(frequency=2.0, octaves=4) * 0.5 + 0.5) * 40.0
    w = T.normalize(h, out_lo=0.5, out_hi=1.0)
    yawf = T.const(0.37)                                        # constant -> one heading
    place = T.scatter_place(h,
        export_name="yawc", count=60, seed=3, align="up",
        scale=(1.0, 1.0), cutoff=0.01, jitter=1.0, apron_m=4.0,
        types={"p": w}, footprints={"p": (3.0, 3.0)}, yaw_from_field=yawf)
    hf = T.mix(h, place.pad_elev, place.pad_mask)
    self.capture(hf, "height")
    self.capture(yawf, "yawfield")
'''

YAW_CELLS_DSL = '''
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

class YawCellsHF(HeightField):
  EXTENT_M = 256.0
  def __init__(self):
    super().__init__()
    h = (T.fbm(frequency=2.0, octaves=4) * 0.5 + 0.5) * 40.0
    w = T.normalize(h, out_lo=0.5, out_hi=1.0)
    alt = T.normalize(h)                                        # [0,1]
    gate = T.smoothstep(alt, 0.4990, 0.5010)                   # ~hard 0/1 split -> two cells
    yawf = T.mix(T.const(0.2), T.const(0.8), gate)             # 0.2 / 0.8 -> two headings
    place = T.scatter_place(h,
        export_name="yawcells", count=120, seed=5, align="up",
        scale=(1.0, 1.0), cutoff=0.01, jitter=1.0, apron_m=4.0,
        types={"p": w}, footprints={"p": (3.0, 3.0)}, yaw_from_field=yawf)
    hf = T.mix(h, place.pad_elev, place.pad_mask)
    self.capture(hf, "height")
    self.capture(yawf, "yawfield")
'''


# ---- v2 fixtures: AGGREGATION LATTICE / CLUSTER PADS / DIRECT YAW -----------
def LATTICE_DSL(lane_every, lane_m):
    """const-weight field (keep everywhere) + lattice_m=4 -> every candidate snaps to
    the 4m village grid before the mask kill. lane_every/lane_m widen every Nth line."""
    return '''
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

class LatHF(HeightField):
  EXTENT_M = 256.0
  def __init__(self):
    super().__init__()
    h = (T.fbm(frequency=2.0, octaves=4) * 0.5 + 0.5) * 30.0
    w = T.const(1.0)                                            # constant weight -> keep everywhere
    place = T.scatter_place(h,
        export_name="lat", count=120, seed=9, align="up",
        scale=(1.0, 1.0), cutoff=0.01, jitter=1.0, apron_m=2.0,
        lattice_m=4.0, lane_every=%d, lane_m=%g,
        types={"p": w}, footprints={"p": (1.5, 1.5)})
    hf = T.mix(h, place.pad_elev, place.pad_mask)
    self.capture(hf, "height")
''' % (int(lane_every), float(lane_m))


def CLUSTER_DSL(cluster_pads):
    """lattice_m=6 forces abutment (pitch < footprint), a 40m-relief fbm gives a slope;
    cluster_pads unions intersecting footprints -> one plane each + cross-level rejects."""
    return '''
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

class ClusHF(HeightField):
  EXTENT_M = 128.0
  def __init__(self):
    super().__init__()
    h = (T.fbm(frequency=2.5, octaves=4) * 0.5 + 0.5) * 40.0    # steeper -> real level splits
    w = T.const(1.0)
    place = T.scatter_place(h,
        export_name="clus", count=80, seed=4, align="up",
        scale=(1.0, 1.0), cutoff=0.01, jitter=0.5, apron_m=2.0,
        lattice_m=6.0, cluster_pads=%s, max_seam_m=1.0,
        types={"b": w}, footprints={"b": (5.0, 5.0)})
    hf = T.mix(h, place.pad_elev, place.pad_mask)
    self.capture(hf, "height")
''' % ("True" if cluster_pads else "False")


DIRECT_DSL = '''
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

class DirHF(HeightField):
  EXTENT_M = 256.0
  def __init__(self):
    super().__init__()
    h = (T.fbm(frequency=2.0, octaves=4) * 0.5 + 0.5) * 40.0
    w = T.normalize(h, out_lo=0.5, out_hi=1.0)
    yawf = T.const(0.37)                                        # constant -> every yaw == 0.37
    place = T.scatter_place(h,
        export_name="dir", count=60, seed=3, align="up",
        scale=(1.0, 1.0), cutoff=0.01, jitter=1.0, apron_m=4.0,
        types={"p": w}, footprints={"p": (3.0, 3.0)},
        yaw_from_field=yawf, yaw_mode="direct")
    hf = T.mix(h, place.pad_elev, place.pad_mask)
    self.capture(hf, "height")
'''

_CLUS_FOOT = 5.0  # the cluster fixture's footprint half-extent (meters)


def _obb_overlap(Pi, Xi, Pj, Xj, hx=_CLUS_FOOT, hz=_CLUS_FOOT):
    """OBB-OBB overlap in XZ (SAT over the 4 box edge normals) — mirrors the C++
    _clusterPads overlap test, so the seam oracle counts EXACTLY the pairs the placer
    considers intersecting."""
    def axes(X):
        ax, az = X[0, 0], X[0, 2]
        al = math.hypot(ax, az)
        if al > 1e-6: ax /= al; az /= al
        else: ax, az = 1.0, 0.0
        return (ax, az), (-az, ax)
    Aa, Ab = axes(Xi); Ba, Bb = axes(Xj)
    dx = Pj[0] - Pi[0]; dz = Pj[2] - Pi[2]
    def sep(Lx, Lz):
        d  = abs(dx * Lx + dz * Lz)
        rA = hx * abs(Aa[0]*Lx + Aa[1]*Lz) + hz * abs(Ab[0]*Lx + Ab[1]*Lz)
        rB = hx * abs(Ba[0]*Lx + Ba[1]*Lz) + hz * abs(Bb[0]*Lx + Bb[1]*Lz)
        return d > rA + rB
    return not (sep(*Aa) or sep(*Ab) or sep(*Ba) or sep(*Bb))


def _cluster_stats(geo):
    """(N, overlapping-pairs, seam-pairs, max |P.y diff| over overlapping pairs)."""
    P = np.array(geo.point["P"], dtype=np.float64)
    X = np.array(geo.point["xform"], dtype=np.float64)
    N = P.shape[0]
    overlaps = seam = 0
    maxdy = 0.0
    for i in range(N):
        for j in range(i + 1, N):
            if _obb_overlap(P[i], X[i], P[j], X[j]):
                overlaps += 1
                dy = abs(P[i, 1] - P[j, 1])
                maxdy = max(maxdy, dy)
                if dy > 1e-4:
                    seam += 1
    return N, overlaps, seam, maxdy


def _tmpdir():
    d = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))), ".tmp")
    os.makedirs(d, exist_ok=True)
    return d


def build(ctx, dsl, asset_name, extent_m=256.0, dimension=256):
    """Build a terrain asset from DSL source -> the products dict + the .ogeo path.
    The asset's cache dir is wiped first so EVERY run is a COLD cook — the gate must
    exercise fresh placement + pad emission, never a served-from-cache prior artifact
    (a placement regression must not hide behind capture-currency). extent_m is the
    authoritative bake world extent the placer grid uses (v2 cluster fixture runs a
    denser 128m so lattice abutment guarantees intersecting footprints)."""
    import shutil
    from orkengine.core import Path as _Path
    from ork.hypergraph.ecs.scene.assets import HeightField
    tmp = _tmpdir()
    dsl_path = os.path.join(tmp, asset_name + ".py")
    open(dsl_path, "w").write(dsl)
    shutil.rmtree(_Path.expandPathString("<assetcache>/terrain/%s" % asset_name), ignore_errors=True)
    hf = HeightField(dsl_file=dsl_path, dimension=dimension, extent_m=extent_m, ctx=ctx)
    hf.gendata.asset_name = asset_name
    art = hf.build()
    outdir = os.path.dirname(art["manifest"])
    return art, outdir


def _recover_yaw(xform_n):
    # align="up", scale=1 -> glm col0 = (-cos y, 0, -sin y). yaw = atan2(sin, cos).
    return math.atan2(-xform_n[0, 2], -xform_n[0, 0]) % TAU


def _roundtrip_graph():
    """Build a scatter_place graph directly (no bake) for the reflection round-trip."""
    from ork.hypergraph.dflow.terrain import HeightField
    from ork.hypergraph.dflow import terrain as T

    class RT(HeightField):
        EXTENT_M = 256.0
        def __init__(self):
            super().__init__()
            h = (T.fbm(frequency=2.0, octaves=3) * 0.5 + 0.5) * 50.0
            w = T.normalize(h, out_lo=0.4, out_hi=1.0)
            place = T.scatter_place(h, export_name="rt", count=20, seed=1,
                                    types={"a": w}, footprints={"a": (5.0, 6.0)}, apron_m=5.0,
                                    colliders={"a": ("box", 5.0, 3.0, 6.0)})
            hf = T.mix(h, place.pad_elev, place.pad_mask)
            self.capture(hf, "height")
    return RT().generatedflow()


def run(ez, ctx):
    from orkengine.lev2 import Geometry
    checks = {}
    extent = 256.0

    # ---- reflection round-trip: the module + its props survive JSON, and reshapeIOs
    #      rebuilds the plug set identically (idempotent re-serialization). Guards the
    #      owner reflection law (an untouched class would serialize as "class": "").
    try:
        g = _roundtrip_graph()
        js1 = core.Object.serializeJson(g)
        g2 = core.Object.deserializeJson(js1)
        js2 = core.Object.serializeJson(g2)
        rt = (js1 == js2) and ("terrain::ScatterPlaceModuleData" in js1) \
             and ('"export_name"' in js1) and ("rt" in js1)
        checks["roundtrip"] = bool(rt)
        print("scatter_place roundtrip: %s (idempotent=%s class_serialized=%s)"
              % ("PASS" if rt else "FAIL", js1 == js2,
                 "terrain::ScatterPlaceModuleData" in js1), flush=True)
    except Exception:
        import traceback; traceback.print_exc()
        checks["roundtrip"] = False

    # ---- (a) determinism + (b) coupling ------------------------------------
    try:
        art_a, dir_a = build(ctx, COUPLE_DSL, "sp_couple_a")
        art_b, dir_b = build(ctx, COUPLE_DSL, "sp_couple_b")
        oga = os.path.join(dir_a, "buildings.ogeo")
        ogb = os.path.join(dir_b, "buildings.ogeo")
        det = (open(oga, "rb").read() == open(ogb, "rb").read())
        geo = Geometry.read(oga)
        Np = int(np.array(geo.point["P"]).shape[0])
        det = det and (Np > 0)
        checks["determinism"] = bool(det)
        print("scatter_place determinism: %s (n=%d, ogeo byte-identical=%s)"
              % ("PASS" if det else "FAIL", Np, det), flush=True)
    except Exception:
        import traceback; traceback.print_exc()
        checks["determinism"] = False

    try:
        h  = _read_channel(art_a["height"])
        pm = _read_channel(os.path.join(dir_a, "padmask.exr"))
        pe = _read_channel(os.path.join(dir_a, "padelev.exr"))
        H, W = h.shape
        # field-level O-gate: post-bake height under PadMask>0.9 == PadElev (flattened).
        strong = pm > 0.9
        n_strong = int(np.count_nonzero(strong))
        err = np.abs(h[strong] - pe[strong]) if n_strong else np.array([0.0])
        max_err = float(err.max()) if n_strong else 1e9
        # placed points: each has a pad (mask>0.5) at PadElev == its P.y.
        P = np.array(geo.point["P"], dtype=np.float64)
        pad_hits, elev_ok = 0, 0
        for n in range(P.shape[0]):
            u = P[n, 0] / extent + 0.5; v = P[n, 2] / extent + 0.5
            xi = min(max(int(u * W), 0), W - 1); yi = min(max(int(v * H), 0), H - 1)
            if pm[yi, xi] > 0.5:
                pad_hits += 1
            if abs(pe[yi, xi] - P[n, 1]) < 0.05:
                elev_ok += 1
        coupling = (n_strong > 0) and (max_err < 3.0) and (pad_hits == P.shape[0]) and (elev_ok == P.shape[0])
        checks["coupling"] = bool(coupling)
        print("scatter_place coupling: %s (strong_texels=%d max|h-padelev|=%.4fm  pad_hits=%d/%d elev_ok=%d/%d)"
              % ("PASS" if coupling else "FAIL", n_strong, max_err, pad_hits, P.shape[0], elev_ok, P.shape[0]),
              flush=True)
    except Exception:
        import traceback; traceback.print_exc()
        checks["coupling"] = False

    # ---- (c) yaw-from-field: constant -> one heading ------------------------
    try:
        art_c, dir_c = build(ctx, YAW_CONST_DSL, "sp_yaw_const")
        geo = Geometry.read(os.path.join(dir_c, "yawc.ogeo"))
        X = np.array(geo.point["xform"], dtype=np.float64)
        yf = _read_channel(os.path.join(dir_c, "yawfield.exr"))
        P = np.array(geo.point["P"], dtype=np.float64)
        yaws = np.array([_recover_yaw(X[n]) for n in range(X.shape[0])])
        exp = heading_of(0.37)
        alleq = all(circdist(y, exp) < 1e-3 for y in yaws) and X.shape[0] > 0
        distinct = len(set(round(float(y), 4) for y in yaws))
        yc = alleq and (distinct == 1)
        checks["yaw_const"] = bool(yc)
        print("scatter_place yaw(const): %s (n=%d distinct=%d expect_heading=%.5f)"
              % ("PASS" if yc else "FAIL", X.shape[0], distinct, exp), flush=True)
    except Exception:
        import traceback; traceback.print_exc()
        checks["yaw_const"] = False

    # ---- (c) yaw-from-field: two cells -> two headings ----------------------
    try:
        art_d, dir_d = build(ctx, YAW_CELLS_DSL, "sp_yaw_cells")
        geo = Geometry.read(os.path.join(dir_d, "yawcells.ogeo"))
        X = np.array(geo.point["xform"], dtype=np.float64)
        P = np.array(geo.point["P"], dtype=np.float64)
        yf = _read_channel(os.path.join(dir_d, "yawfield.exr"))
        H, W = yf.shape
        # EXACT per-point: recovered yaw == heading_of(sampled field value). Accept the
        # point's texel or an immediate neighbor (float32(P.x) vs the placer's double wx
        # can straddle a texel boundary).
        matched = 0
        for n in range(X.shape[0]):
            u = P[n, 0] / extent + 0.5; v = P[n, 2] / extent + 0.5
            xi = min(max(int(u * W), 0), W - 1); yi = min(max(int(v * H), 0), H - 1)
            rec = _recover_yaw(X[n])
            ok = False
            for dy in (0, -1, 1):
                for dx in (0, -1, 1):
                    xx = min(max(xi + dx, 0), W - 1); yy = min(max(yi + dy, 0), H - 1)
                    if circdist(rec, heading_of(yf[yy, xx])) < 1e-3:
                        ok = True
            matched += 1 if ok else 0
        distinct = len(set(round(float(_recover_yaw(X[n])), 3) for n in range(X.shape[0])))
        h02, h08 = heading_of(0.2), heading_of(0.8)
        have_both = (any(circdist(_recover_yaw(X[n]), h02) < 1e-3 for n in range(X.shape[0]))
                     and any(circdist(_recover_yaw(X[n]), h08) < 1e-3 for n in range(X.shape[0])))
        cells = (X.shape[0] > 0) and (matched == X.shape[0]) and (distinct >= 2) and have_both
        checks["yaw_cells"] = bool(cells)
        print("scatter_place yaw(cells): %s (n=%d matched=%d distinct=%d both_headings=%s)"
              % ("PASS" if cells else "FAIL", X.shape[0], matched, distinct, have_both), flush=True)
    except Exception:
        import traceback; traceback.print_exc()
        checks["yaw_cells"] = False

    # ---- (d) v2 AGGREGATION LATTICE: const weight + lattice_m=4 -> every P.x/P.z is an
    #      EXACT multiple of 4 (village grid, world-origin anchored at yaw base 0); two
    #      cold runs byte-identical; lanes stay deterministic and shift the grid. --------
    try:
        _, dl_a = build(ctx, LATTICE_DSL(0, 0.0), "sp_lattice_a")
        _, dl_b = build(ctx, LATTICE_DSL(0, 0.0), "sp_lattice_b")
        oga = os.path.join(dl_a, "lat.ogeo"); ogb = os.path.join(dl_b, "lat.ogeo")
        det = (open(oga, "rb").read() == open(ogb, "rb").read())
        P = np.array(Geometry.read(oga).point["P"], dtype=np.float64)
        Np = P.shape[0]
        modx = np.abs(((P[:, 0] + 2.0) % 4.0) - 2.0)   # distance to nearest multiple of 4
        modz = np.abs(((P[:, 2] + 2.0) % 4.0) - 2.0)
        on_grid = int(np.count_nonzero((modx < 1e-3) & (modz < 1e-3)))
        # lanes: deterministic across cold runs AND they change the placement (grid widened).
        _, dl_l1 = build(ctx, LATTICE_DSL(3, 5.0), "sp_lattice_lane_a")
        _, dl_l2 = build(ctx, LATTICE_DSL(3, 5.0), "sp_lattice_lane_b")
        lane_a = open(os.path.join(dl_l1, "lat.ogeo"), "rb").read()
        lane_b = open(os.path.join(dl_l2, "lat.ogeo"), "rb").read()
        lane_det   = (lane_a == lane_b)
        lane_moved = (lane_a != open(oga, "rb").read())
        lattice = (Np > 0) and det and (on_grid == Np) and lane_det and lane_moved
        checks["lattice"] = bool(lattice)
        print("scatter_place lattice: %s (n=%d on_grid=%d/%d byte_ident=%s max_mod=%.6f lane_det=%s lane_moved=%s)"
              % ("PASS" if lattice else "FAIL", Np, on_grid, Np, det,
                 float(max(modx.max(), modz.max())), lane_det, lane_moved), flush=True)
    except Exception:
        import traceback; traceback.print_exc()
        checks["lattice"] = False

    # ---- (e) v2 CLUSTER PADS: intersecting footprints union to ONE grade plane each;
    #      overlapping members become coplanar (seam pairs 0, |P.y diff|==0) and late
    #      cross-level candidates are REJECTED. Contrast vs cluster_pads=False, which
    #      interpenetrates (party-wall seams present). ----------------------------------
    try:
        _, d_off = build(ctx, CLUSTER_DSL(False), "sp_cluster_off", extent_m=128.0)
        _, d_on  = build(ctx, CLUSTER_DSL(True),  "sp_cluster_on",  extent_m=128.0)
        n_off, ov_off, seam_off, _   = _cluster_stats(Geometry.read(os.path.join(d_off, "clus.ogeo")))
        n_on,  ov_on,  seam_on,  mdy = _cluster_stats(Geometry.read(os.path.join(d_on,  "clus.ogeo")))
        cluster = (n_on > 0) and (ov_on > 0) and (seam_on == 0) and (mdy == 0.0) \
                  and (seam_off > 0) and (n_on <= n_off)
        checks["cluster"] = bool(cluster)
        print("scatter_place cluster: %s (off: n=%d ovl=%d seams=%d | on: n=%d ovl=%d seams=%d maxdy=%.4f rejected=%d)"
              % ("PASS" if cluster else "FAIL", n_off, ov_off, seam_off,
                 n_on, ov_on, seam_on, mdy, n_off - n_on), flush=True)
    except Exception:
        import traceback; traceback.print_exc()
        checks["cluster"] = False

    # ---- (f) v2 DIRECT YAW: yaw_mode="direct" takes the field value AS radians (0.37),
    #      distinct from the hashed heading of 0.37. ------------------------------------
    try:
        _, d_dir = build(ctx, DIRECT_DSL, "sp_yaw_direct")
        X = np.array(Geometry.read(os.path.join(d_dir, "dir.ogeo")).point["xform"], dtype=np.float64)
        yaws = [_recover_yaw(X[n]) for n in range(X.shape[0])]
        maxerr = max((circdist(float(y), 0.37) for y in yaws), default=1e9)
        differs_from_hash = circdist(0.37, heading_of(0.37)) > 0.1  # direct != the hash path
        direct_yaw = (X.shape[0] > 0) and (maxerr < 1e-4) and differs_from_hash
        checks["direct_yaw"] = bool(direct_yaw)
        print("scatter_place yaw(direct): %s (n=%d max|yaw-0.37|=%.2e hash_heading=%.5f differs=%s)"
              % ("PASS" if direct_yaw else "FAIL", X.shape[0], maxerr,
                 heading_of(0.37), differs_from_hash), flush=True)
    except Exception:
        import traceback; traceback.print_exc()
        checks["direct_yaw"] = False

    return checks


def main():
    ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"
    results = run(ez, ctx)
    ez.mainThreadEnd()
    ok = bool(results) and all(results.values())
    print("=== terrain scatter_place gate %s (%s) ==="
          % ("PASSED" if ok else "FAILED",
             ", ".join("%s:%s" % (k, "P" if v else "F") for k, v in results.items())), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
