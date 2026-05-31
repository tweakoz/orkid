#!/usr/bin/env python3
###############################################################################
# M5 step 2/3 — the HeightField ASSET WRAPPER (ork.hypergraph.ecs.scene.assets).
#   1. AUTHORING: HeightField(dsl_file="rolling_hills", ...) resolves + runs the
#      terrain DSL ONCE, embeds the serialized graph in HeightFieldGenData,
#      build() bakes -> dict {channel: exr_path, "stats": {...}}.
#   2. GENDATA ROUND-TRIP (no DSL): serialize the gendata -> deserialize ->
#      from_gendata -> build() -> dict. Proves materialize with no Python DSL +
#      that the reloaded bake matches the authored one (deterministic + cached).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Object

from ork.hypergraph.ecs.scene.assets import HeightField


def main():
    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    # 1) AUTHORING — resolve + run the DSL file once -> embedded graph -> bake.
    # "hf1" resolves to ork/hypergraph/assets/terrain/hf1.py (HF1 HeightField).
    hf = HeightField(dsl_file="hf1", dimension=512, octaves=6, steps=6, ctx=ctx)
    hf.gendata.asset_name = "hf1"
    art1 = hf.build()
    print(f"authored:  {[(k, art1[k]) for k in art1 if k != 'stats']}  stats={art1['stats']}", flush=True)

    # 2) GENDATA ROUND-TRIP — serialize the gendata (embeds the graph), deserialize
    #    with NO DSL, rehydrate the wrapper, materialize again.
    js  = hf.gendata.serializeJson()
    gd2 = Object.deserializeJson(js)
    hf2 = HeightField.from_gendata(gd2, ctx=ctx)
    art2 = hf2.build()
    print(f"reloaded:  {[(k, art2[k]) for k in art2 if k != 'stats']}  stats={art2['stats']}  (no DSL)", flush=True)

    ezapp.mainThreadEnd()

    ok = ("height" in art1 and "height" in art2
          and os.path.exists(art2["height"])
          and abs(art1["stats"]["height"].mean - art2["stats"]["height"].mean) < 1e-6)
    print(f"=== terrain asset wrapper {'PASSED' if ok else 'FAILED'} ===", flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
