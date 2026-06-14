#!/usr/bin/env python3
###############################################################################
# D.4 gate — the remaining binding serialization (review 1.10 + 1.11). Asserts:
#   1. the SCATTER CONTRACT round-trips: scatter() declarations (placement params +
#      the NEW type→asset/material bindings) reflect onto HeightFieldGenData.scatters,
#      survive JSON, and a build FROM THE DESERIALIZED gendata re-places the ScatterSet
#      (.ogeo with points) with NO Python DSL state — before D.4 a round-trip lost ALL
#      scatter,
#   2. the SAMPLER→TEXTURE map round-trips on PbrMaterialGenData and the C++
#      materializer loads + binds it (a ptex3d ctx.tex material samples a baked terrain
#      channel by its deterministic <assetcache> path).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Object

_HF_DSL = '''
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

class ScatterHF(HeightField):
  def __init__(self):
    super().__init__()
    h = T.fbm(frequency=3.0, octaves=4) * 0.5 + 0.5
    self.capture(h, "height")
    alt = T.normalize(h)
    self.scatter("props",
        count   = 300,
        seed    = 7,
        align   = "up",
        yaw     = (0.0, 1.0),
        scale   = (0.8, 1.2),
        cutoff  = 0.05,
        jitter  = 0.9,
        lift    = 0.5,
        types   = {"low": 1.0 - alt, "high": alt},
        assets    = {"low": "mesh_low", "high": "mesh_high"},
        materials = {"low": "mat_low"})
'''


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    from ork.hypergraph.ecs.scene.assets import HeightField, Ptex3d

    tmpdir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))), ".tmp")
    os.makedirs(tmpdir, exist_ok=True)
    dsl_path = os.path.join(tmpdir, "d4_scatter_hf.py")
    open(dsl_path, "w").write(_HF_DSL)

    # 1a. authoring reflects the scatter contract onto the gendata
    hwrap = HeightField(dsl_file=dsl_path, dimension=128, ctx=ctx)
    hwrap.gendata.asset_name = "d4_hf"
    sinks = hwrap.gendata.scatters
    assert len(sinks) == 1, "scatter sink did not reflect onto the gendata"
    s = sinks[0]
    assert s.name == "props" and s.count == 300 and s.density == 0.0
    assert s.seed == 7 and s.align == "up" and abs(s.lift - 0.5) < 1e-6
    assert abs(s.yaw_hi - 1.0) < 1e-6 and abs(s.scale_lo - 0.8) < 1e-6
    assert s.type_names == ["low", "high"], s.type_names      # type_id order preserved
    assert len(s.type_channels) == 2 and all(c for c in s.type_channels)
    assert s.type_assets == {"low": "mesh_low", "high": "mesh_high"}
    assert s.type_materials == {"low": "mat_low"}
    print("D4 scatter reflect PASS", flush=True)

    # 1b. JSON round-trip preserves the whole contract
    js  = hwrap.gendata.serializeJson()
    gd2 = Object.deserializeJson(js)
    s2  = gd2.scatters[0]
    assert s2.name == "props" and s2.count == 300 and s2.seed == 7
    assert s2.type_names == ["low", "high"] and s2.type_channels == s.type_channels
    assert s2.type_assets == {"low": "mesh_low", "high": "mesh_high"}
    assert s2.type_materials == {"low": "mat_low"}
    assert abs(s2.jitter - 0.9) < 1e-6 and abs(s2.cutoff - 0.05) < 1e-6
    print("D4 scatter roundtrip PASS", flush=True)

    # 1c. THE WIN: build from the DESERIALIZED gendata re-places the scatter
    result = HeightField.from_gendata(gd2, ctx=ctx).build()
    sc = result.get("scatters", {}).get("props")
    assert sc is not None, "deserialized build produced NO scatter (the pre-D.4 regression)"
    assert sc["count"] > 0 and os.path.isfile(sc["path"]), sc
    assert sc["types"] == ["low", "high"]
    assert sc["assets"] == {"low": "mesh_low", "high": "mesh_high"}
    assert sc["materials"] == {"low": "mat_low"}
    print("D4 scatter placement-from-clone PASS (%d points -> %s)" % (sc["count"], sc["path"]), flush=True)

    # 2. sampler→texture: a ptex3d material samples the baked height channel
    from ork.hypergraph.ptex3d import Ptex3d as Ptex3dBase, P

    class TexMat(Ptex3dBase):
      def __init__(self, ctx_):
        h = ctx_.tex("TestMap").x
        self.surface(albedo=P.vec3(h, h, h), metallic=0.0, roughness=0.8)

    height_path = result["height"]
    assert os.path.isfile(height_path)
    mwrap = Ptex3d(dsl_class=TexMat, sampler_textures={"TestMap": height_path})
    mwrap.gendata.asset_name = "d4_mat"
    assert mwrap.gendata.sampler_textures == {"TestMap": height_path}
    mjs = mwrap.gendata.serializeJson()
    assert "sampler_textures" in mjs and "TestMap" in mjs
    md2 = Object.deserializeJson(mjs)
    assert md2.sampler_textures == {"TestMap": height_path}, md2.sampler_textures
    print("D4 sampler roundtrip PASS", flush=True)

    mat = md2.materialize(ctx)   # C++ loads the image + binds the sampler
    assert mat is not None
    assert mat.param("TestMap") is not None, "generated shader lost its sampler uniform"
    print("D4 sampler materialize PASS (TestMap bound from %s)" % height_path, flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== hyperecs d4 bindings gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
