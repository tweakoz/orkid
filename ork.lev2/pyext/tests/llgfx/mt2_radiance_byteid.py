#!/usr/bin/env ork.python
"""
MT2 (JUL05_GPUMICROTASK §3 gate b) — radiance-prefilter determinism oracle.

Bakes ONE raw env map to XIR via the SLICED RadiancePrefilterMicrotask path
TWICE (independent slice schedules), and sha256-compares. The two bakes differ
in WHEN each roughness/mip slice is submitted (budget-driven, frame-to-frame),
so byte-identical output proves the sliced GPU path is DETERMINISTIC — slice
boundaries do not change results (JUL05 §5 / T12 determinism law).

Then bakes the SAME source via the burst createFilteringTaskGraph and compares
that too (GATE-B): both paths share the per-level render/package helpers, but
since COMFORT-2 they no longer share the submit structure (the sliced path
batches a level's tiles and captures in its own submit), so micro==burst is a
measurement rather than a construction argument.
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

    burst_out = "/tmp/mt2_burst.xir"
    if os.path.exists(burst_out):
        os.remove(burst_out)
    print(f"[bake burst] taskgraph -> {burst_out}", flush=True)
    burst_ok = lev2.EnvMapProcessor.processToXIR(SRC, burst_out)
    burst_sz = os.path.getsize(burst_out) if os.path.exists(burst_out) else 0
    print(f"[bake burst] ok={burst_ok} bytes={burst_sz}", flush=True)
    oks.append(burst_ok and burst_sz > 0)

    ezapp.mainThreadEnd()

    failures = []
    if all(oks):
        ha, hb, hburst = sha256(outs[0]), sha256(outs[1]), sha256(burst_out)
        print(f"[bake 0]     sha256 {ha}", flush=True)
        print(f"[bake 1]     sha256 {hb}", flush=True)
        print(f"[bake burst] sha256 {hburst}", flush=True)
        if ha == hb:
            print("GATE-A PASS: sliced radiance bakes byte-identical across independent slice schedules (deterministic)", flush=True)
        else:
            failures.append("GATE-A")
            print("GATE-A FAIL: sliced bakes differ -> slice boundaries changed output (determinism violation)", flush=True)
        # GATE-B (COMFORT-2): the two paths are no longer the same code path with
        # different sequencing — the sliced one renders a level's tiles in
        # batched submits (LOAD-op resume), captures in a submit of its OWN, and
        # packages per level, while the burst taskgraph still does one submit per
        # level with the capture inline. "Byte-identical by construction" is
        # therefore no longer an argument; this is the measurement.
        if ha == hburst:
            print("GATE-B PASS: sliced bake byte-identical to the burst taskgraph bake (submit structure is pixel-neutral)", flush=True)
        else:
            failures.append("GATE-B")
            print("GATE-B FAIL: sliced bake differs from the burst bake -> the slice structure moved pixels", flush=True)
    else:
        failures.append("GATE-A")
        print("GATE-A FAIL: a bake produced no output", flush=True)

    ecs.headless_exit()
    return 0 if not failures else 1

if __name__ == "__main__":
    sys.exit(main())
