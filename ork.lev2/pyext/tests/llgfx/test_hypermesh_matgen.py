#!/usr/bin/env python3
###############################################################################
# D.1 gate — PbrMaterialGenData: JSON round-trip + the C++ MATERIALIZER (Goal-C shape:
# the deserialized gendata materializes to a live PBRMaterial with ZERO Python build
# logic — gendata.materialize(ctx) is pure C++). Asserts:
#   1. serialize(gendata) -> deserialize -> field equality (factors, lobes, shaderpath),
#   2. materialize(ctx) on the DESERIALIZED object yields a live material whose
#      observable state mirrors the gendata (incl. the generated-fxv2 shaderpath
#      resolving through the <staging> dslshadercache token and a resolvable bindable param),
#   3. C++ vs Python reference (ORK_HM_PYMAT path) produce equal observable state.
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
    # a ptex3d-generated material (generated fxv2s persist in the per-family staging cache —
    # ptex3d-generated material: factors stay identity, the SURFACE values ride shader_params and
    # owner policy 2026-06-12: <staging>/dslshadercache/<family>, token reference; B.5c retired)
    from ork.hypergraph.ecs.scene.assets import Ptex3d
    from ork.hypergraph.assets.materials.terrain.solid import Solid
    wrap = Ptex3d(dsl_class=Solid, roughness=0.41, metallic=0.12)
    wrap._ctx = ctx
    gd = wrap.gendata
    gd.asset_name = "d1_matgen"
    gd.has_clearcoat = True
    gd.clearcoat_factor = 0.66

    js  = gd.serializeJson()
    gd2 = Object.deserializeJson(js)                  # the pure-deserialized gendata (no DSL)
    assert gd2.asset_name == "d1_matgen"
    assert gd2.has_clearcoat and abs(gd2.clearcoat_factor - 0.66) < 1e-6
    assert gd2.shaderpath.startswith("<staging>/dslshadercache/"), gd2.shaderpath
    print("MATGEN roundtrip PASS (shaderpath=%s)" % gd2.shaderpath, flush=True)

    mat = gd2.materialize(ctx)                        # D.1: pure C++ build off the deserialized data
    assert mat is not None
    assert mat.has_clearcoat and abs(mat.clearcoat_factor - 0.66) < 1e-6
    assert mat.shaderpath == gd2.shaderpath
    assert mat.param("roughness_factor") is not None or mat.param("base_color") is not None, \
        "generated-fxv2 bindable params did not resolve (shaderpath load failed?)"
    print("MATGEN c++ materialize PASS (ptex3d shaderpath)", flush=True)

    # plain PBR gendata: factors + lobes round-trip and materialize
    from ork.hypergraph.ecs.scene.assets import PbrMaterialGenData
    gp = PbrMaterialGenData(base_color=vec4(0.6, 0.3, 0.2, 1), metallic=0.12, roughness=0.41,
                            has_sheen=True, sheen_factor=0.5, sheen_color=vec3(0.9, 0.8, 0.7))
    gp2 = Object.deserializeJson(gp.serializeJson())
    m2  = gp2.materialize(ctx)
    assert abs(m2.roughnessFactor - 0.41) < 1e-6 and abs(m2.metallicFactor - 0.12) < 1e-6
    assert m2.has_sheen and abs(m2.sheen_factor - 0.5) < 1e-6
    bc = m2.baseColor
    assert abs(bc.x - 0.6) < 1e-6 and abs(bc.y - 0.3) < 1e-6
    print("MATGEN c++ materialize PASS (plain pbr factors+lobes)", flush=True)

    # parity vs the retained Python reference (same gendata, both builders)
    os.environ["ORK_HM_PYMAT"] = "1"
    from ork.hypergraph.ecs.scene.assets import PbrMaterial
    ref = PbrMaterial.from_gendata(gd2, ctx=ctx).build()
    os.environ.pop("ORK_HM_PYMAT", None)
    for prop in ("roughnessFactor", "metallicFactor", "has_clearcoat", "clearcoat_factor",
                 "shaderpath", "has_transmission", "ior"):
      a, b = getattr(mat, prop), getattr(ref, prop)
      assert a == b or (isinstance(a, float) and abs(a - b) < 1e-6), \
          "parity: %s differs (%r vs %r)" % (prop, a, b)
    print("MATGEN parity PASS (c++ == python reference)", flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    os.environ.pop("ORK_HM_PYMAT", None)
    ezapp.mainThreadEnd()
    print("=== hypermesh matgen gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
