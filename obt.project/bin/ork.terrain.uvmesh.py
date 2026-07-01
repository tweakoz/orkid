#!/usr/bin/env python3
###############################################################################
# ork.terrain.uvmesh.py — DEBUG dump of a relaxed-UV parameterization as a mesh.
#
# Reads a terrain's "relaxed_uv" channel (the equal-area relax_uv module output;
# RGBA = relaxed uv.xy + geometric normal.x,z) and writes a wavefront OBJ whose
# TOPOLOGY is the base terrain grid but whose VERTICES are placed at the RELAXED
# uv coords on the XZ plane (Y=0): vertex = (uv.x, 0, uv.y). A regular planar grid
# shows up as a WARPED grid — steep faces expand (equal-area), so the relaxation is
# visible at a glance (load it next to the planar grid in any OBJ viewer).
#
#   ork.terrain.uvmesh.py erodeflow                 # by terrain name (<assetcache>/terrain/<name>)
#   ork.terrain.uvmesh.py <bakefolder|manifest.json|relaxed_uv.exr> -o /tmp/uv.obj
#   ork.terrain.uvmesh.py erodeflow --dim 256       # resample to 256x256 (default 200; keeps the OBJ small)
#   ork.terrain.uvmesh.py erodeflow --channel relaxed_uv --planar   # ALSO dump the planar grid for A/B
#
# The full-res EXR is huge (dim^2 verts); --dim resamples (nearest) to a viewable size.
###############################################################################
import sys, os, argparse, glob

from orkengine import core      # MUST precede lev2 (orkid python import order)
from orkengine import lev2
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))
from ork.hypergraph.dflow.terrain.manifest import TerrainManifest


def _assetcache_roots():
  """Candidate assetcache roots (terrain bakes land in <root>/terrain/<name>). expandPathString only
  resolves <assetcache> after a full app-init, so fall back to the known OBT global + staging caches."""
  roots = []
  try:
    from orkengine.core import Path as _Path
    exp = _Path.expandPathString("<assetcache>")
    if exp and "<" not in exp:
      roots.append(exp)
  except Exception:
    pass
  roots.append(os.path.expanduser("~/.obt-global/assetcache"))   # OBT global cache (where bakes land)
  sub = os.environ.get("OBT_SUBSPACE_DIR")
  if sub:
    roots.append(os.path.join(sub, "assetcache"))
  # de-dup preserving order
  seen, out = set(), []
  for r in roots:
    if r not in seen:
      seen.add(r); out.append(r)
  return out


def resolve_terrain(arg):
  """A bare terrain NAME -> <root>/terrain/<name> (first root that exists); an existing path -> as-is."""
  if os.path.exists(arg):
    return arg
  for root in _assetcache_roots():
    cand = os.path.join(root, "terrain", arg)
    if os.path.isdir(cand):
      return cand
  # nothing matched — return the first candidate so the caller's error names a real-ish path
  roots = _assetcache_roots()
  return os.path.join(roots[0], "terrain", arg) if roots else arg


def resolve_channel(path, channel):
  """(exr_path, manifest|None). `path` = bake FOLDER (-> *.terrain.json) / manifest.json / raw EXR."""
  if os.path.isdir(path):
    cands = sorted(glob.glob(os.path.join(path, "*.terrain.json")))
    if not cands:
      raise FileNotFoundError(f"no *.terrain.json in folder {path}")
    man = TerrainManifest.load(cands[0])
  elif path.endswith(".json"):
    man = TerrainManifest.load(path)
  else:
    return path, None
  if channel not in man.channels:
    raise KeyError(f"channel {channel!r} not in manifest; have {sorted(man.channels)}")
  return man.channel_path(channel), man


def load_rgba(exr_path):
  """-> (H, W, C) float32 array."""
  img = lev2.Image.createFromFile(str(exr_path))
  if not img or img.width == 0:
    raise RuntimeError(f"orkengine could not load {exr_path}")
  arr = np.array(img.numpy, dtype=np.float32)
  if arr.ndim == 2:
    arr = arr[..., None]
  if img.bytesPerChannel == 1:
    arr = arr / 255.0
  elif img.bytesPerChannel == 2 and "F" not in img.format_name:
    arr = arr / 65535.0
  return arr.astype(np.float32)


def resample_idx(src_dim, n):
  """n evenly-spaced source indices in [0, src_dim-1] (nearest)."""
  if n >= src_dim:
    return np.arange(src_dim)
  return np.round(np.linspace(0, src_dim - 1, n)).astype(np.int64)


def write_grid_obj(out_path, xs, ys, zs, comment=""):
  """xs/ys/zs are (N,N) arrays of the per-vertex position; topology = the NxN grid."""
  N = xs.shape[0]
  lines = []
  if comment:
    lines.append("# " + comment)
  lines.append(f"# {N}x{N} grid ({N*N} verts, {2*(N-1)*(N-1)} tris)")
  for r in range(N):
    for c in range(N):
      lines.append("v %.6f %.6f %.6f" % (float(xs[r, c]), float(ys[r, c]), float(zs[r, c])))
  def vid(r, c):
    return r * N + c + 1            # OBJ is 1-indexed
  for r in range(N - 1):
    for c in range(N - 1):
      a, b, d, e = vid(r, c), vid(r, c + 1), vid(r + 1, c + 1), vid(r + 1, c)
      lines.append("f %d %d %d" % (a, b, d))
      lines.append("f %d %d %d" % (a, d, e))
  with open(out_path, "w") as f:
    f.write("\n".join(lines) + "\n")
  return N * N, 2 * (N - 1) * (N - 1)


def main():
  ap = argparse.ArgumentParser(description="dump a relaxed-UV parameterization as an OBJ (Y=0, uv on XZ)")
  ap.add_argument("input", help="terrain NAME, bake folder, manifest.json, or a relaxed_uv EXR")
  ap.add_argument("--channel", default="relaxed_uv", help="manifest channel to read (default relaxed_uv)")
  ap.add_argument("--dim", type=int, default=200, help="resample target NxN (default 200; full-res is huge)")
  ap.add_argument("--planar", action="store_true",
                  help="ALSO write a sibling _planar.obj (the regular grid) for A/B comparison")
  ap.add_argument("-o", "--out", default=None, help="output OBJ path (default <name>_uvmesh.obj in cwd)")
  args = ap.parse_args()

  exr, man = resolve_channel(resolve_terrain(args.input), args.channel)
  if not os.path.isfile(exr):
    raise FileNotFoundError(f"relaxed-uv EXR not found: {exr}\n"
                            f"(is the terrain baked WITH relax_uv? the channel must exist)")
  uv = load_rgba(exr)                      # (H, W, C): .x=uv.x, .y=uv.y
  H, W = uv.shape[0], uv.shape[1]
  if uv.shape[2] < 2:
    raise RuntimeError(f"{args.channel} has <2 channels ({uv.shape[2]}) — not a uv field")

  ri = resample_idx(H, args.dim)
  ci = resample_idx(W, args.dim)
  sub = uv[np.ix_(ri, ci)]                 # (N, N, C)
  N = sub.shape[0]
  ru = sub[..., 0]                         # relaxed u
  rv = sub[..., 1]                         # relaxed v
  zeros = np.zeros((N, N), np.float32)

  base = os.path.splitext(os.path.basename(args.input.rstrip("/")))[0]
  out = args.out or os.path.join(os.getcwd(), f"{base}_uvmesh.obj")
  nv, nf = write_grid_obj(out, ru, zeros, rv,
                          comment=f"relaxed-uv mesh from {os.path.basename(exr)} (uv->XZ, Y=0)")
  print(f"wrote {out}  ({nv} verts, {nf} tris)  uv range x[{ru.min():.3f},{ru.max():.3f}] "
        f"y[{rv.min():.3f},{rv.max():.3f}]")

  if args.planar:
    # the regular grid the relaxation started from (texel centers) — the A/B reference.
    gx = (ci.astype(np.float32) + 0.5) / float(W)
    gy = (ri.astype(np.float32) + 0.5) / float(H)
    PX = np.broadcast_to(gx[None, :], (N, N))
    PZ = np.broadcast_to(gy[:, None], (N, N))
    pout = os.path.splitext(out)[0] + "_planar.obj"
    write_grid_obj(pout, PX, zeros, PZ, comment="planar reference grid (uv->XZ, Y=0)")
    print(f"wrote {pout}  (planar A/B reference)")


if __name__ == "__main__":
  main()
