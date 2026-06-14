#!/usr/bin/env python3
###############################################################################
# E.2-B gate — the TYPED INSTANCE EDGE (InstanceSet plug + ScatterSource + sink):
#   1. bake a small HF + scatter (the C++ placer writes the .ogeo),
#   2. a hypermesh graph carries `instance_source(ogeo_path=..., type_id=...)` —
#      materialize_live discovers the InstanceSet (live.instance_count == the
#      type's placed count),
#   3. make_drawable AUTO-instances from the graph's set; the scenegraph renders
#      frames (lazy path exercised; the indirect draw carries the set's count),
#   4. the MIXED graph (mesh modules + ScatterSource) round-trips byte-identically
#      and the clone re-materializes with the same instance count,
#   5. install_hypermeshes (now the one-wire convenience loop) still works.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time
import numpy as np
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3, VarMap, Object

DSL = '''
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

class EdgeHF(HeightField):
  EXTENT_M = 128.0
  HEIGHT_M = 20.0
  def __init__(self):
    super().__init__()
    h = T.fbm(frequency=3.0, octaves=4) * 0.5 + 0.5
    self.capture(h, "height")
    alt = T.normalize(h)
    self.scatter("rocks",
        density = 0.05, seed = 5, align = "normal",
        scale  = (0.6, 1.2), cutoff = 0.05, jitter = 0.9,
        types  = {"small": 1.0 - alt, "big": alt})
'''


def render_frames(scene, ctx, camlut, n):
  for _ in range(n):
    scene.updateScene(camlut)
    ctx.beginFrame()
    scene.renderOnContext(ctx)
    ctx.endFrame()
    time.sleep(0.005)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    from ork.hypergraph.ecs.scene.assets import HeightField
    from ork.hypergraph.dflow.hypermesh import (Hypermesh, make_drawable,
                                                materialize_live_graph)
    from ork.hypergraph.dflow.terrain.scatter_consumer import install_hypermeshes
    from ork.hypergraph.assets.materials.terrain.solid import Solid
    from orkengine.lev2 import Geometry

    tmpdir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))), ".tmp")
    os.makedirs(tmpdir, exist_ok=True)
    dsl_path = os.path.join(tmpdir, "edge_hf.py")
    open(dsl_path, "w").write(DSL)

    # 1. bake + place (the C++ placer)
    hf = HeightField(dsl_file=dsl_path, dimension=128, extent_m=128.0, height_scale_m=20.0, ctx=ctx)
    hf.gendata.asset_name = "edge_hf"
    art = hf.build()
    sc = art["scatters"]["rocks"]
    tid = np.array(Geometry.read(sc["path"]).point["type_id"])
    n_small = int((tid == 0).sum())
    n_big   = int((tid == 1).sum())
    assert n_small > 0 and n_big > 0, "scatter produced an empty type (n=%d/%d)" % (n_small, n_big)
    print("EDGE bake PASS (%d small + %d big placed)" % (n_small, n_big), flush=True)

    # 2. the typed edge: a mesh graph carrying its own instance source
    m = Hypermesh()
    src = m.box(size=0.4)
    m.output(m.face_normals(src))
    m.instance_source(ogeo_path=sc["path"], type_id=1)        # the "big" type
    live = m.materialize_live(ctx)
    assert live.instance_count == n_big, \
        "InstanceSet count %d != placed big count %d" % (live.instance_count, n_big)
    print("EDGE instance_source PASS (live.instance_count=%d)" % live.instance_count, flush=True)

    # 3. render through the scenegraph — make_drawable AUTO-instances from the set
    cdd, _mtl = make_drawable(live, ctx, material_cls=Solid, roughness=0.7)
    params = VarMap(); params.preset = "ForwardPBR"
    scene  = lev2.scenegraph.Scene(params)
    layer  = scene.createLayer("std_forward")
    layer.createDrawableNodeFromData("rocks", cdd)
    cam = lev2.CameraData(); cam.perspective(0.1, 500.0, 45.0)
    cam.lookAt(vec3(80, 60, 80), vec3(0, 0, 0), vec3(0, 1, 0))
    camlut = lev2.CameraDataLut(); camlut.addCamera("spawncam", cam)
    render_frames(scene, ctx, camlut, 3)
    print("EDGE render PASS (3 frames, auto-instanced drawable)", flush=True)

    # 4. round-trip: the mixed graph (mesh + ScatterSource) is a serialized citizen
    g  = m.generatedflow()
    js = g.serializeJson()
    clone = Object.deserializeJson(js)
    assert clone.serializeJson() == js, "instance-edge graph re-serialization diverged"
    live2 = materialize_live_graph(clone, ctx)
    assert live2.instance_count == n_big, "clone lost the InstanceSet (count %d)" % live2.instance_count
    print("EDGE roundtrip PASS (byte-identical; clone re-instances %d)" % n_big, flush=True)

    # 5. install_hypermeshes — the retired glue's signature still works over the edge
    class Rock(Hypermesh):
      def __init__(self):
        super().__init__()
        self.output(self.face_normals(self.icosphere(radius=0.3, subdivisions=1)))
    nodes, lives, assets = install_hypermeshes(
        sc["path"], [(Rock(), Solid), (Rock(), Solid)], layer, ctx, name="rk")
    assert len(nodes) == 2 and len(lives) == 2
    assert lives[0].instance_count == n_small and lives[1].instance_count == n_big
    render_frames(scene, ctx, camlut, 2)
    print("EDGE install_hypermeshes PASS (2 typed nodes, %d + %d instances)"
          % (n_small, n_big), flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== hypermesh instance-edge (E.2) gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
