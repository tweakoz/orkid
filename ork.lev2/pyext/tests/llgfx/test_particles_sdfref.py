#!/usr/bin/env python3
###############################################################################
# SDF asset-reference gate — cross-asset particle systems are now FULLY model B.
# col_vdb's collision_sdf (a LIVE openvdb FloatGrid, non-serializable) rides the
# scene as an ASSET NAME on the collider module (sdf_asset, stamped automatically
# at trace time) and re-resolves from the artifact registry at load. Asserts:
#   1. authoring embeds the graph (cross-asset systems no longer fall back to
#      model A) and the serialized graph carries the sdf_asset name,
#   2. graph round-trip is byte-identical,
#   3. build() from the DESERIALIZED gendata with DSL resolution POISONED
#      succeeds (zero Python re-run) AND the collider's live grid is re-resolved
#      from the registry (resolve count >= 1).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Object, vec3, vec4


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    from ork.hypergraph.ecs.scene.assets import ParticleSystem, ThickSaddleSdf

    # the cross-asset SDF (live FloatGrid artifact — exactly ren_saddle's shape)
    sdfwrap = ThickSaddleSdf(half_extent_x=2.0, half_extent_z=2.0,
                             saddle_coef=0.25, thickness=0.5, voxel_size=0.2)
    sdfwrap.gendata.asset_name = "gate_sdf"
    grid = sdfwrap.build()
    assert grid is not None

    # 1. authoring: cross-asset system EMBEDS now; the collider carries the name
    wrap = ParticleSystem(dsl_file="col_vdb", collision_sdf=sdfwrap,
                          color=vec4(1, 1, 1, 1))
    gd = wrap.gendata
    gd.asset_name = "gate_ptc"
    assert gd.graph is not None, "cross-asset system did not embed (model A relapse)"
    js = gd.serializeJson()
    assert "gate_sdf" in js, "collider sdf_asset name missing from the serialized graph"
    assert "collision_sdf" in js, "asset_kwargs provenance missing"
    # the renderer's material RECIPE must ride too — the live PBRMaterial is runtime-only
    # and dropping it silently rendered the blobs INVISIBLE after a round-trip.
    assert "material_gen" in js and "PbrMaterialGenData" in js, \
        "renderer material recipe missing from the serialized graph"
    print("SDFREF embed+stamp PASS (sdf ref + material recipe)", flush=True)

    # 2. round-trip byte-identical
    gd2 = Object.deserializeJson(js)
    assert gd2.graph is not None
    assert gd2.graph.serializeJson() == gd.graph.serializeJson(), "graph round-trip diverged"
    print("SDFREF roundtrip PASS (byte-identical)", flush=True)

    # 3. THE WIN: build from the clone with DSL resolution POISONED; grid re-resolves
    import ork.hypergraph.dflow.particles.resolve as _resolve
    real = _resolve.resolve_dsl_file
    def _poisoned(arg):
      raise AssertionError("model A regression: build() re-ran the Python DSL (%r)" % arg)
    _resolve.resolve_dsl_file = _poisoned
    try:
      ps2 = ParticleSystem.from_gendata(gd2, artifacts={"gate_sdf": grid})
      dd  = ps2.build()
    finally:
      _resolve.resolve_dsl_file = real
    assert dd is not None and dd.graphdata is not None
    n = lev2.particles.resolve_sdf_assets(dd.graphdata, "gate_sdf", grid)
    assert n >= 1, "no collider module carries the sdf_asset reference after round-trip"
    print("SDFREF no-python-at-load PASS (grid re-resolved on %d module(s))" % n, flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== particles sdfref gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
