#!/usr/bin/env python3
###############################################################################
# M5 step 1 — terrain scene asset, the python-decoupled core. EMBED a DSL-
# produced terrain graph in HeightFieldGenData (model B), serialize the gendata
# to JSON, deserialize it WITH NO DSL RE-RUN, and bake the deserialized graph.
# This proves the .ecs would load + materialize terrain with no Python DSL file.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Object   # serializeJson / deserializeJson

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

DIM = 512
OUT = "/tmp/terrain_asset_height.exr"


class RollingHills(HeightField):
    def __init__(self, octaves=5, steps=6):
        super().__init__()
        h = T.Fbm(frequency=3.0, octaves=octaves) * 0.5 + 0.5
        self.capture(T.Terrace(h, steps=steps, sharpness=4.0), "height")


def main():
    if os.path.exists(OUT):
        os.remove(OUT)
    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    # AUTHORING: run the DSL ONCE -> graph -> embed in the gendata.
    hf    = RollingHills(octaves=6, steps=6)
    graph = hf.generatedflow()
    gd    = lev2.HeightFieldGenData(asset_name="rolling_hills", dimension=DIM, graph=graph)

    js = gd.serializeJson()
    print(f"gendata JSON bytes={len(js)} (embeds the whole terrain graph)", flush=True)

    # DESERIALIZE — no DSL, no Python terrain family needed to rebuild the graph.
    gd2    = Object.deserializeJson(js)
    graph2 = gd2.graph
    dim2   = gd2.dimension
    assert graph2 is not None, "deserialized gendata carries no graph"
    caps = lev2.terrain.capture_modules(graph2)
    chans = [c.channel for c in caps]
    print(f"deserialized: dimension={dim2} cacheable={graph2.cacheable} channels={chans}", flush=True)

    # MATERIALIZE: derive each channel's path from its (serialized) channel name,
    # then bake the DESERIALIZED graph (cook-cache-backed).
    for cap in caps:
        cap.path = OUT if cap.channel == "height" else f"/tmp/terrain_asset_{cap.channel}.exr"
    stats = lev2.terrain.bake_heightfield(graph2, ctx, dim2)

    ezapp.mainThreadEnd()

    ok = (os.path.exists(OUT)
          and dim2 == DIM
          and chans == ["height"]
          and len(stats) == 1
          and 0.0 <= stats[0].min <= stats[0].max <= 1.0001)
    print(f"=== terrain asset round-trip {'PASSED' if ok else 'FAILED'} ===", flush=True)
    print(f"    {OUT} exists={os.path.exists(OUT)} stats={stats}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
