#!/usr/bin/env python3
###############################################################################
# Terrain test BATTERY — ONE engine init, ONE gfx bind, every terrain suite run
# back to back. Each factored suite file exposes run(ez, ctx) -> ordered
# {check_name: bool} against the ALREADY-initialized app; this harness pays the
# ~15s engine init (and its one flaky-GPU-init exposure) exactly once instead of
# per file, collapsing the whole battery to well under the 2-minute worst case.
# The suite files still run standalone (their main() owns its own lifecycle).
#
# A suite that raises = that suite FAILS (traceback printed) and the battery
# continues. A soft 100s self-budget prints a LOUD OVER BUDGET banner but never
# skips a suite. Exit 0 only when EVERY suite passes.
#
# (test_terrain_pybind / test_terrain_dsl are bring-up-era plumbing tests whose
#  coverage is subsumed by the suites below — intentionally not in the battery.)
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time, traceback, importlib
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

# fixed order: document/plumbing/editor suites first, GPU-render (instedge) last.
SUITES = [
    ("doc",            "test_terrain_doc"),
    ("constructs",     "test_terrain_constructs"),
    ("asset",          "test_terrain_asset"),
    ("hfbake",         "test_terrain_hfbake"),
    ("flags",          "test_terrain_flags"),
    ("undo",           "test_terrain_undo"),
    ("add",            "test_terrain_add"),
    ("kwarg_guard",    "test_terrain_kwarg_guard"),
    ("doc_params",     "test_terrain_doc_params"),
    ("typed_literals", "test_terrain_typed_literals"),
    ("pywriter",       "test_terrain_pywriter"),
    ("resugar",        "test_terrain_resugar"),
    ("graphdocument",  "test_dflow_graphdocument"),
    ("scatter_parity", "test_terrain_scatter_parity"),
    ("instedge",       "test_hypermesh_instedge"),
]

BUDGET_S = 100.0


def main():
    ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ez.mainThreadBegin()
    ctx = ez.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    t0 = time.perf_counter()
    suite_pass = {}
    suite_time = {}
    for label, modname in SUITES:
        elapsed = time.perf_counter() - t0
        if elapsed > BUDGET_S:
            print(f"\n[battery] OVER BUDGET at {label} "
                  f"(elapsed {elapsed:.1f}s > {BUDGET_S:.0f}s) — running anyway", flush=True)
        print(f"\n[battery] >>> {label} starting (t+{elapsed:.1f}s)", flush=True)
        ts = time.perf_counter()
        fails = []
        try:
            mod = importlib.import_module(modname)
            results = mod.run(ez, ctx)
            passed = bool(results) and all(results.values())
            fails = [k for k, v in results.items() if not v]
        except Exception:
            traceback.print_exc()
            passed = False
            fails = ["<exception>"]
        dt = time.perf_counter() - ts
        suite_pass[label] = passed
        suite_time[label] = dt
        print(f"[battery] <<< {label} {'PASSED' if passed else 'FAILED'} ({dt:.1f}s)"
              + (f"  fails={fails}" if not passed else ""), flush=True)

    ez.mainThreadEnd()

    total = time.perf_counter() - t0
    npass = sum(1 for v in suite_pass.values() if v)
    nall = len(SUITES)
    all_ok = (npass == nall)
    print("\n" + "=" * 66, flush=True)
    for label, _ in SUITES:
        print(f"  {label:16s} {'PASS' if suite_pass[label] else 'FAIL'}  "
              f"{suite_time[label]:6.1f}s", flush=True)
    print("=" * 66, flush=True)
    print(f"=== terrain battery {'PASSED' if all_ok else 'FAILED'} "
          f"({npass}/{nall} suites, {total:.0f}s total) ===", flush=True)
    ecs.headless_exit()
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
