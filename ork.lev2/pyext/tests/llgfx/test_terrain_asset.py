#!/usr/bin/env python3
###############################################################################
# M5 step 1 — terrain scene asset, the python-decoupled core. EMBED a DSL-
# produced terrain graph in HeightFieldGenData (model B), serialize the gendata
# to JSON, deserialize it WITH NO DSL RE-RUN, and bake the deserialized graph.
# This proves the .ecs would load + materialize terrain with no Python DSL file.
# 2.20 STRENGTHENED to BAKE-EQUALITY: the AUTHORED graph and the DESERIALIZED
# graph are both baked and their height EXRs must be BYTE-IDENTICAL (t=0
# determinism oracle, B.4) — not merely "a file exists with sane stats".
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
    gd    = lev2.HeightFieldGenData(asset_name="rolling_hills", dimension=DIM, graph=graph,
                                    # E.6/2.20 — the terrain↔material contract (serdes coverage)
                                    material="terra_mat",
                                    channel_samplers={"height": "HeightTex"})

    # bake the AUTHORED graph (the equality reference for the round trip)
    REF = "/tmp/terrain_asset_height_ref.exr"
    for cap in lev2.terrain.capture_modules(graph):
        cap.path = REF if cap.channel == "height" else f"/tmp/terrain_asset_ref_{cap.channel}.exr"
    lev2.terrain.bake_heightfield(graph, ctx, DIM)

    js = gd.serializeJson()
    print(f"gendata JSON bytes={len(js)} (embeds the whole terrain graph)", flush=True)

    # DESERIALIZE — no DSL, no Python terrain family needed to rebuild the graph.
    gd2    = Object.deserializeJson(js)
    graph2 = gd2.graph
    dim2   = gd2.dimension
    assert graph2 is not None, "deserialized gendata carries no graph"
    # E.6/2.20 — the terrain↔material contract survives the round trip
    assert gd2.material_asset == "terra_mat", \
        "material_asset lost on round-trip: %r" % gd2.material_asset
    assert gd2.channel_samplers == {"height": "HeightTex"}, \
        "channel_samplers lost on round-trip: %r" % gd2.channel_samplers
    assert gd2.serializeJson() == js, "gendata re-serialization diverged"
    caps = lev2.terrain.capture_modules(graph2)
    chans = [c.channel for c in caps]
    print(f"deserialized: dimension={dim2} cacheable={graph2.cacheable} channels={chans}", flush=True)

    # MATERIALIZE: derive each channel's path from its (serialized) channel name,
    # then bake the DESERIALIZED graph (cook-cache-backed).
    for cap in caps:
        cap.path = OUT if cap.channel == "height" else f"/tmp/terrain_asset_{cap.channel}.exr"
    stats = lev2.terrain.bake_heightfield(graph2, ctx, dim2)

    ezapp.mainThreadEnd()

    # BAKE-EQUALITY (2.20): deserialize(serialize(graph)) must bake the IDENTICAL field.
    with open(REF, "rb") as f:
        ref_bytes = f.read()
    with open(OUT, "rb") as f:
        out_bytes = f.read()
    bake_equal = (ref_bytes == out_bytes) and len(ref_bytes) > 0

    ok = (os.path.exists(OUT)
          and dim2 == DIM
          and chans == ["height"]
          and len(stats) == 1
          and 0.0 <= stats[0].min <= stats[0].max <= 1.0001
          and bake_equal)
    print(f"=== terrain asset round-trip {'PASSED' if ok else 'FAILED'} ===", flush=True)
    print(f"    {OUT} exists={os.path.exists(OUT)} stats={stats}", flush=True)
    print(f"    BAKE-EQUALITY: {len(ref_bytes)} bytes, identical={bake_equal}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
