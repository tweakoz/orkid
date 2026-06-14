#!/usr/bin/env ork.python
"""
ork.terrain.summarize - SPECTRAL / STRUCTURAL + PHYSICAL characterization of a baked
terrain HEIGHTFIELD. Complements ork.image.summarize.py: where the generic image
summarize reports first-order INTENSITY statistics, this is terrain-aware — it reads
the bake's manifest and reports SPATIAL STRUCTURE, STABILITY, and REAL-WORLD slope.

  <name>          a terrain asset name (e.g. erox) -> <assetcache>/terrain/<name>; finds
                  <asset>.terrain.json, loads the scale contract (extent_m/height_m/dim).
  <folder>        a terrain bake folder directly.
  <file.json>     a manifest json directly.
  <image>         a raw EXR/PNG (no manifest -> image-space metrics only, no meters).

Catches what intensity stats miss:
  * SPECKLE / INSTABILITY — single-pixel spikes + checkerboard (Nyquist) oscillation, via
    real-space local-MAD outliers + high-pass kurtosis (works even when the field was
    auto-exposed to [0,1], which defeats HDR firefly tests) and mode-weighted band energy.
  * BLUR / NO STRUCTURE — over-diffused mush (low band energy, tight slope, no concave tail).
  * PHYSICAL SLOPE — average ground slope along X (E-W) and Z (N-S) in DEGREES, using the
    manifest scale (Y_m = stored*height_m, cell_m = extent_m/dim) — NOT image-space. Both
    cell-scale (noise-sensitive) and landform-scale (over a ~64 m baseline).

Usage:
  ork.terrain.summarize.py <terrainname> [--channel height] [--json]   # e.g. erox
"""

import sys, os, argparse, json, glob
from orkengine import core
from orkengine import lev2
lev2.lev2appinit()
import numpy as np

_scripts = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "scripts")
if _scripts not in sys.path:
  sys.path.insert(0, _scripts)
from ork.hypergraph.dflow.terrain.manifest import TerrainManifest


# ----------------------------------------------------------------------------- load
def resolve_input(path, channel):
  """(exr_path, manifest|None, channel_name). `path` may be a bake FOLDER (-> its
  *.terrain.json), a manifest .json, or a raw image file."""
  if os.path.isdir(path):
    cands = sorted(glob.glob(os.path.join(path, "*.terrain.json")))
    if not cands:
      raise FileNotFoundError(f"no *.terrain.json in folder {path}")
    man = TerrainManifest.load(cands[0])
  elif path.endswith(".json"):
    man = TerrainManifest.load(path)
  else:
    return path, None, channel
  chan = channel or "height"
  if chan not in man.channels:
    raise KeyError(f"channel {chan!r} not in manifest; have {sorted(man.channels)}")
  return man.channel_path(chan), man, chan


def resolve_terrain(arg):
  """A bare terrain asset NAME (e.g. 'erox') -> its bake folder <assetcache>/terrain/<name>.
  An existing path (folder / manifest .json / image) is returned as-is."""
  if os.path.exists(arg):
    return arg
  from orkengine.core import Path as _Path
  return _Path.expandPathString(f"<assetcache>/terrain/{arg}")


def load_field(exr_path, channel_idx=0):
  img = lev2.Image.createFromFile(str(exr_path))
  if not img or img.width == 0:
    raise RuntimeError(f"orkengine could not load {exr_path}")
  arr = np.array(img.numpy, dtype=np.float32)
  meta = dict(width=img.width, height=img.height, numcomponents=img.numcomponents,
              bytesPerChannel=img.bytesPerChannel, format_name=img.format_name)
  if arr.ndim == 3:
    arr = arr[..., int(channel_idx)]
  fn = img.format_name
  if img.bytesPerChannel == 1:
    arr = arr / 255.0
  elif img.bytesPerChannel == 2 and "F" not in fn:
    arr = arr / 65535.0
  return arr.astype(np.float32), meta


def _excess_kurtosis(x):
  x = x.reshape(-1).astype(np.float64)
  mu = x.mean(); var = x.var() + 1e-30
  return float(((x - mu) ** 4).mean() / (var * var) - 3.0)


# ----------------------------------------------------------- physical slope (meters)
def _boxblur(f, r):
  """O(n) separable moving-average (integral-image), radius r."""
  if r < 1:
    return f
  k = 2 * r + 1
  def ma(a, axis):
    n = a.shape[axis]
    pad = [(0, 0), (0, 0)]; pad[axis] = (r + 1, r)
    cs = np.cumsum(np.pad(a, pad, mode="edge"), axis=axis)
    hi = [slice(None)] * 2; lo = [slice(None)] * 2
    hi[axis] = slice(k, k + n); lo[axis] = slice(0, n)
    return (cs[tuple(hi)] - cs[tuple(lo)]) / k
  return ma(ma(f.astype(np.float64), 0), 1).astype(np.float32)


def physical_slope(f, man, landform_m=64.0):
  """Average ground slope along X (E-W) and Z (N-S) in DEGREES, in REAL meters.
  D1 anchor: Y_m = stored[0,1] * height_m;  horizontal cell = extent_m/dim. gx is the
  rise per meter EAST (image cols), gy the rise per meter NORTH (image rows).
  Reported at the cell scale (noise-sensitive) and a landform scale (field box-blurred
  over ~landform_m so the value is the terrain's slope, not per-pixel jitter)."""
  cell_m = man.extent_m / man.dim
  ym = f * man.height_m                       # rendered physical height (D1 stored anchor)
  def dir_slope(field):
    gy, gx = np.gradient(field)               # per-pixel diff; /cell_m -> rise_m/run_m
    return (gx / cell_m), (gy / cell_m)
  def stats(a):
    aa = np.abs(a).reshape(-1)
    return dict(mean_deg=float(np.degrees(np.arctan(aa.mean()))),
                p50_deg=float(np.degrees(np.arctan(np.percentile(aa, 50)))),
                p99_deg=float(np.degrees(np.arctan(np.percentile(aa, 99)))))
  sx, sz = dir_slope(ym)
  r = max(1, int(round(landform_m / cell_m)))
  sxb, szb = dir_slope(_boxblur(ym, r))
  mx, mz = float(np.abs(sx).mean()), float(np.abs(sz).mean())
  mxb, mzb = float(np.abs(sxb).mean()), float(np.abs(szb).mean())
  return dict(cell_m=cell_m, landform_m=r * cell_m,
              cell={"x_EW": stats(sx), "z_NS": stats(sz)},
              landform={"x_EW": stats(sxb), "z_NS": stats(szb)},
              anisotropy_xz_cell=float(mx / (mz + 1e-12)),
              anisotropy_xz_landform=float(mxb / (mzb + 1e-12)))


def walk_profile(f, man, ndir=16):
  """Walk an observer across the heightfield along ndir evenly-spaced directions, one cell
  (cell_m) per footstep, sampling the ACTUAL pixel (nearest, NO interpolation -> preserves
  spikes). Measures the PHYSICAL up/down a walker feels — the perceptual 'is it spikee' test
  (a small-dB high-freq noise still jolts the walker every step):
    per_step_rise_m   mean |dH| per footstep (m)
    per_step_slope    that as an angle (deg); p50 = the TYPICAL step
    reversal_rate     fraction of steps where the path flips up<->down  (0 = smooth ramp,
                      ~0.5 = salt-and-pepper) — the cleanest 'spikee' discriminator
    jolt_m            mean |2nd difference| along the path (m) — vertical kink per step
  A steep-but-SMOOTH slope has high slope but LOW reversal_rate + jolt. To sample arbitrary
  directions without going out of bounds, an inscribed rotated grid (~dim/1.5) is used."""
  dim = man.dim
  cell_m = man.extent_m / dim
  Hm = (f * man.height_m).astype(np.float32)        # as-rendered physical height (D1)
  G = max(8, int(dim / 1.5))
  g0 = G / 2.0; c = dim / 2.0
  ii = np.arange(G, dtype=np.float32)[:, None]       # step index (axis 0 = along direction)
  jj = np.arange(G, dtype=np.float32)[None, :]       # line index (axis 1 = parallel walks)
  rise, slope_mean, slope_p50, rev, jolt, jolt99 = [], [], [], [], [], []
  for k in range(ndir):
    th = np.pi * k / ndir                            # line orientations over 180 deg
    ux, uy = float(np.cos(th)), float(np.sin(th)); px, py = -uy, ux
    x = c + (ii - g0) * ux + (jj - g0) * px
    y = c + (ii - g0) * uy + (jj - g0) * py
    xi = np.clip(np.round(x).astype(np.int32), 0, dim - 1)
    yi = np.clip(np.round(y).astype(np.int32), 0, dim - 1)
    samp = Hm[yi, xi]                                # (G,G): axis0 = footstep, axis1 = path
    d1 = samp[1:, :] - samp[:-1, :]                  # per-step rise (m)
    a = np.abs(d1)
    rise.append(float(a.mean()))
    slope_mean.append(float(np.degrees(np.arctan(a.mean() / cell_m))))
    slope_p50.append(float(np.degrees(np.arctan(float(np.median(a)) / cell_m))))
    s = np.sign(d1)
    rev.append(float((s[1:, :] * s[:-1, :] < 0).mean()))   # up<->down flips
    d2 = np.abs(samp[2:, :] - 2.0 * samp[1:-1, :] + samp[:-2, :])
    jolt.append(float(d2.mean())); jolt99.append(float(np.percentile(d2, 99)))
  return dict(cell_m=cell_m, ndir=ndir,
              per_step_rise_m=float(np.mean(rise)),
              per_step_slope_deg=float(np.mean(slope_mean)),
              per_step_slope_p50_deg=float(np.mean(slope_p50)),
              reversal_rate=float(np.mean(rev)),
              jolt_m=float(np.mean(jolt)), jolt_p99_m=float(np.mean(jolt99)))


# ------------------------------------------------------------------------- intensity
def intensity_stats(f):
  flat = f.reshape(-1)
  mean = float(flat.mean()); std = float(flat.std())
  skew = float((((flat - mean) / (std + 1e-12)) ** 3).mean())
  ps = np.percentile(flat, [0, 1, 50, 99, 100])
  return dict(min=float(ps[0]), p1=float(ps[1]), p50=float(ps[2]), p99=float(ps[3]),
              max=float(ps[4]), mean=mean, std=std, hypsometric_skew=skew)


# -------------------------------------------------------------- speckle (REAL space)
def _box3(f):
  bx = (np.roll(f, 1, 1) + f + np.roll(f, -1, 1)) / 3.0
  return (np.roll(bx, 1, 0) + bx + np.roll(bx, -1, 0)) / 3.0


def _neighbor_count(mask):
  """8-connected count of True neighbours (toroidal; edge wrap negligible at 4k)."""
  m = mask.astype(np.uint8)
  return (np.roll(m, 1, 0) + np.roll(m, -1, 0) + np.roll(m, 1, 1) + np.roll(m, -1, 1)
          + np.roll(np.roll(m, 1, 0), 1, 1) + np.roll(np.roll(m, 1, 0), -1, 1)
          + np.roll(np.roll(m, -1, 0), 1, 1) + np.roll(np.roll(m, -1, 0), -1, 1))


def speckle_stats(f):
  """Distinguish INSTABILITY SPECKLE (isolated salt-and-pepper spikes) from real sharp
  STRUCTURE (ridgelines / channels / terrace risers). BOTH produce high-pass outliers and
  heavy kurtosis, so magnitude/kurtosis ALONE cannot tell them apart (verified: good
  eroded terrain scores HIGHER kurtosis than a spiky-but-flat field). The discriminator is
  SPATIAL ISOLATION: a true speckle pixel has NO outlier neighbour; a ridge/channel pixel
  is part of a connected line. isolation_ratio ~1 => salt-and-pepper; ~0 => coherent."""
  hp = f - _box3(f)
  ahp = np.abs(hp); flat = ahp.reshape(-1)
  med = float(np.median(flat)); mad = float(np.median(np.abs(flat - med))) + 1e-12
  scale = 1.4826 * mad
  O = ahp > (med + 6.0 * scale)               # high-pass outliers (spikes OR sharp features)
  nO = int(O.sum())
  isolated = O & (_neighbor_count(O) == 0)    # outlier w/ NO outlier neighbour = salt&pepper
  return dict(outlier_frac=nO / O.size,
              isolation_ratio=float(isolated.sum()) / max(nO, 1),    # KEY: speckle vs structure
              salt_pepper_frac=float(isolated.sum()) / O.size,
              hp_excess_kurtosis=_excess_kurtosis(hp),
              hp_std_over_signal=float(hp.std()) / (float(f.std()) + 1e-12),
              hp_max_over_robust=float(flat.max() / (med + scale)))


# -------------------------------------------------------------------------- spectral
def _windowed_power(f):
  h, w = f.shape
  f = f - f.mean()
  wy = np.hanning(h)[:, None]; wx = np.hanning(w)[None, :]
  F = np.fft.fftshift(np.fft.fft2(f * (wy * wx)))
  power = (F.real ** 2 + F.imag ** 2)
  power[h // 2, w // 2] = 0.0
  return power


def band_energy(power):
  h, w = power.shape; cy, cx = h // 2, w // 2
  y, x = np.indices((h, w))
  k = np.sqrt(((x - cx) / w) ** 2 + ((y - cy) / h) ** 2)
  tot = float(power.sum()) + 1e-30
  def frac(lo, hi): return float(power[(k >= lo) & (k < hi)].sum()) / tot
  return dict(low=frac(0.0, 0.10), mid=frac(0.10, 0.25),
              high=frac(0.25, 0.45), nyquist=frac(0.45, 0.7101))


def radial_spectrum(power, nbins=28):
  h, w = power.shape; cy, cx = h // 2, w // 2
  y, x = np.indices((h, w))
  k = np.sqrt(((x - cx) / w) ** 2 + ((y - cy) / h) ** 2).reshape(-1)
  p = power.reshape(-1)
  m = (k > 0) & (k <= 0.5); k, p = k[m], p[m]
  edges = np.logspace(np.log10(k.min() + 1e-6), np.log10(0.5), nbins + 1)
  idx = np.clip(np.digitize(k, edges) - 1, 0, nbins - 1)
  P = np.zeros(nbins); cnt = np.zeros(nbins)
  np.add.at(P, idx, p); np.add.at(cnt, idx, 1.0)
  cnt[cnt == 0] = 1.0; P = P / cnt
  kc = 0.5 * (edges[:-1] + edges[1:]); g = P > 0
  return kc[g], P[g]


def spectral_summary(f):
  power = _windowed_power(f)
  bands = band_energy(power)
  kc, P = radial_spectrum(power)
  lk, lp = np.log10(kc), np.log10(P)
  lo, hi = 2, len(lk) - 2
  beta = float(-np.polyfit(lk[lo:hi], lp[lo:hi], 1)[0]) if hi > lo + 1 else float("nan")
  h, w = power.shape; cy, cx = h // 2, w // 2
  y, x = np.indices((h, w)); ky = (y - cy) / h; kx = (x - cx) / w
  ang = (np.degrees(np.arctan2(ky, kx)) % 180.0); kk = np.sqrt(kx * kx + ky * ky)
  bm = (kk > 0.02) & (kk < 0.45); labels = ["E-W", "diag45", "N-S", "diag135"]; en = {}
  for s in range(4):
    loa = (s * 45.0 - 22.5) % 180.0; hia = (loa + 45.0) % 180.0
    sel = ((ang >= loa) & (ang < hia)) if loa < hia else ((ang >= loa) | (ang < hia))
    en[labels[s]] = float(power[bm & sel].sum())
  tt = sum(en.values()) + 1e-30; fr = {k2: v / tt for k2, v in en.items()}
  return dict(beta=beta, bands=bands, anisotropy=float(max(fr.values()) / (min(fr.values()) + 1e-12)),
              sector_frac={k2: round(v, 3) for k2, v in fr.items()}), kc, P


# --------------------------------------------------------------------------- terrain
def terrain_structure(f):
  gy, gx = np.gradient(f)
  slope = np.sqrt(gx * gx + gy * gy)
  lap = (np.roll(f, 1, 0) + np.roll(f, -1, 0) + np.roll(f, 1, 1) + np.roll(f, -1, 1) - 4.0 * f)
  s = slope.reshape(-1); c = lap.reshape(-1); cs = float(c.std() + 1e-12)
  return dict(slope_p50=float(np.percentile(s, 50)), slope_p99=float(np.percentile(s, 99)),
              slope_max=float(s.max()),
              slope_p99_over_p50=float(np.percentile(s, 99) / (np.percentile(s, 50) + 1e-12)),
              curvature_skew=float((((c - c.mean()) / cs) ** 3).mean()),
              curvature_excess_kurtosis=_excess_kurtosis(c))


# ------------------------------------------------------------------------- rendering
def ascii_spectrum(kc, P, width=46, rows=10):
  lp = np.log10(P); lk = np.log10(kc)
  cols = np.linspace(lk.min(), lk.max(), width)
  vals = np.interp(cols, lk, lp); lo, hi = lp.min(), lp.max()
  norm = (vals - lo) / (hi - lo + 1e-12); out = []
  for r in range(rows, 0, -1):
    out.append("  |" + "".join("#" if norm[c] >= (r - 0.5) / rows else " " for c in range(width)))
  out.append("  +" + "-" * width)
  out.append("   k=%.4f%sk=0.5  y=log10 P(k)" % (kc[0], " " * (width - 16)))
  return "\n".join(out)


def verdict(spk, spec, terr, walk=None, scalar=True):
  bands = spec["bands"]; flags = []
  # SPIKEE = the PERCEPTUAL test (calibrated vs good xxx/xxx2/xxx3): the typical footstep
  # is steep AND the path jolts (2nd-diff ~ the per-step rise) rather than ramping smoothly.
  # Catches low-dB broadband per-cell noise that band-energy / isolation / histogram miss.
  if walk is not None and walk["per_step_slope_p50_deg"] > 30.0 and walk["jolt_m"] > 0.5 * walk["per_step_rise_m"]:
    flags.append(f"SPIKEE (median step {walk['per_step_slope_p50_deg']:.0f}deg, jolt {walk['jolt_m']:.1f}m/step)")
  # SPECKLE = ISOLATED salt-and-pepper outliers (instability) — NOT mere sharp features
  # (good eroded terrain has many sharp-but-CONNECTED outliers, so isolation is the key).
  if spk["isolation_ratio"] > 0.45 and spk["salt_pepper_frac"] > 0.001:
    flags.append(f"SPECKLE (isolated {spk['salt_pepper_frac']*100:.2f}% px, iso {spk['isolation_ratio']:.2f})")
  if bands["nyquist"] > 0.03:
    flags.append(f"CHECKERBOARD (nyquist {bands['nyquist']*100:.1f}%)")
  if terr["slope_p99_over_p50"] < 2.5:
    flags.append(f"BLURRY (slope p99/p50 {terr['slope_p99_over_p50']:.2f})")
  # NOTE: high spectral anisotropy is NOT flagged — terracing (xxx2/xxx3) makes it
  # legitimately directional; it is printed for inspection, not judged.
  return flags if flags else ["clean"]


def main():
  ap = argparse.ArgumentParser(description="Terrain heightfield spectral/structural/physical summary")
  ap.add_argument("terrain", help="terrain asset NAME (e.g. erox) -> <assetcache>/terrain/<name>; "
                                  "or a bake folder / manifest .json / image path")
  ap.add_argument("--channel", default=None, help="manifest channel name (default 'height')")
  ap.add_argument("--landform-m", type=float, default=64.0, help="landform slope baseline (m)")
  ap.add_argument("--json", action="store_true")
  args = ap.parse_args()

  target = resolve_terrain(args.terrain)
  if not os.path.exists(target):
    print(f"Error: terrain {args.terrain!r} -> {target} not found (no such bake; run it first?)",
          file=sys.stderr); return 1
  exr_path, man, chan = resolve_input(target, args.channel)
  f, meta = load_field(exr_path)
  inten = intensity_stats(f)
  spk = speckle_stats(f)
  spec, kc, P = spectral_summary(f)
  terr = terrain_structure(f)
  scalar = (meta["numcomponents"] == 1)
  phys = physical_slope(f, man, args.landform_m) if (man is not None and scalar) else None
  walk = walk_profile(f, man) if (man is not None and scalar) else None
  v = verdict(spk, spec, terr, walk=walk, scalar=scalar)

  if args.json:
    out = dict(info=meta, channel=chan, intensity=inten, speckle=spk, spectral=spec,
               terrain=terr, verdict=v)
    if man is not None:
      out["scale"] = dict(extent_m=man.extent_m, height_m=man.height_m, dim=man.dim,
                          cell_m=man.extent_m / man.dim)
    if phys is not None:
      out["physical_slope"] = phys
    if walk is not None:
      out["walk"] = walk
    print(json.dumps(out, indent=2)); return 0

  b = spec["bands"]
  print(f"=== terrain summary: {os.path.basename(exr_path)}"
        + (f"  [{chan}]" if man is not None else "") + " ===")
  print(f"  VERDICT   {', '.join(v)}")
  if man is not None:
    print(f"  scale     extent={man.extent_m:.0f}m  height={man.height_m:.0f}m  dim={man.dim}  "
          f"cell={man.extent_m/man.dim:.2f}m/px")
  print(f"  info      {meta['width']}x{meta['height']}  {meta['format_name']}  {meta['numcomponents']}ch")
  print(f"  intensity min={inten['min']:.4f} p50={inten['p50']:.4f} max={inten['max']:.4f} "
        f"mean={inten['mean']:.4f} std={inten['std']:.4f} skew={inten['hypsometric_skew']:+.3f}")
  if phys is not None:
    c = phys["cell"]; l = phys["landform"]
    print(f"  SLOPE(deg, physical)   cell={phys['cell_m']:.1f}m   landform={phys['landform_m']:.0f}m")
    print(f"     cell-scale     X/E-W mean={c['x_EW']['mean_deg']:5.1f} p99={c['x_EW']['p99_deg']:5.1f}   "
          f"Z/N-S mean={c['z_NS']['mean_deg']:5.1f} p99={c['z_NS']['p99_deg']:5.1f}   (noise-sensitive)")
    print(f"     landform       X/E-W mean={l['x_EW']['mean_deg']:5.1f} p99={l['x_EW']['p99_deg']:5.1f}   "
          f"Z/N-S mean={l['z_NS']['mean_deg']:5.1f} p99={l['z_NS']['p99_deg']:5.1f}")
    print(f"     anisotropy X/Z  cell={phys['anisotropy_xz_cell']:.2f}  landform={phys['anisotropy_xz_landform']:.2f}  (1=isotropic)")
  if walk is not None:
    print(f"  WALK ({walk['ndir']} dir, physical)   footstep={walk['cell_m']:.1f}m")
    print(f"     per-step rise={walk['per_step_rise_m']:.2f}m   slope mean={walk['per_step_slope_deg']:.1f} "
          f"p50={walk['per_step_slope_p50_deg']:.1f} deg")
    print(f"     reversal_rate={walk['reversal_rate']:.3f} (0=smooth ramp .. ~0.5=salt&pepper)   "
          f"jolt mean={walk['jolt_m']:.2f}m p99={walk['jolt_p99_m']:.2f}m")
  print(f"  SPECKLE   isolation_ratio={spk['isolation_ratio']:.2f} (1=salt&pepper, 0=coherent)  "
        f"salt_pepper={spk['salt_pepper_frac']*100:.3f}%  outliers={spk['outlier_frac']*100:.2f}%")
  print(f"            hp_kurtosis={spk['hp_excess_kurtosis']:.0f} (sharp features OR spikes — not diagnostic alone)  "
        f"hp_max/robust={spk['hp_max_over_robust']:.0f}")
  print(f"  spectral  beta={spec['beta']:.2f}  band energy: low={b['low']:.3f} mid={b['mid']:.3f} "
        f"high={b['high']:.3f} nyq={b['nyquist']:.3f}")
  print(f"            anisotropy={spec['anisotropy']:.2f} {spec['sector_frac']}"
        + ("" if scalar else "  (per-component; biased for normals)"))
  print(ascii_spectrum(kc, P))
  print(f"  terrain   slope p50={terr['slope_p50']:.5f} p99={terr['slope_p99']:.5f} "
        f"p99/p50={terr['slope_p99_over_p50']:.2f} max={terr['slope_max']:.5f}")
  print(f"            curv_skew={terr['curvature_skew']:+.3f}  curv_kurtosis={terr['curvature_excess_kurtosis']:.1f}")
  return 0


if __name__ == "__main__":
  sys.exit(main())
