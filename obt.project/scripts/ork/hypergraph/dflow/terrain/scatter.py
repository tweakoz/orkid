###############################################################################
# scatter — CPU POST-bake placer for HeightField.scatter() sinks.
#
# E.2: PRODUCTION placement is the C++ placer (lev2.terrain.scatter_place_ogeo /
# hfdflow_scatter.cpp), which the C++ HeightField materializer runs at load with
# zero Python. THIS numpy implementation is the PARITY REFERENCE — the gate runs
# both against the same channels and pins counts / type_id / variant_seed EXACT
# and the float geometry to tolerance. Keep the two in operation-for-operation
# lockstep (any spec change lands in BOTH + the gate).
#
# Reads the baked height + per-type WEIGHT EXRs, runs a mask-weighted JITTERED-GRID
# placement with per-point WEIGHTED type selection (types mutually exclusive within a
# sink -> no overlap; overlapping layers are separate scatter() calls), and emits a
# ScatterSet as a POINT Geometry (.ogeo):
#   P            vec3   world position (meters)
#   xform        mtx4   full per-instance TRS (translate · align-to-normal · yaw · scale)
#   type_id      int    which type (declaration index; consumer maps -> mesh/variant)
#   variant_seed int    per-instance procedural variation
# MESH-AGNOSTIC: scatter produces ONLY this placement data; a separate consumer
# (instance_to_mesh node, or a direct instanced-drawable / mesh-task handoff) binds meshes.
#
# DETERMINISM (the E.2 spec — language-independent): every random draw comes from a
# STATELESS counter-based hash RNG keyed (seed, cell, stream):
#     mix(x) = splitmix64 finalizer
#     hash(seed, idx, stream) = mix( mix( mix(seed ^ GOLDEN) ^ idx ) ^ stream )
#     u01    = (hash >> 11) * 2^-53
# (GOLDEN = 0x9E3779B97F4A7C15). The grid is in METERS (density), so the same
# graph+seed yields identical placement at any bake dim. Output point order is
# grid-cell order (ascending cell index), also after a subsample (the cap keeps the
# `target` SMALLEST SS_PRIO priorities, ties by cell index, then re-sorts by cell).
###############################################################################
import math
import numpy as np

_GOLDEN = np.uint64(0x9E3779B97F4A7C15)
_M1     = np.uint64(0xBF58476D1CE4E5B9)
_M2     = np.uint64(0x94D049BB133111EB)

# streams — MUST match the C++ ScatterStream enum (hfdflow_scatter.h)
SS_JITTER_X, SS_JITTER_Z, SS_KEEP, SS_TYPE, SS_YAW, SS_SCALE, SS_VSEED, SS_PRIO = range(8)


def _mix(x):
    """splitmix64 finalizer, vectorized over uint64 (wrapping arithmetic)."""
    x = x ^ (x >> np.uint64(30))
    x = x * _M1
    x = x ^ (x >> np.uint64(27))
    x = x * _M2
    x = x ^ (x >> np.uint64(31))
    return x


def _hash(seed, idx, stream):
    """hash(seed, idx, stream) -> uint64 (idx may be a uint64 array)."""
    h = _mix(np.uint64(seed) ^ _GOLDEN)
    h = _mix(h ^ np.asarray(idx, dtype=np.uint64))
    h = _mix(h ^ np.uint64(stream))
    return h


def _u01(seed, idx, stream):
    """uniform double in [0,1) (vectorized over idx)."""
    return (_hash(seed, idx, stream) >> np.uint64(11)).astype(np.float64) * (2.0 ** -53)


def _read_channel(path):
    """Read a baked channel EXR -> (H,W) float32 (channel 0)."""
    from orkengine.lev2 import Image
    img = Image.createFromFile(str(path))
    a = np.array(img.numpy, dtype=np.float32)
    return a[..., 0] if a.ndim == 3 else a


def _sample(field, u, v):
    """Nearest-texel sample of an (H,W) field at uv in [0,1] (vectorized over u,v)."""
    h, w = field.shape
    xi = np.clip((u * w).astype(np.int64), 0, w - 1)
    yi = np.clip((v * h).astype(np.int64), 0, h - 1)
    return field[yi, xi]


def place(spec, channel_paths, *, extent_m):
    """Run a ScatterSpec against the baked channels -> (lev2.Geometry, count). MESH-AGNOSTIC.
    channel_paths: {channel_name -> EXR path} (height + the spec's per-type weight channels).
    Heights are TRUE METERS (natural units). PARITY REFERENCE for the C++ placer."""
    from orkengine.lev2 import Geometry
    extent_m = float(extent_m)
    seed = int(spec.seed) & 0xFFFFFFFF

    height  = _read_channel(channel_paths["height"])                       # (H,W) TRUE METERS
    weights = [_read_channel(channel_paths[ch]) for (_, ch) in spec.types] # K x (H,W)
    H, W = height.shape
    K = len(weights)

    # --- jittered grid (cells in METERS -> resolution-independent) ---
    if spec.density is not None:
        ncx = max(1, int(round(extent_m * math.sqrt(spec.density))))
    else:  # count mode: oversample, then priority-subsample the kept set to `count`
        ncx = max(1, int(math.ceil(math.sqrt(max(1, spec.count) * 4.0))))
    cell_m = extent_m / ncx
    k  = np.arange(ncx * ncx, dtype=np.uint64)            # the cell index — THE RNG key
    ci = (k % np.uint64(ncx)).astype(np.float64)          # meshgrid 'xy' raveled: ci varies fastest
    cj = (k // np.uint64(ncx)).astype(np.float64)
    wx = (ci + 0.5 + (_u01(seed, k, SS_JITTER_X) - 0.5) * spec.jitter) * cell_m - extent_m * 0.5
    wz = (cj + 0.5 + (_u01(seed, k, SS_JITTER_Z) - 0.5) * spec.jitter) * cell_m - extent_m * 0.5
    u = wx / extent_m + 0.5; v = wz / extent_m + 0.5

    # --- per-point weights -> total coverage W, keep test ---
    ws   = np.stack([np.clip(_sample(wt, u, v), 0.0, None) for wt in weights], axis=1)  # (Np,K) float32
    Wsum = ws.sum(axis=1)                                                               # (Np,) float32
    keep = (Wsum >= max(spec.cutoff, 1e-9)) & (_u01(seed, k, SS_KEEP) < np.clip(Wsum, 0.0, 1.0))
    idx  = np.nonzero(keep)[0]                            # ascending cell order

    geo = Geometry()
    if idx.size == 0:
        return geo, 0

    # --- per-point type pick: inverse-CDF over the per-point weights ---
    cdf     = np.cumsum(ws[idx], axis=1)                          # (Nk,K) float32
    r       = _u01(seed, k[idx], SS_TYPE) * cdf[:, -1]
    type_id = np.clip((r[:, None] >= cdf).sum(axis=1), 0, K - 1).astype(np.int32)

    # --- cap (or count-mode subsample): keep the `target` SMALLEST priorities (ties by
    #     cell index), then restore cell order — the language-neutral output order. ---
    target = spec.max_points if spec.count is None else min(spec.count, spec.max_points)
    if idx.size > target:
        prio = _u01(seed, k[idx], SS_PRIO)
        sel  = np.lexsort((idx, prio))[:target]           # by (prio, cell)
        sel  = np.sort(sel)                               # restore ascending-cell order
        idx = idx[sel]; type_id = type_id[sel]
    N = idx.size

    # --- positions: x,z from world; y from the baked height ---
    px = wx[idx]; pz = wz[idx]
    py = _sample(height, u[idx], v[idx])
    pos = np.stack([px, py, pz], axis=1).astype(np.float32)       # (N,3)

    # --- normals from the height gradient (physical meters) ---
    if spec.align == "normal":
        gz, gx = np.gradient(height)                              # d/d(row=z), d/d(col=x) (meters)
        nx = -_sample(gx, u[idx], v[idx]) / (extent_m / W)
        nz = -_sample(gz, u[idx], v[idx]) / (extent_m / H)
        nrm = np.stack([nx, np.ones_like(nx), nz], axis=1)
    else:
        nrm = np.tile(np.array([0.0, 1.0, 0.0]), (N, 1))
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True)

    # --- yaw + per-point scale ---
    yaw = spec.yaw[0] + (spec.yaw[1] - spec.yaw[0]) * _u01(seed, k[idx], SS_YAW)
    sc  = (spec.scale[0] + (spec.scale[1] - spec.scale[0]) * _u01(seed, k[idx], SS_SCALE))[:, None]

    # --- (N,4,4) TRS, rows = glm COLUMNS (np.array(fmtx4) / memcpy layout) ---
    up  = nrm
    ref = np.where(np.abs(up[:, 2:3]) > 0.9, np.array([1.0, 0.0, 0.0]), np.array([0.0, 0.0, 1.0]))
    t   = np.cross(ref, up); t /= np.linalg.norm(t, axis=1, keepdims=True)
    b   = np.cross(t, up)            # RIGHT-HANDED basis (det=+1). cross(up,t) was left-handed (det=-1) ->
                                     # a reflection that flipped triangle winding on every instanced draw.
    cy  = np.cos(yaw)[:, None]; sy = np.sin(yaw)[:, None]
    t2  = cy * t + sy * b
    b2  = -sy * t + cy * b
    mats = np.zeros((N, 4, 4), np.float32)
    mats[:, 0, :3] = (t2 * sc)       # glm column 0 = scaled tangent
    mats[:, 1, :3] = (up * sc)       # glm column 1 = scaled up  (instance +Y -> surface normal)
    mats[:, 2, :3] = (b2 * sc)       # glm column 2 = scaled bitangent
    lift = float(getattr(spec, "lift", 0.0))
    mats[:, 3, :3] = pos + nrm * lift  # glm column 3 = translation (+ uniform lift along the up/align axis)
    mats[:, 3, 3]  = 1.0

    # --- ScatterSet point Geometry ---
    geo.point["P"]            = pos                                       # vec3
    geo.point["xform"]        = mats                                      # mtx4 (comps==16 -> MTX4)
    geo.point["type_id"]      = type_id                                   # int
    # PER-ITEM physics proxy (mirrors the C++ placer; parity-gated): kind/dims per point
    # from the spec's per-type "kind:d0:d1:d2" collider declarations (-1 = none).
    tkind = np.full(K, -1, np.int32); tdims = np.zeros((K, 3), np.float32)
    for ti, (tname, _ch) in enumerate(spec.types):
        cs = (getattr(spec, "colliders", {}) or {}).get(tname)
        if cs:
            parts = cs.split(":")
            tkind[ti] = int(parts[0])
            tdims[ti] = [float(x) for x in (parts[1:4] + ["0", "0", "0"])[:3]]
    geo.point["proxy_kind"]   = tkind[type_id]                            # int per point
    geo.point["proxy_dims"]   = tdims[type_id]                            # vec3 per point
    # integer variant seed — EXACT across languages: top 30 bits of the hash
    geo.point["variant_seed"] = ((_hash(seed, k[idx], SS_VSEED) >> np.uint64(34))
                                 & np.uint64(0x3FFFFFFF)).astype(np.int32)
    return geo, N
