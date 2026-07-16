#!/usr/bin/env ork.python
"""
Phase-1 acceptance for the HyperSyn unified procedural substrate (ExprModule / hfbake).

Bakes two ptex3d EXPRESSIONS authored with P + ctx (the SAME surface a Ptex3d material
uses) to channels via self.hfbake -> a generic compute ExprModule:
  * stripes  : pure-ALU (sin/dot)        -> proves emit_compute_field + the compute shell
  * mottle   : noise (lib_pnoise)        -> proves the canonical noise runs in COMPUTE
and verifies both EXRs land with real variation. See UNIFIED_SUBSTRATE.md §9 (Phase 1).

GPU lifecycle mirrors test_terrain_bake.py. Requires the C++ ExprModule (ork.build.py).
"""
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs


def run(ez, ctx):
  print("=" * 60, flush=True)
  print("Phase-1 acceptance: hfbake (ExprModule) -> data channels", flush=True)
  print("=" * 60, flush=True)

  from ork.hypergraph.ecs.scene.assets import HeightField as HFAsset
  hf = HFAsset(dsl_file="strata_bake", dsl_class="StrataBake", dimension=256,
               extent_m=4096.0, ctx=ctx)
  hf.gendata.asset_name = "phase1_hfbake"
  artifacts = hf.build(ext="exr")

  print("\n" + "=" * 60, flush=True)
  stats = artifacts.get("stats", {})

  def show(ch):
    p  = artifacts.get(ch)
    st = stats.get(ch)
    present = bool(p) and os.path.exists(p) and os.path.getsize(p) > 1000
    print(f"{ch:11s}: exists={present} min={getattr(st,'min',None):.4f} "
          f"max={getattr(st,'max',None):.4f}  ({p})", flush=True)
    return present, st

  results = {}
  # ExprModule: both channels present + actually varying.
  for ch in ("stripes", "mottle"):
    present, st = show(ch)
    results[f"{ch}_varied"] = bool(present and st is not None and (st.max - st.min) > 1e-3)

  # NormalizeModule: `compressed` is narrow (~[0.5,0.8]); `normalized` spans ~[0,1].
  # (The check is on raw FieldStats — the flush re-exposes every stored EXR to [0,1].)
  cp, cst = show("compressed")
  np_, nst = show("normalized")
  comp_narrow = cst is not None and (cst.max - cst.min) < 0.5 and cst.min > 0.30
  norm_full   = nst is not None and nst.min < 0.05 and nst.max > 0.95
  print(f"normalize check: compressed_narrow={comp_narrow}  normalized_full={norm_full}", flush=True)
  results["compressed_narrow"] = bool(cp and comp_narrow)
  results["normalized_full"]   = bool(np_ and norm_full)

  # Phase-2: hfdisplacement (multi-input ExprModule reading the height via In0).
  tp, tst = show("terraced")
  terr_ok = tp and tst is not None and (tst.max - tst.min) > 1e-3
  print(f"displacement check: terraced present+varied={terr_ok}", flush=True)
  results["terraced_varied"] = bool(terr_ok)

  return results


def main():
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  results = run(ez, ctx)
  ez.mainThreadEnd()
  ok = all(results.values())
  print("=" * 60)
  print("PHASE-1/2 hfbake + normalize + hfdisplacement:", "GREEN" if ok else "RED")
  for k, v in results.items():
    print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
  print("=" * 60)
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


if __name__ == "__main__":
  main()
