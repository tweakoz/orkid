#!/usr/bin/env python3
###############################################################################
# M3: author terrain heightfields in the EXPRESSION-FIRST DSL, generatedflow(),
# and bake EXR channels. Exercises:
#   - RollingHills  : pure expression  (fbm * 0.5 + 0.5 -> Terrace)
#   - FractalRidges : trace-time `for` loop + conditional that UNROLL into DAG
#                     topology (octaves Fbm+Mul+Add modules; optional Terrace)
# proving Python control flow runs at trace time (graph metaprogramming), not at
# bake time. Each bakes to /tmp for visual inspection in Preview.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import math
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

DIM = 1024


class RollingHills(HeightField):
    def __init__(self, octaves=6, steps=6):
        super().__init__()
        h = T.Fbm(frequency=3.0, octaves=octaves) * 0.5 + 0.5   # operators -> Remap modules
        self.capture(T.Terrace(h, steps=steps, sharpness=4.0), "height")


class FractalRidges(HeightField):
    # trace-time loop unrolls into `octaves` Fbm+Mul+Add modules; the conditional
    # includes the Terrace module or not — all resolved before any bake runs.
    def __init__(self, octaves=5, terrace=True, terrace_steps=8):
        super().__init__()
        h, amp, freq, norm = T.Const(0.0), 1.0, 2.0, 0.0
        for _ in range(octaves):
            h = h + T.Fbm(frequency=freq, octaves=1) * amp   # accumulate weighted octaves
            norm += amp
            amp *= 0.5
            freq *= 2.0
        h = h * (1.0 / norm)                                 # normalize the sum -> ~[0,1]
        if terrace:
            h = T.Terrace(h, steps=terrace_steps, sharpness=3.0)
        self.capture(h, "height")


class TiltedNoise(HeightField):
    # exercises the vec2 "dir" plug THROUGH the DSL/pybind setter
    # (T.Gradient -> m.inputs.dir = vec2(dir_x, dir_y)): a diagonal ramp
    # (dir is non-axis-aligned, so both components matter) plus fbm detail.
    def __init__(self, octaves=5):
        super().__init__()
        ramp   = T.Gradient(dir_x=0.6, dir_y=0.8, scale=0.5, bias=0.0)
        detail = T.Fbm(frequency=4.0, octaves=octaves) * 0.25
        self.capture(T.Clamp(ramp + detail, 0.0, 1.0), "height")


def _bake(hf, channel, path, ctx):
    g = hf.generatedflow()
    hf.set_capture_path(channel, path)
    nmods = g.numModules() if hasattr(g, "numModules") else "?"
    stats = lev2.terrain.bake_heightfield(g, ctx, DIM)
    print(f"  channels={hf.channels} modules={nmods} stats={stats}", flush=True)
    return stats


def _valid(stats, path):
    if not (os.path.exists(path) and len(stats) == 1):
        return False
    s = stats[0]
    # natural units: stats are the RAW field values (meters) — assert sanity, not [0,1]
    import math
    return (math.isfinite(s.min) and math.isfinite(s.max) and s.min <= s.max
            and (s.max - s.min) > 0.03)


def main():
    outs = {"rolling": "/tmp/terrain_rolling.exr", "ridges": "/tmp/terrain_ridges.exr",
            "tilted": "/tmp/terrain_tilted.exr"}
    for p in outs.values():
        if os.path.exists(p):
            os.remove(p)

    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    print("RollingHills (pure expression):", flush=True)
    s1 = _bake(RollingHills(octaves=6, steps=6), "height", outs["rolling"], ctx)
    print("FractalRidges (trace-time loop unroll + conditional):", flush=True)
    s2 = _bake(FractalRidges(octaves=5, terrace=True, terrace_steps=8), "height", outs["ridges"], ctx)
    print("TiltedNoise (vec2 'dir' plug via DSL setter):", flush=True)
    s3 = _bake(TiltedNoise(octaves=5), "height", outs["tilted"], ctx)

    ezapp.mainThreadEnd()

    ok = _valid(s1, outs["rolling"]) and _valid(s2, outs["ridges"]) and _valid(s3, outs["tilted"])
    print(f"=== terrain DSL {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for _, p in outs.items():
        print(f"    {p} exists={os.path.exists(p)} size={os.path.getsize(p) if os.path.exists(p) else 0}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
