#!/usr/bin/env python3
###############################################################################
# JUL09 S1 GATE — stale-dim artifact rebake (headless, mac).
#
# Repro of the owner's crash: `ork.terrain.viewer2.py xxx` asserted
# `spec.width == dim` (terrain_chunk_drawable.cpp:471) after the cook line
# "0 cache-loaded, N demand-skipped, 0 computed" — the bake fully SKIPPED because
# name-keyed final products already existed on disk, but baked at a DIFFERENT dim
# than the exported scene requested; materialize then rewrote the manifest at the
# new dim while the stale EXR kept the old dim -> drawable assert.
#
# This bakes the same terrain (same hf_asset name -> same <assetcache>/terrain/<name>)
# at dim A, then exports+plays at dim B. WITH the stale-params guard (asset_gen.cpp
# HeightFieldGenData::materialize) the dim-B run must: log "bake params changed
# (dim A->B ...)", RECOMPUTE (cook computed>0, not "0 computed"), render (no assert,
# rc=0), and produce a LIT snapshot. (Pre-fix: it served the stale EXR and asserted.)
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, re, time, shutil, tempfile, textwrap, subprocess

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Path as _Path

from ork.editor.terrain_runtime import TerrainRuntime

DIM_A = 512
DIM_B = 256
EXTENT_M = 512.0
HEIGHT_M = 60.0
# match BOTH cook line forms; group 1 = computed. cache=True: "cacheable bake: L cache-loaded,
# S demand-skipped, C computed"; the stale-dim SKIP (RED) prints "0 cache-loaded, N demand-skipped,
# 0 computed" — after the guard (GREEN) it recomputes (computed>0).
_COOK_CACHEABLE = re.compile(r"cacheable bake:\s+\d+ cache-loaded,\s+\d+ demand-skipped,\s+(\d+) computed")

# cache=True fixture — its captures ride the disk cook cache, so a stale-dim bake CAN skip (the RED
# condition). voronoi is cache=False (always recomputes) and would never reproduce the skip. A unique
# per-run frequency salt keeps BOTH dims COLD in the per-node dflowcache, so a GREEN recompute shows
# computed>0 (a warm dflowcache from a prior run would load instead, masking the recompute count).
_SALT = round(3.0 + (time.time() % 1000.0) * 0.000137, 6)
FIXTURE = textwrap.dedent(f"""
    from ork.hypergraph.dflow.terrain import HeightField
    from ork.hypergraph.dflow import terrain as T

    class DimTestHF(HeightField):
        EXTENT_M = 512.0
        HEIGHT_M = 60.0
        def __init__(self):
            super().__init__()
            h = T.Fbm(frequency={_SALT}, octaves=6) * 0.5 + 0.5
            self.capture(h, "height", cache=True)
""")


def _export(ez, ctx, dim, path, dsl_path):
  rt = TerrainRuntime(preview_dim=dim, chunk=128)
  rt.load(dsl_path, extent_m=EXTENT_M, height_m=HEIGHT_M)
  rt.set_context(ctx)
  rt.export_scene_json(path, dim=dim, simple_material=True)


def _play(ecs_path, snap=None, timeout=110):
  player = shutil.which("ork.ecs.player.exe")
  assert player, "ork.ecs.player.exe not on PATH"
  cmd = [player, ecs_path, "--offscreen", "--frames", "600"]
  if snap:
    cmd += ["-S", snap, "-F", "30", "--camdist", "220", "--camheight", "90"]
  return subprocess.run(cmd, timeout=timeout, capture_output=True, text=True)


def main():
  work = tempfile.mkdtemp(prefix="tered_dimrebake_")
  ecs_a = os.path.join(work, "dimA.ecs")
  ecs_b = os.path.join(work, "dimB.ecs")
  snap = os.path.join(work, "dimB.png")
  dsl_path = os.path.join(work, "dimtest.py")
  with open(dsl_path, "w") as f:
    f.write(FIXTURE)

  # clean the name-keyed artifact dir so dim A is a COLD bake (the terrain is named "terra"
  # by TerrainRuntime.build_scene_data -> <assetcache>/terrain/terra).
  terra_dir = str(_Path.expandPathString("<assetcache>/terrain/terra"))
  shutil.rmtree(terra_dir, ignore_errors=True)

  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  _export(ez, ctx, DIM_A, ecs_a, dsl_path)
  _export(ez, ctx, DIM_B, ecs_b, dsl_path)
  ez.mainThreadEnd()

  # 1) bake dim A (writes the name-keyed artifacts at DIM_A)
  ra = _play(ecs_a)
  print(f"[dimrebake] dim A={DIM_A} bake rc={ra.returncode}", flush=True)

  # 2) request dim B on the SAME terrain -> the guard must invalidate + recompute
  rb = _play(ecs_b, snap=snap)
  out = (rb.stdout or "") + (rb.stderr or "")
  guard_line = next((ln for ln in out.splitlines() if "bake params changed" in ln), None)
  # the player materializes TWICE (materializeAndWireScene pre-bind -> the GUARDED recompute;
  # then AssetSystem::_onGpuInit -> a warm re-materialize that skips). Take the MAX computed
  # across all cook lines -> the guarded recompute (RED skip was 0 computed on every line).
  computed_vals = [int(m.group(1)) for ln in out.splitlines()
                   if (m := _COOK_CACHEABLE.search(ln))]
  computed = max(computed_vals) if computed_vals else None
  asserted = ("spec.width == dim" in out) or (rb.returncode not in (0,))
  snap_ok = os.path.exists(snap) and os.path.getsize(snap) > 0

  # vet the snapshot LIT
  lit = False; dyn = "(no snapshot)"
  if snap_ok:
    vet = shutil.which("ork.vet.image.py")
    vr = subprocess.run([vet, snap, "--kind", "render", "--max-highband", "1.0",
                         "--max-spike", "1.0", "--min-range", "0.02"], capture_output=True, text=True)
    dyn = next((l for l in (vr.stdout or "").splitlines()
                if l.split("\t")[0] == "frame.dynamic_range"), "(no line)")
    lit = (len(dyn.split("\t")) >= 4 and dyn.split("\t")[3] == "PASS")

  print(f"[dimrebake] dim B={DIM_B} rc={rb.returncode}", flush=True)
  print(f"[dimrebake] guard log line: {guard_line!r}", flush=True)
  print(f"[dimrebake] dim-B cacheable-bake computed = {computed}", flush=True)
  print(f"[dimrebake] snapshot LIT: {dyn}", flush=True)

  guard_ok = guard_line is not None and (f"dim {DIM_A}->{DIM_B}" in guard_line)
  recompute_ok = (computed is not None and computed > 0)   # computed>0 (RED skip was "0 computed")
  no_assert = (rb.returncode == 0) and not asserted

  ok = guard_ok and recompute_ok and no_assert and lit
  print(f"\n=== terrain stale-dim rebake gate {'PASSED' if ok else 'FAILED'} ===", flush=True)
  print(f"    guard_fired (dim {DIM_A}->{DIM_B}): {'ok' if guard_ok else 'FAIL'}", flush=True)
  print(f"    recomputed (computed>0): {'ok' if recompute_ok else 'FAIL'}", flush=True)
  print(f"    no_assert (rc=0): {'ok' if no_assert else 'FAIL'}", flush=True)
  print(f"    snapshot_lit: {'ok' if lit else 'FAIL'}", flush=True)
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


main()
