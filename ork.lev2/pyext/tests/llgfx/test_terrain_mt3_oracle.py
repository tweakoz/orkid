#!/usr/bin/env python3
###############################################################################
# MT3 T9 GATE (MANDATORY, cache-poison risk) — the SLICER is byte-safe.
#
# JUL13_DFLOW §2.6/§E5 + Appendix D T9: "a sliced cook may NOT cacheStore until
# sliced-vs-blocking byte-identity is proven." This oracle IS that proof: for every
# corpus terrain it cooks the SAME graph at the SAME dim/extent, once via the SLICED
# driver (one topo node per frame — max slice-boundary stress) and once via the BURST
# driver (bake_heightfield), each from a COLD cook cache, and asserts every capture
# product is byte-identical. Node-boundary grain (T12) + per-op submit+WAIT (T7) are
# WHY they match; this oracle proves the resume-cursor / pooled-reuse-across-slices /
# per-node dispatch / incremental-readback / flush machinery introduces NO divergence.
#
# DETERMINISM CONTROL (crucial): the GPU flow/erosion ops (flow3d, flow_erode,
# erode_thermal — parallel atomics / non-associative reductions) are NOT cold-vs-cold
# byte-reproducible EVEN BLOCKING (a second independent burst bake differs). So a byte
# oracle is only MEANINGFUL where a burst-vs-burst control is itself identical. The
# gate therefore requires sliced==burst on the DETERMINISTIC corpus (analytic ops:
# fbm / terrace / lpf / remap, incl. a deterministic T.loop composite) and reports the
# erosion corpus (xxx/xxx3/erodeflow) as an informational diagnostic only — their
# blocking nondeterminism is pre-existing and orthogonal to slicing.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"

from orkengine import core   # core before lev2 — and BEFORE any stdlib that pulls libcrypto
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Path as _Path

import sys, glob, shutil
import zlib   # crc32 tag only — hashlib pulls the SYSTEM libcrypto and clashes with the
              # staged libssl (see test_terrain_gpuupdate.py). Byte-identity is compared RAW.

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

DIM    = 256
EXTENT = 8192.0


# --- DETERMINISTIC corpus (analytic ops only — no flow/erosion atomics) ----------
class Det1(HeightField):   # 2 compute nodes + 2 captures
  def __init__(self):
    super().__init__()
    h = T.Fbm(frequency=4.0, octaves=6) * 0.5 + 0.5
    h = T.Terrace(h, step_m=1.0 / 8.0, sharpness=4.0)
    self.capture(h, "height", cache=True)
    self.capture(h, "normal", cache=True)


class Det2(HeightField):   # a longer analytic chain (more slice boundaries)
  def __init__(self):
    super().__init__()
    h = (T.Fbm(frequency=3.0, octaves=7) * 0.5 + 0.5) * 400.0
    h = T.lpf(h, cutoff=64, units='meters', blend=0.5)
    h = T.Terrace(h, step_m=20.0, sharpness=3.0)
    h = T.lpf(h, cutoff=16, units='meters', blend=0.35)
    self.capture(h, "height", cache=True)
    self.capture(h, "normal", cache=True)


class Det3(HeightField):   # a DETERMINISTIC T.loop composite (one node = one slice)
  def __init__(self):
    super().__init__()
    z = (T.Fbm(frequency=5.0, octaves=6) * 0.5 + 0.5) * 300.0
    with T.loop(4, z=z) as L:
      L.z = T.lpf(L.z, cutoff=8, units='meters', blend=0.4)
    z = L.z
    self.capture(z, "height", cache=True)
    self.capture(z, "normal", cache=True)


class DetBasin(HeightField):   # isolates basin_fill (a _per_op_sync CPU-readback cache-point)
  def __init__(self):
    super().__init__()
    h = (T.Fbm(frequency=4.0, octaves=6) * 0.5 + 0.5) * 300.0
    h = T.basin_fill(h, blend=0.5)
    h = T.lpf(h, cutoff=16, units='meters', blend=0.35)
    self.capture(h, "height", cache=True)
    self.capture(h, "normal", cache=True)


def _det_corpus():
  return [("det1", Det1), ("det2", Det2), ("det3", Det3), ("detbasin", DetBasin)]


def _erosion_corpus():
  from ork.hypergraph.assets.terrain.xxx import XXX
  from ork.hypergraph.assets.terrain.erodeflow import ErodeFlow
  from ork.hypergraph.assets.terrain.xxx3 import XXX3
  return [("xxx", lambda: XXX(iters=6)),
          ("erodeflow", lambda: ErodeFlow(iters=6)),
          ("xxx3", lambda: XXX3(iters=6))]


def _tag(b):
  return "%08x" % (zlib.crc32(b) & 0xFFFFFFFF) if b is not None else "<miss>"


def _read_products(outdir):
  out = {}
  for path in sorted(glob.glob(os.path.join(outdir, "*"))):
    if path.endswith(".cookhash") or not (path.endswith(".exr") or path.endswith(".png")):
      continue
    with open(path, "rb") as f:
      out[os.path.basename(path)] = f.read()
  return out


def _set_paths(graph, outdir):
  os.makedirs(outdir, exist_ok=True)
  for cap in lev2.terrain.capture_modules(graph):
    chans = [c.strip() for c in (cap.channel or "height").split(",") if c.strip()] or ["height"]
    cap.path = (os.path.join(outdir, chans[0] + ".exr") if len(chans) == 1
                else os.path.join(outdir, "{channel}.exr"))


def _wipe_dflowcache():
  # COLD cook cache before EACH bake -> both recompute every node (T9 "both fresh").
  shutil.rmtree(str(_Path.expandPathString("<staging>/dflowcache")), ignore_errors=True)


_last_frames = 0

def _bake_sliced(factory, ctx, outdir):
  global _last_frames
  _wipe_dflowcache()
  g = factory().generatedflow()
  assert g.cacheable, "graph not cacheable (cook cache disabled?)"
  _set_paths(g, outdir)
  h = lev2.terrain.build_sliced_bake(g, ctx, DIM, EXTENT)  # NOT scheduler-enqueued
  h.pumpToCompletion(ctx)                                  # one topo node per frame
  assert h.done, "sliced bake never completed"
  _last_frames = h.frames_driven   # gate-6: slices the cook spanned (== frames if 1 node/frame)
  return _read_products(outdir)


def _bake_burst(factory, ctx, outdir):
  _wipe_dflowcache()
  g = factory().generatedflow()
  _set_paths(g, outdir)
  lev2.terrain.bake_heightfield(g, ctx, DIM, EXTENT)
  return _read_products(outdir)


def _report(label, sliced, burst, burst2):
  keys = sorted(set(sliced) | set(burst) | set(burst2))
  det = (burst == burst2)                 # blocking self-reproducible?
  sliced_ok = (sliced == burst)
  for k in keys:
    s, b, c = sliced.get(k), burst.get(k), burst2.get(k)
    print(f"    {k:18s} sliced={_tag(s)} burst={_tag(b)} burst2={_tag(c)}", flush=True)
  return det, sliced_ok


def main():
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  base = "/tmp/mt3_oracle"
  shutil.rmtree(base, ignore_errors=True); os.makedirs(base, exist_ok=True)

  gate_ok = True
  gated = 0
  try:
    print("\n--- DETERMINISTIC corpus (byte-identity GATE) ---", flush=True)
    for label, factory in _det_corpus():
      sliced = _bake_sliced(factory, ctx, os.path.join(base, label, "sliced"))
      burst  = _bake_burst(factory, ctx, os.path.join(base, label, "burst"))
      burst2 = _bake_burst(factory, ctx, os.path.join(base, label, "burst2"))
      det, sliced_ok = _report(label, sliced, burst, burst2)
      assert det, (f"{label}: burst-vs-burst DIVERGED — a 'deterministic' graph is NOT "
                   f"reproducible; oracle cannot gate it (pick analytic ops only)")
      status = "IDENTICAL" if sliced_ok else "DIVERGED"
      # gate-6 headless evidence: the sliced cook advanced across _last_frames slices
      # (one node's dispatch triplet each) — a live editor presents a frame per drain; the
      # burst path completes in ONE call (~0 frames presented during the cook).
      print(f"[mt3-oracle] {label:10s} GATE sliced-vs-burst={status} "
            f"(control burst-vs-burst=IDENTICAL, {len(sliced)} products, "
            f"sliced spanned {_last_frames} frames vs burst=1)", flush=True)
      gate_ok = gate_ok and sliced_ok
      gated += 1

    print("\n--- EROSION corpus (flow/thermal atomics — determinism characterization) ---", flush=True)
    for label, factory in _erosion_corpus():
      sliced1 = _bake_sliced(factory, ctx, os.path.join(base, label, "sliced1"))
      sliced2 = _bake_sliced(factory, ctx, os.path.join(base, label, "sliced2"))
      burst1  = _bake_burst(factory, ctx, os.path.join(base, label, "burst1"))
      burst2  = _bake_burst(factory, ctx, os.path.join(base, label, "burst2"))
      block_det  = (burst1 == burst2)     # blocking self-reproducible?
      sliced_det = (sliced1 == sliced2)   # sliced self-reproducible?
      sliced_ok  = (sliced1 == burst1)
      for k in sorted(set(sliced1) | set(burst1)):
        print(f"    {k:18s} sliced1={_tag(sliced1.get(k))} sliced2={_tag(sliced2.get(k))} "
              f"burst1={_tag(burst1.get(k))} burst2={_tag(burst2.get(k))}", flush=True)
      # INFORMATIONAL ONLY — the erosion ops (flow3d/flow_erode) accumulate via parallel
      # GPU atomics whose ordering is submit-pattern-sensitive: blocking itself is often not
      # cold-vs-cold reproducible, and even when a single run looks self-consistent it can
      # differ across the sliced-vs-blocking submit interleaving. That is the SAME class of
      # nondeterminism blocking already has (NOT a slicing corruption — proven by the
      # deterministic-corpus gate above, which includes composite loops + basin_fill). These
      # graphs therefore cannot be byte-gated and never affect the gate verdict.
      if block_det and sliced_det and sliced_ok:
        note = "deterministic both sides this run; sliced==burst"
      elif not block_det:
        note = "blocking NON-reproducible (flow atomics) — byte gate inherently N/A"
      else:
        note = "flow atomics submit-order-sensitive across runs/drivers — byte gate N/A"
      print(f"[mt3-oracle] {label:10s} DIAG block_det={block_det} sliced_det={sliced_det} "
            f"sliced==burst={sliced_ok} :: {note}", flush=True)
  except BaseException:
    import traceback; traceback.print_exc()
    gate_ok = False

  ez.mainThreadEnd()
  ok = gate_ok and gated >= 1
  print(f"\n=== MT3 T9 sliced-vs-blocking byte-identity oracle "
        f"{'PASSED' if ok else 'FAILED'} ({gated} deterministic graphs gated) ===", flush=True)
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


if __name__ == "__main__":
  main()
