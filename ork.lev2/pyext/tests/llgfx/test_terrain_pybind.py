#!/usr/bin/env python3
###############################################################################
# M1+M2: build a terrain compute-dataflow graph FROM PYTHON via the bound module
# classes + the generic GraphData create/connect surface, then bake it. Proves
# the pyext bindings (lev2.terrain.*Module + bake_heightfield) work end to end —
# the plumbing the expression-first terrain DSL family will sit on top of.
#
#   fbm(octaves=5, freq=3) -> remap(*0.5+0.5) -> terrace(steps=6) -> capture
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import math
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import dataflow

OUT = "/tmp/terrain_pybind.exr"


def main():
    if os.path.exists(OUT):
        os.remove(OUT)

    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    T = lev2.terrain
    g = dataflow.GraphData.createShared()

    fbm = g.create("fbm", T.FbmModule)
    fbm.octaves = 5                       # baked ctor arg (def_readwrite)
    fbm.inputs.frequency = 3.0            # float plug via the generic inputs proxy

    remap = g.create("remap", T.RemapModule)
    remap.inputs.scale = 1.0
    remap.inputs.bias = 0.0               # fbm -> ~[0.5,1]

    terr = g.create("terr", T.TerraceModule)
    terr.inputs.step_m = 1.0 / 12.0   # meters per plateau (fixture field spans ~[0.5,1] m -> ~6 benches)
    terr.inputs.sharpness = 4.0

    remap2 = g.create("remap2", T.RemapModule)
    remap2.inputs.scale = 1.0
    remap2.inputs.bias = 0.0               # fbm -> ~[0.5,1]

    cap = g.create("cap", T.CaptureModule)
    cap.path = OUT

    g.connect(remap.inputs.In, fbm.outputs.Out)   # DAG edges (input, output)
    g.connect(terr.inputs.In, remap.outputs.Out)
    g.connect(remap2.inputs.In, terr.outputs.Out)
    g.connect(cap.inputs.In, remap2.outputs.Out)

    stats = T.bake_heightfield(g, ctx, 512)
    ezapp.mainThreadEnd()

    s = stats[0] if stats else None
    print(f"stats: {stats}", flush=True)
    ok = (os.path.exists(OUT)
          and len(stats) == 1
          and math.isfinite(s.min) and math.isfinite(s.max) and s.min <= s.max
          and (s.max - s.min) > 0.05)     # non-trivial terraced field (heights = raw meters)
    print(f"=== terrain pybind {'PASSED' if ok else 'FAILED'} ===", flush=True)
    print(f"    {OUT} exists={os.path.exists(OUT)} size={os.path.getsize(OUT) if os.path.exists(OUT) else 0}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
