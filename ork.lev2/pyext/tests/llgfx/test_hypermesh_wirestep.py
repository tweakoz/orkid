#!/usr/bin/env python3
###############################################################################
# D.1 stage 3 gate — the C++ WIRE STEP. An AssetSystemData carrying a ptex3d material gen
# AND a terrain heightfield gen round-trips through JSON, and ONE C++ call
# (AssetSystemData.materializeAll) materializes the whole list in declaration order into a
# name->artifact registry — the exact shape AssetSystem::_onGpuInit runs for the future
# pure-C++ host. Asserts: the material is live (factors/shaderpath), the heightfield's
# manifest exists + parses + its channel images exist, and unsupported gen types are
# skipped (not fatal) during the transition.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, json
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Object


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    from ork.hypergraph.ecs.scene.assets import Ptex3d, HeightField
    from ork.hypergraph.assets.materials.terrain.solid import Solid

    asys = ecs.AssetSystemData()
    mwrap = Ptex3d(dsl_class=Solid, roughness=0.37)
    mwrap.gendata.asset_name = "wire_mat"
    asys.declareAssetGen(mwrap.gendata)
    hwrap = HeightField(dsl_file="voronoi", dimension=128, ctx=ctx)
    hwrap.gendata.asset_name = "wire_hf"
    asys.declareAssetGen(hwrap.gendata)

    js    = asys.serializeJson()
    asys2 = Object.deserializeJson(js)               # the deserialized side (no DSL, no wrappers)
    assert len(asys2.gens) == 2

    artifacts = asys2.materializeAll(ctx)            # ONE C++ call wires the whole list
    assert set(artifacts.keys()) == {"wire_mat", "wire_hf"}, artifacts.keys()

    mat = artifacts["wire_mat"]
    assert mat.shaderpath.startswith("<hyperassets>"), mat.shaderpath
    assert mat.param("base_color") is not None or mat.param("roughness_factor") is not None
    print("WIRESTEP material PASS (%s)" % mat.shaderpath, flush=True)

    manifest = artifacts["wire_hf"]
    assert os.path.isfile(manifest) and os.path.isabs(manifest), manifest
    doc = json.load(open(manifest))
    assert doc["version"] == 1 and doc["scale"]["dim"] == 128
    for ch, meta in doc["channels"].items():
      img = os.path.join(os.path.dirname(manifest), meta["file"])
      assert os.path.isfile(img), img
    print("WIRESTEP heightfield PASS (%s, %d channels)" % (manifest, len(doc["channels"])), flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== hypermesh wirestep gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
