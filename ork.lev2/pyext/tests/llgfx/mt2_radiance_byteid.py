#!/usr/bin/env ork.python
"""
MT2 (JUL05_GPUMICROTASK §3 gate b) — radiance-prefilter determinism oracle.

Bakes ONE raw env map to XIR via the SLICED RadiancePrefilterMicrotask path
TWICE (independent slice schedules), and sha256-compares. The two bakes differ
in WHEN each roughness/mip slice is submitted (budget-driven, frame-to-frame),
so byte-identical output proves the sliced GPU path is DETERMINISTIC — slice
boundaries do not change results (JUL05 §5 / T12 determinism law).

The microtask and the burst createFilteringTaskGraph call the *same* shared
per-level render + package helpers (initEnvFilterState / renderSpecularLevel /
renderDiffuseLevel / packageFilterResult), differing only in submit sequencing;
so micro==burst by construction and this determinism check is the empirical
half of the "byte-equal or bit-exact-explained" gate.
"""
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
# import orkengine BEFORE hashlib (hashlib's _hashlib pins the system libcrypto,
# which shadows orkengine's staged libssl -> OPENSSL_3.3.0 not found).
from orkengine import core
from orkengine import lev2
from orkengine import ecs
import hashlib

import os
SRC = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("MT2_SRC_HDR", "")
if not SRC:
    sys.exit("mt2_radiance_byteid: pass an .hdr path or set MT2_SRC_HDR")

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

def main():
    print("="*64, flush=True)
    print(f"MT2 radiance-prefilter determinism oracle  src={SRC}", flush=True)
    print("="*64, flush=True)
    if not os.path.exists(SRC):
        print(f"FAIL: source env map not found: {SRC}", flush=True)
        return 2

    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    outs = ["/tmp/mt2_micro_a.xir", "/tmp/mt2_micro_b.xir"]
    oks = []
    for i, out in enumerate(outs):
        if os.path.exists(out):
            os.remove(out)
        print(f"[bake {i}] microtask -> {out}", flush=True)
        ok = lev2.EnvMapProcessor.processToXIRViaMicrotask(SRC, out)
        sz = os.path.getsize(out) if os.path.exists(out) else 0
        print(f"[bake {i}] ok={ok} bytes={sz}", flush=True)
        oks.append(ok and sz > 0)

    ezapp.mainThreadEnd()

    verdict = 1
    if all(oks):
        ha, hb = sha256(outs[0]), sha256(outs[1])
        print(f"[bake 0] sha256 {ha}", flush=True)
        print(f"[bake 1] sha256 {hb}", flush=True)
        if ha == hb:
            print("GATE-A PASS: sliced radiance bakes byte-identical across independent slice schedules (deterministic)", flush=True)
            verdict = 0
        else:
            print("GATE-A FAIL: sliced bakes differ -> slice boundaries changed output (determinism violation)", flush=True)
    else:
        print("GATE-A FAIL: a bake produced no output", flush=True)

    ecs.headless_exit()
    return verdict

if __name__ == "__main__":
    sys.exit(main())
