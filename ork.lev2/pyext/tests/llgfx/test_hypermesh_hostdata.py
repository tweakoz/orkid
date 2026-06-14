#!/usr/bin/env python3
###############################################################################
# D.3 gate — the HYPERECS hypermesh host data. Asserts the full model-B path with the
# NEW C++ host classes (no make_drawable, no Python at load):
#   1. HypermeshGenData embeds the graph at authoring; JSON round-trip is byte-identical;
#      the DESERIALIZED gen materializes (C++ materializeLive) to a live mesh,
#   2. HypermeshDrawableData (graph + material-by-name + flags) round-trips through JSON,
#   3. the AssetSystemData wire step materializes BOTH gen types in one C++ call
#      (PBRMaterial + LiveHypermesh artifacts),
#   4. a drawable created FROM THE DESERIALIZED HypermeshDrawableData renders through the
#      scenegraph: the lazy bootstrap fires on first onGpuUpdate (material resolved by
#      name), the live clock advances (animated), geometry MOVES, and the per-graph
#      pause contract holds (the ECS component's PAUSE/RESUME path).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3, VarMap, Object

from ork.hypergraph.dflow.hypermesh import (Hypermesh, S, isolate, group, POLY,
                                            GpuMeshRenderSource)


class AnimAsset(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.box(size=1.0)
    n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
    n = self.extrude_faces(n, distance=0.3 + 0.1 * S.sin(S.time), slot=2)   # S.time -> animated
    self.output(n)


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
    from ork.hypergraph.ecs.scene.assets import Hypermesh as HmAsset, Ptex3d
    from ork.hypergraph.assets.materials.terrain.solid import Solid

    # 1. HypermeshGenData: author once, embed, round-trip, materialize from the clone
    hwrap = HmAsset(dsl_class=AnimAsset)
    hwrap.gendata.asset_name = "host_mesh"
    assert hwrap.gendata.graph is not None, "authoring did not embed the graph"
    assert hwrap._animated, "S.time asset must report animated"
    js   = hwrap.gendata.serializeJson()
    gd2  = Object.deserializeJson(js)
    assert gd2.graph is not None
    assert gd2.graph.serializeJson() == hwrap.gendata.graph.serializeJson(), \
        "embedded hypermesh graph round-trip diverged"
    live0 = gd2.materialize(ctx)
    assert live0 is not None and live0.mesh is not None
    m0 = live0.mesh
    assert m0.num_faces > 0 and m0.num_verts > 0, "deserialized gen materialized an empty mesh"
    print("HOSTDATA gen PASS (faces=%d verts=%d from DESERIALIZED gen)" % (m0.num_faces, m0.num_verts), flush=True)

    # 2. the material asset (ptex3d w/ the SSBO-pull vertex source) + the drawable data
    mwrap = Ptex3d(dsl_class=Solid, vertex_source=GpuMeshRenderSource(), roughness=0.4)
    mwrap.gendata.asset_name = "host_mat"
    hmdd = hwrap.drawable_data(material=mwrap)
    assert hmdd.animated and hmdd.material_asset == "host_mat"
    djs   = hmdd.serializeJson()
    hmdd2 = Object.deserializeJson(djs)
    assert hmdd2.animated and hmdd2.material_asset == "host_mat"
    assert hmdd2.graph is not None
    assert hmdd2.graph.serializeJson() == hmdd.graph.serializeJson(), \
        "drawabledata graph round-trip diverged"
    print("HOSTDATA drawabledata roundtrip PASS", flush=True)

    # 3. the wire step covers BOTH new artifact types in one C++ call
    asys = ecs.AssetSystemData()
    asys.declareAssetGen(mwrap.gendata)
    asys.declareAssetGen(hwrap.gendata)
    asys2 = Object.deserializeJson(asys.serializeJson())
    artifacts = asys2.materializeAll(ctx)
    assert set(artifacts.keys()) == {"host_mat", "host_mesh"}, artifacts.keys()
    assert artifacts["host_mesh"].mesh.num_faces == m0.num_faces
    print("HOSTDATA wirestep PASS (material + livehypermesh artifacts)", flush=True)

    # 4. render from the DESERIALIZED drawable data — material resolved by name
    hmdd2.resolved_material = artifacts["host_mat"]

    params = VarMap()
    params.preset = "ForwardPBR"
    scene = lev2.scenegraph.Scene(params)
    layer = scene.createLayer("std_forward")
    node  = layer.createDrawableNodeFromData("hm_host", hmdd2)

    cam = lev2.CameraData()
    cam.perspective(0.1, 100.0, 45.0)
    cam.lookAt(vec3(3, 2, 3), vec3(0, 0, 0), vec3(0, 1, 0))
    camlut = lev2.CameraDataLut()
    camlut.addCamera("spawncam", cam)

    assert hmdd2.live is None, "live must not exist before the first frame (lazy bootstrap)"
    render_frames(scene, ctx, camlut, 3)
    live = hmdd2.live
    assert live is not None, "lazy bootstrap never fired (onGpuUpdate path dead)"
    t_run = live.clock_abstime
    assert t_run > 0.0, "live clock did not advance: animated graph not recomputing"
    print("HOSTDATA bootstrap PASS clock_abstime=%.6f after 3 frames" % t_run, flush=True)

    tmpdir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))), ".tmp")
    os.makedirs(tmpdir, exist_ok=True)
    o1 = os.path.join(tmpdir, "hostdata_a.obj")
    o2 = os.path.join(tmpdir, "hostdata_b.obj")
    lev2.hypermesh.dump_obj(live.mesh, ctx, o1)
    render_frames(scene, ctx, camlut, 2)
    lev2.hypermesh.dump_obj(live.mesh, ctx, o2)
    assert open(o1).read() != open(o2).read(), "animated geometry froze while UNPAUSED"
    print("HOSTDATA anim PASS (geometry moves)", flush=True)

    # the ECS HypermeshComponent PAUSE/RESUME contract = this per-graph flag
    live.paused = True
    render_frames(scene, ctx, camlut, 2)
    t_paused = live.clock_abstime
    lev2.hypermesh.dump_obj(live.mesh, ctx, o1)
    render_frames(scene, ctx, camlut, 2)
    lev2.hypermesh.dump_obj(live.mesh, ctx, o2)
    assert open(o1).read() == open(o2).read(), "paused geometry still moving"
    assert live.clock_abstime == t_paused, "per-graph pause must hold the live clock"
    live.paused = False
    render_frames(scene, ctx, camlut, 2)
    assert live.clock_abstime > t_paused, "unpause must resume the live clock"
    print("HOSTDATA pause PASS (per-graph pause holds + resumes)", flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== hypermesh hostdata gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
