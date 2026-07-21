#!/usr/bin/env python3
###############################################################################
# S4 PROGRESSIVE-DISPLAY GATE (JUL13_DFLOW §E5 / Appendix A §S4).
#
# Proves, headless:
#  (2) BYTE-IDENTITY (the T9-class mandatory gate): final capture products with
#      visual_update_mode=on_checkpoint (sliced) == on_complete (burst) — the
#      progressive publishes must not change the final bytes. Burst-vs-burst is
#      the determinism control (analytic ops only, per the MT3 oracle's rule).
#  (5) CHECKPOINT OBSERVABLE: a sliced re-bake with on_checkpoint logs N>1
#      "[s4-live] publish" events with MONOTONICALLY-ADVANCING node coverage and a
#      strictly-increasing LiveOutput generation counter; the artifact is ARMED at
#      plan and FINAL at flush, and the registry's generation delta matches.
#  (K) KILL-SWITCH: ORKID_S4_DISABLE=1 produces ZERO publish events and identical
#      final bytes (pure on_complete behavior).
#
# Parent/worker split: the [s4-live] evidence is C++ stdout, so the parent re-runs
# this file as a subprocess per phase and parses the captured log; the worker does
# the bakes + byte compares + registry asserts.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"

import re
import subprocess
import sys

DIM    = 256
EXTENT = 8192.0
BASE   = "/tmp/s4_progressive"


###############################################################################
# worker — the actual bakes (runs in a subprocess so the parent owns the log)
###############################################################################

def worker(disabled):
  from orkengine import core   # core before lev2
  from orkengine import lev2
  from orkengine import ecs
  from orkengine.core import Path as _Path

  import glob, shutil

  from ork.hypergraph.dflow.terrain import HeightField
  from ork.hypergraph.dflow import terrain as T

  class Chain(HeightField):    # 4 analytic compute nodes -> >1 checkpoint by construction
    def __init__(self):
      super().__init__()
      h = (T.Fbm(frequency=3.0, octaves=7) * 0.5 + 0.5) * 400.0
      h = T.lpf(h, cutoff=64, units='meters', blend=0.5)
      h = T.Terrace(h, step_m=20.0, sharpness=3.0)
      h = T.lpf(h, cutoff=16, units='meters', blend=0.35)
      self.capture(h, "height", cache=True)
      self.capture(h, "normal", cache=True)

  def wipe_dflowcache():
    shutil.rmtree(str(_Path.expandPathString("<staging>/dflowcache")), ignore_errors=True)

  def set_paths(graph, outdir, checkpoint):
    os.makedirs(outdir, exist_ok=True)
    height_path = None
    for cap in lev2.terrain.capture_modules(graph):
      chans = [c.strip() for c in (cap.channel or "height").split(",") if c.strip()] or ["height"]
      cap.path = (os.path.join(outdir, chans[0] + ".exr") if len(chans) == 1
                  else os.path.join(outdir, "{channel}.exr"))
      if "height" in chans:
        height_path = os.path.join(outdir, "height.exr")
        if checkpoint:
          cap.visual_update_mode = "on_checkpoint"
    return height_path

  def read_products(outdir):
    out = {}
    for path in sorted(glob.glob(os.path.join(outdir, "*"))):
      if path.endswith(".cookhash") or not path.endswith(".exr"):
        continue
      with open(path, "rb") as f:
        out[os.path.basename(path)] = f.read()
    return out

  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  ok = True
  try:
    sub = os.path.join(BASE, "disabled" if disabled else "enabled")
    import shutil as _sh
    _sh.rmtree(sub, ignore_errors=True)

    # burst on_complete — the byte BASELINE (today's behavior)
    wipe_dflowcache()
    g = Chain().generatedflow()
    assert g.cacheable, "corpus graph must be cacheable (the checkpoint-capable branch)"
    set_paths(g, os.path.join(sub, "burst"), checkpoint=False)
    lev2.terrain.bake_heightfield(g, ctx, DIM, EXTENT)
    burst = read_products(os.path.join(sub, "burst"))

    # burst determinism control
    wipe_dflowcache()
    g2 = Chain().generatedflow()
    set_paths(g2, os.path.join(sub, "burst2"), checkpoint=False)
    lev2.terrain.bake_heightfield(g2, ctx, DIM, EXTENT)
    burst2 = read_products(os.path.join(sub, "burst2"))
    assert burst == burst2, "burst-vs-burst DIVERGED — corpus not deterministic, gate invalid"

    # sliced + on_checkpoint — the S4 path (one topo node per frame, max boundary stress)
    wipe_dflowcache()
    g3 = Chain().generatedflow()
    hpath = set_paths(g3, os.path.join(sub, "sliced"), checkpoint=True)
    lf   = lev2.terrain.live_field_acquire(hpath)
    gen0 = lf.generation
    h = lev2.terrain.build_sliced_bake(g3, ctx, DIM, EXTENT)  # NOT scheduler-enqueued
    h.pumpToCompletion(ctx)
    assert h.done, "sliced bake never completed"
    sliced = read_products(os.path.join(sub, "sliced"))

    # (2) byte-identity: on_checkpoint == on_complete
    for k in sorted(set(sliced) | set(burst)):
      same = (sliced.get(k) == burst.get(k))
      print(f"[s4-worker] product {k}: {'IDENTICAL' if same else 'DIVERGED'}", flush=True)
      ok = ok and same

    # (5) registry observable — generation delta + lifecycle
    gen1 = lf.generation
    if disabled:
      assert gen1 == gen0, f"S4 disabled but generation moved ({gen0} -> {gen1})"
      assert not lf.is_live, "S4 disabled but artifact is live"
      print(f"[s4-worker] DISABLED: generation held at {gen1}; products identical={ok}", flush=True)
    else:
      # 4 viewable compute nodes + the flush-final publish -> at least 3 increments
      assert gen1 - gen0 > 1, f"expected N>1 publishes, generation moved {gen0} -> {gen1}"
      assert lf.is_final, "artifact not FINAL after the flush"
      assert not lf.is_live, "artifact still LIVE after the flush"
      print(f"[s4-worker] generation {gen0} -> {gen1} (N={gen1 - gen0} publishes), "
            f"final={lf.is_final}", flush=True)
  except BaseException:
    import traceback; traceback.print_exc()
    ok = False

  ez.mainThreadEnd()
  print(f"[s4-worker] {'PASSED' if ok else 'FAILED'}", flush=True)
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


###############################################################################
# parent — captures the worker's C++ stdout and asserts the log-line contract
###############################################################################

_PUB_RE = re.compile(r"\[s4-live\] publish key<[^>]*> gen<(\d+)> node<([^>]*)> coverage<([^>]*)>")


def run_phase(disabled):
  env = dict(os.environ)
  if disabled:
    env["ORKID_S4_DISABLE"] = "1"
  else:
    env.pop("ORKID_S4_DISABLE", None)
  proc = subprocess.run(
      [sys.executable, os.path.abspath(__file__), "--worker"] + (["--disabled"] if disabled else []),
      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env, text=True, timeout=1800)
  sys.stdout.write(proc.stdout)
  return proc.returncode, proc.stdout


def main():
  fails = []

  # --- enabled phase: byte-identity + N>1 monotonic publishes + ARMED/FINAL ---
  rc, log = run_phase(disabled=False)
  if rc != 0:
    fails.append(f"enabled worker rc={rc}")
  pubs = _PUB_RE.findall(log)
  node_pubs = [(int(g), cov) for (g, n, cov) in pubs if cov != "final"]
  if len(node_pubs) <= 1:
    fails.append(f"expected N>1 checkpoint publish events, got {len(node_pubs)}")
  gens = [g for (g, _cov) in node_pubs]
  if gens != sorted(gens) or len(set(gens)) != len(gens):
    fails.append(f"generation counter not strictly increasing: {gens}")
  covs = []
  for (_g, cov) in node_pubs:
    a, b = cov.split("/")
    covs.append(int(a))
  if covs != sorted(covs):
    fails.append(f"node coverage not monotonically advancing: {covs}")
  if "[s4-live] ARMED" not in log:
    fails.append("no [s4-live] ARMED event")
  if "[s4-live] FINAL" not in log:
    fails.append("no [s4-live] FINAL event")
  print(f"[s4-gate] enabled: {len(node_pubs)} checkpoint publishes, coverage {covs}", flush=True)

  # --- kill-switch phase: ORKID_S4_DISABLE=1 -> zero publishes, identical bytes ---
  rc_d, log_d = run_phase(disabled=True)
  if rc_d != 0:
    fails.append(f"disabled worker rc={rc_d}")
  if _PUB_RE.findall(log_d):
    fails.append("ORKID_S4_DISABLE=1 but [s4-live] publish events were emitted")
  if "[s4-live] ARMED" in log_d:
    fails.append("ORKID_S4_DISABLE=1 but a live artifact was ARMED")
  print("[s4-gate] disabled: no publish events (kill-switch honored)", flush=True)

  ok = not fails
  for f in fails:
    print(f"[s4-gate] FAIL: {f}", flush=True)
  print(f"\n=== S4 progressive-display gate {'PASSED' if ok else 'FAILED'} ===", flush=True)
  sys.exit(0 if ok else 1)


if __name__ == "__main__":
  if "--worker" in sys.argv:
    worker(disabled=("--disabled" in sys.argv))
  else:
    main()
