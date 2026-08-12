#!/usr/bin/env ork.python
###############################################################################
# EROX SOLVER-STABILITY gate — the per-step bed-change LIMITER + the SCALE-AWARE
# creep ceiling, measured across three cell sizes of ONE scale-family landscape.
#
# The two failure modes this pins, at opposite ends of the resolution range:
#
#   COARSE CELLS -> GRID WEAVE. The erode/deposit exchange used to have no
#     magnitude limiter (flow_erode always clamped dz to a fraction of the local
#     relief; erox did not), so at large cells the capacity term could over-carve
#     a cell below its neighbours in one step and the solver rang in the grid-axis
#     2-cell (Nyquist) mode. Detector: the fraction of the SLOPE field's power
#     spectrum above 0.35 cycles/texel (the top 30% of the band, where a weave
#     lives and real landform does not).
#
#   FINE CELLS -> FEATURELESS. The only damping was hillslope creep, whose per-step
#     diffusion number is creep*dt/cell^2. Creep is a PHYSICAL diffusivity, so the
#     total smoothing length sqrt(4*D*T) is the same in METERS at every resolution —
#     which means the value that reads as "a couple of cells" at 32 m cells erases
#     everything a 1 m grid could resolve. Detector: the bake's SMOOTHING SCALE in
#     meters (cell * rms_slope / rms_curvature — the length over which the surface
#     turns), which must SHRINK when the grid refines, or the finer grid paid 8x the
#     cook for the same damped sheet.
#
# The three bakes share a landscape SHAPE: dim is fixed and extent varies, with
# relief held at a fixed fraction of extent, so the fbm slopes are comparable and
# only the cell size (and therefore the solver's dt/iteration count) changes. Each
# case is baked TWICE — with the erosion and with zero passes — because every verdict
# is a ratio against the raw landscape the solver was handed, never an absolute.
#
#   s1  32 m cells: adds no top-of-band energy.
#   s2   8 m cells: likewise (the erox.py / viewer working point).
#   s3a  1 m cells: top-of-band energy bounded (a looser bar — real rills live there).
#   s3b  1 m cells: smoothing scale at least 2x finer than at 8 m cells.
#   s4  every bake is finite (no NaN/Inf) and bounded (no runaway bed).
#
# cache=False on the captures: the whole bake recomputes every run (no disk cook
# cache read/write), so the numbers are of THIS build, never of a stale blob.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

import sys, shutil
import numpy as np

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

DIM        = 256      # grid side for all three bakes (cheap; cell size rides EXTENT)
RELIEF_FRA = 0.05     # authored vertical relief as a fraction of extent (slope parity)
PASSES     = 4        # sequential erox passes (channels deepen pass over pass)

# the erox.py working tuning, shortened: thin concentrating flow, gentle rates.
EROX = dict(sim_time_s            = 30.0,
            rain_mps              = 0.006,
            evaporation_per_s     = 0.08,
            flow_speed_max_mps    = 10.0,
            capacity_Kc           = 0.5,
            erosion_rate_per_s    = 0.7,
            deposition_rate_per_s = 1.0,
            creep_m2ps            = 3.0)

# (label, extent_m) -> cell = extent/DIM
CASES = [("32m", 32.0 * DIM), ("8m", 8.0 * DIM), ("1m", 1.0 * DIM)]

# --- thresholds -----------------------------------------------------------------
# Both verdicts are RATIOS against the RAW (un-eroded) bake of the same landscape on the
# same grid, never absolute numbers: at 32 m cells an fbm landscape legitimately carries a
# lot of near-Nyquist slope energy simply because its features are only a few cells wide,
# so an absolute high-band threshold would fail a clean solver at coarse cells and pass a
# ringing one at fine cells.
#
# HI_GAIN_*: how much top-of-band (>0.35 cyc/texel) slope energy the solver may ADD, as a
# multiple of what it was handed. At coarse and mid cells the landform is broad relative to
# the grid, so anything the solver ADDS up there is the 2-cell mode: the bar is "adds none"
# (the unlimited solver measured 1.10 at 32 m cells; the limited one measures 0.84). At 1 m
# cells the solver resolves genuine dendritic rills two to four cells wide — real structure
# that legitimately lives in the top band — so the fine bar is looser; it is still a bar,
# because a solver that RANG at 1 m would land far above it.
HI_GAIN_MAX      = 1.00
HI_GAIN_MAX_FINE = 1.40
# FINE_SHARPER_MAX: the OVER-damping guard, stated as what refinement must BUY. The bake's
# smoothing scale is measured in METERS (cell * rms_slope / rms_curvature — the length over
# which the surface turns); going from 8 m cells to 1 m cells must bring that scale down by
# at least 2x, or the finer grid is paying 8x the cook to return the same damped sheet.
# Erosion is allowed to smooth — this only forbids smoothing that ignores the cell size.
FINE_SHARPER_MAX = 0.50


class _Scaled(HeightField):
  """One fbm landscape, optionally put through `passes` erox passes. EXTENT_M is
  per-instance so a single class serves all three cell sizes; relief tracks extent, so the
  bakes differ ONLY in cell size (and thus in the solver's CFL dt + iteration count).
  passes=0 is the RAW REFERENCE: the same landscape the solver was handed, sampled on the
  same grid — the only honest baseline for "how much high-band energy is the SOLVER's"."""

  def __init__(self, extent_m, passes):
    self.EXTENT_M = float(extent_m)
    super().__init__()
    amp = float(extent_m) * RELIEF_FRA
    h   = (T.Fbm(frequency=6.0, octaves=7) * 0.5 + 0.5) * amp
    for _ in range(int(passes)):
      h = T.erox(h, **EROX)
    self.capture(h, "height")   # cache=False: no cook cache, this build's numbers


def _field(path):
  """The RAW R32F capture as a (h,w) float array (EXR scalar captures are true units)."""
  img = lev2.Image.createFromFile(path)
  a   = np.frombuffer(img.data.bytes, dtype=np.float32)
  return a.reshape(img.height, img.width, img.numcomponents)[:, :, 0]


def _metrics(h, cell):
  """Resolution-FAIR descriptors of a baked bed.

  slope    : |grad h| / cell           — dimensionless rise/run.
  hi_frac  : fraction of the mean-removed SLOPE power spectrum above 0.35 cycles/texel.
             Texel-relative by construction, so the three cell sizes are comparable;
             the grid-axis 2-cell mode lands at the very top of this band.
  rms_curv : RMS of laplacian/(4*cell) — a slope DIFFERENCE (dimensionless), i.e. how
             much local structure survived the smoothing.
  """
  hi_ = h[1:-1, 2:]; lo_ = h[1:-1, :-2]
  up_ = h[2:, 1:-1]; dn_ = h[:-2, 1:-1]
  c_  = h[1:-1, 1:-1]
  gx  = (hi_ - lo_) / (2.0 * cell)
  gz  = (up_ - dn_) / (2.0 * cell)
  slope = np.sqrt(gx * gx + gz * gz)
  curv  = (hi_ + lo_ + up_ + dn_ - 4.0 * c_) / (4.0 * cell)

  s = slope - slope.mean()
  F = np.abs(np.fft.rfft2(s)) ** 2
  ky = np.fft.fftfreq(s.shape[0])[:, None]
  kx = np.fft.rfftfreq(s.shape[1])[None, :]
  k  = np.sqrt(kx * kx + ky * ky)
  tot = float(F.sum())
  hi  = float(F[k > 0.35].sum()) / max(tot, 1e-30)

  rms_slope = float(np.sqrt((slope ** 2).mean()))
  rms_curv  = float(np.sqrt((curv ** 2).mean()))
  # smoothing LENGTH in METERS: rms_slope / rms(curvature per meter), and curvature per
  # meter is rms_curv/cell — so smooth_m = cell*rms_slope/rms_curv. The scale over which
  # the surface turns; the number that must SHRINK as the grid refines, or the extra
  # resolution was damped away.
  smooth_m = float(cell * rms_slope / max(rms_curv, 1e-12))

  return dict(hi_frac=hi, rms_curv=rms_curv, rms_slope=rms_slope, smooth_m=smooth_m,
              relief=float(h.max() - h.min()),
              hmin=float(h.min()), hmax=float(h.max()),
              finite=bool(np.isfinite(h).all()))


def _bake(extent_m, passes, ctx, outdir):
  g = _Scaled(extent_m, passes).generatedflow()
  os.makedirs(outdir, exist_ok=True)
  for cap in lev2.terrain.capture_modules(g):
    cap.path = os.path.join(outdir, "{channel}.exr")
  lev2.terrain.bake_heightfield(g, ctx, DIM, float(extent_m))
  hpath = os.path.join(outdir, "height.exr")
  assert os.path.exists(hpath), f"no height capture in {outdir}"
  return _field(hpath)


def run(ez, ctx):
  base = "/tmp/erox_stability_gate"
  shutil.rmtree(base, ignore_errors=True); os.makedirs(base, exist_ok=True)
  results = {}
  M, R = {}, {}

  for label, extent in CASES:
    cell     = extent / DIM
    R[label] = _metrics(_bake(extent, 0,      ctx, os.path.join(base, label + "_raw")), cell)
    M[label] = _metrics(_bake(extent, PASSES, ctx, os.path.join(base, label)),          cell)
    m, r = M[label], R[label]
    print(f"[erox-stab] {label:>4} cells (extent {extent:g} m, dim {DIM}): "
          f"hi_frac={m['hi_frac']:.5f} (raw {r['hi_frac']:.5f}, x{m['hi_frac']/max(r['hi_frac'],1e-12):.2f})  "
          f"smooth={m['smooth_m']:.2f} m = {m['smooth_m']/cell:.2f} cells (raw {r['smooth_m']:.2f} m)  "
          f"rms_curv={m['rms_curv']:.5f} ({100.0*m['rms_curv']/max(r['rms_curv'],1e-12):.1f}% of raw)  "
          f"relief={m['relief']:.2f} m ({100.0*m['relief']/max(r['relief'],1e-12):.1f}% of raw)  "
          f"finite={m['finite']}", flush=True)

  # ---- s1/s2/s3a: the solver ADDS no top-of-band energy -------------------------
  for key, label, bar in (("s1_coarse_weave_free", "32m", HI_GAIN_MAX),
                          ("s2_mid_weave_free",    "8m",  HI_GAIN_MAX),
                          ("s3a_fine_weave_free",  "1m",  HI_GAIN_MAX_FINE)):
    ratio = M[label]["hi_frac"] / max(R[label]["hi_frac"], 1e-12)
    ok    = ratio < bar
    print(f"[erox-stab] {key}: hi_frac {R[label]['hi_frac']:.5f} -> {M[label]['hi_frac']:.5f} "
          f"(x{ratio:.2f} < {bar}) -> {'ok' if ok else 'WEAVE'}", flush=True)
    results[key] = ok

  # ---- s3b: refining the grid must BUY resolvable structure --------------------
  ratio = M["1m"]["smooth_m"] / max(M["8m"]["smooth_m"], 1e-12)
  ok    = ratio < FINE_SHARPER_MAX
  print(f"[erox-stab] s3b_fine_detail_survives: smoothing scale {M['8m']['smooth_m']:.2f} m "
        f"at 8 m cells -> {M['1m']['smooth_m']:.2f} m at 1 m cells (x{ratio:.3f} < "
        f"{FINE_SHARPER_MAX}) -> {'ok' if ok else 'OVER-DAMPED'}", flush=True)
  results["s3b_fine_detail_survives"] = ok

  # ---- s4: finite + bounded ----------------------------------------------------
  s4 = True
  for label, extent in CASES:
    m = M[label]
    bound = extent * RELIEF_FRA * 4.0      # 4x the authored relief is already absurd
    if not m["finite"] or abs(m["hmin"]) > bound or abs(m["hmax"]) > bound:
      s4 = False
  print(f"[erox-stab] s4_finite_and_bounded: {'ok' if s4 else 'DIVERGED'}", flush=True)
  results["s4_finite_and_bounded"] = s4

  return results


def main():
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    results = run(ez, ctx)
    ok = bool(results) and all(results.values())
    print(f"\n=== EROX solver-stability gate {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
      print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
  except BaseException:
    import traceback; traceback.print_exc()
    ok = False
  ez.mainThreadEnd()
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


if __name__ == "__main__":
  main()
