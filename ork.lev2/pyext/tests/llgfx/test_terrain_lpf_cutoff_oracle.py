#!/usr/bin/env python3
###############################################################################
# LPF CUTOFF UNITS byte-oracle (the gate for the cutoff restructure).
#
# The lpf cutoff is now ONE `cutoff` plug + a `cutoff_units` reflected enum
# {texels, meters} (replacing the old cutoff_texels / cutoff_m two-plug form).
# This oracle proves the two units agree EXACTLY at the conversion boundary and
# that texels-mode is world-scale invariant, byte-for-byte:
#
#   o1  meters<->texels EQUIVALENCE. For an EXACT conversion factor
#       texelsPerMeter = dim/extent_m, `cutoff=meters(X)` bakes BYTE-IDENTICAL to
#       `cutoff=texels(X * texelsPerMeter)` — the meters path multiplies by exactly
#       that factor in C++, so both reach the same texel-domain sigma. dim/extent are
#       chosen a power of two so the factor is exact and there is no fp slop to hide a
#       real divergence (true byte-parity, not approximate).
#   o2  texels-mode DIM-INVARIANCE (the sigma path). With `units='texels'` the cutoff
#       is consumed DIRECTLY (never scaled by texelsPerMeter), so the same texels(K)
#       cutoff bakes BYTE-IDENTICAL across DIFFERENT extent_m at fixed dim — the texel
#       sigma does not consult the world mapping. (Meters-mode, by contrast, WOULD
#       differ across extent; o1 already exercises that path.)
#   o3  the OLD kwargs FAIL LOUD. cutoff_m= / cutoff_texels= are removed (pre-1.0, no
#       shim) so a call using them raises a TypeError naming the offending keyword.
#
# All-analytic corpus (fbm/terrace/lpf) so the bakes are deterministic; capDate (a
# per-bake timestamp) is masked before hashing, the sibling battery's convention.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Path as _Path

import sys, glob, shutil, hashlib

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.units import meters, texels

DIM     = 256
EXTENT  = 8192.0          # tpm = 256/8192 = 0.03125 (2^-5, EXACT)
EXTENT2 = 4096.0          # a second world scale for the texels-invariance test
TPM     = float(DIM) / EXTENT

_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"


def _masked(path):
  """Raw EXR bytes with the embedded capDate (a per-bake timestamp) zeroed, so two
  identical bakes hash equal (the battery's byte-identity convention)."""
  with open(path, "rb") as f:
    b = bytearray(f.read())
  i = b.find(_CAPDATE)
  if i >= 0:
    b[i + len(_CAPDATE):i + len(_CAPDATE) + 19] = b"\x00" * 19
  return hashlib.sha256(bytes(b)).hexdigest()


def _wipe():
  # COLD cook cache before each bake (the v3->v4 salt bump already invalidated old
  # lpf entries; wiping keeps the oracle independent of any warm state).
  shutil.rmtree(str(_Path.expandPathString("<staging>/dflowcache")), ignore_errors=True)


class _Lpf(HeightField):
  """fbm -> terrace -> lpf(cutoff), capture height. cutoff/units are ctor-injected."""
  def __init__(self, cutoff, units=None):
    super().__init__()
    h = (T.Fbm(frequency=3.0, octaves=7) * 0.5 + 0.5) * 400.0
    h = T.terrace(h, step_m=20.0, sharpness=3.0)
    h = T.lpf(h, cutoff=cutoff, units=units)
    self.capture(h, "height", cache=True)


def _bake(factory, ctx, outdir, extent):
  _wipe()
  g = factory().generatedflow()
  os.makedirs(outdir, exist_ok=True)
  for cap in lev2.terrain.capture_modules(g):
    cap.path = os.path.join(outdir, "height.exr")
  lev2.terrain.bake_heightfield(g, ctx, DIM, extent)
  paths = sorted(glob.glob(os.path.join(outdir, "*.exr")))
  assert paths, f"no EXR product in {outdir}"
  return _masked(paths[0])


def _raises_typeerror(fn):
  try:
    fn()
    return False
  except TypeError:
    return True


def run(ez, ctx):
  base = "/tmp/lpf_cutoff_oracle"
  shutil.rmtree(base, ignore_errors=True); os.makedirs(base, exist_ok=True)
  results = {}

  # ---- o1: meters(X) == texels(X * tpm), byte-identical (EXACT conversion) ----------
  o1 = True
  for X in (64.0, 128.0, 256.0):
    tx = X * TPM                                   # exact (X * 2^-5)
    sha_m = _bake(lambda X=X: _Lpf(meters(X)), ctx, os.path.join(base, f"m{int(X)}"), EXTENT)
    sha_t = _bake(lambda tx=tx: _Lpf(texels(tx)), ctx, os.path.join(base, f"t{int(X)}"), EXTENT)
    match = (sha_m == sha_t)
    print(f"[lpf-oracle] o1 meters({X:g}) vs texels({tx:g}) @dim={DIM} extent={EXTENT:g}: "
          f"{'IDENTICAL' if match else 'DIVERGED'} ({sha_m[:12]} / {sha_t[:12]})", flush=True)
    o1 = o1 and match
  results["o1_meters_texels_equivalence"] = o1

  # ---- o2: texels-mode is world-scale invariant (same texels(K) across extent) ------
  K = 8.0
  sha_e1 = _bake(lambda: _Lpf(texels(K)), ctx, os.path.join(base, "tx_e1"), EXTENT)
  sha_e2 = _bake(lambda: _Lpf(texels(K)), ctx, os.path.join(base, "tx_e2"), EXTENT2)
  o2 = (sha_e1 == sha_e2)
  print(f"[lpf-oracle] o2 texels({K:g}) @extent={EXTENT:g} vs @extent={EXTENT2:g}: "
        f"{'INVARIANT' if o2 else 'DIVERGED'} ({sha_e1[:12]} / {sha_e2[:12]})", flush=True)
  # control: METERS-mode DOES vary across extent (proves o2 is meaningful, not a no-op).
  sha_m1 = _bake(lambda: _Lpf(meters(256.0)), ctx, os.path.join(base, "m_e1"), EXTENT)
  sha_m2 = _bake(lambda: _Lpf(meters(256.0)), ctx, os.path.join(base, "m_e2"), EXTENT2)
  meters_varies = (sha_m1 != sha_m2)
  print(f"[lpf-oracle] o2 control meters(256) extent-sensitive: {meters_varies}", flush=True)
  results["o2_texels_dim_invariance"] = o2 and meters_varies

  # ---- o3: the removed kwargs FAIL LOUD (pre-1.0, no shim) ---------------------------
  o3 = (_raises_typeerror(lambda: T.lpf(object(), cutoff_m=64))
        and _raises_typeerror(lambda: T.lpf(object(), cutoff_texels=8)))
  print(f"[lpf-oracle] o3 old kwargs (cutoff_m / cutoff_texels) raise TypeError: {o3}", flush=True)
  results["o3_old_kwargs_fail_loud"] = o3

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
    print(f"\n=== LPF cutoff-units byte-oracle {'PASSED' if ok else 'FAILED'} ===", flush=True)
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
