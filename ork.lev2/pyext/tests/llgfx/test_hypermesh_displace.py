#!/usr/bin/env python3
###############################################################################
# E.1 gate — DisplaceByField, the first CROSS-FAMILY graph edge: terrain field
# modules (T.*) and hypermesh modules composing in ONE dflow graph.
#   1. CONST oracle      : flat grid + T.const(0.7) field, mode="y", amount=2
#                          -> every vertex y == 1.4 EXACTLY (and x/z untouched).
#   2. GRADIENT oracle   : T.gradient(dir_x=1) field -> closed-form mapping check
#                          y(P) = (P.x/extent + 0.5) * (dim-1)/dim * amount.
#                          dim enters the formula, so this ALSO proves the
#                          module-carried field_dim took effect (64 != the 512 default).
#   3. FBM smoke         : fbm-displaced flat grid -> y in (0, amp*amount], varying.
#   4. ROUND-TRIP        : the MIXED graph serializes; clone re-serialization is
#                          byte-identical; the clone BAKES byte-identical geometry;
#                          and the clone LIVE-materializes (the ECS hosting path).
#   5. ANIMATED FIELD    : (E.1b) offset_vel pans the fbm via the env clock — on a FLAT
#                          grid only field animation can move geometry: it MOVES live,
#                          FREEZES on pause, resumes; is_animated auto-detected; and the
#                          BAKE of the same animated graph stays the t=0 snapshot
#                          (two bakes byte-identical, velocity ignored).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3, VarMap, Object

from ork.hypergraph.dflow.hypermesh import (Hypermesh, materialize_graph, materialize_live_graph,
                                            GpuMeshRenderSource)
from ork.hypergraph.dflow import terrain as T


class ScrollField(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.ripple(grid=GRID_A, amp=0.0, freq=1.0, extent=EXTENT)
    fld = T.fbm(frequency=4.0, offset_vel=(0.8, 0.3))    # env-clock pan -> animated field
    self.displace(n, field=fld, amount=1.5, extent=EXTENT, mode="y", field_dim=FLD_DIM)


def render_frames(scene, ctx, camlut, n):
  for _ in range(n):
    scene.updateScene(camlut)
    ctx.beginFrame()
    scene.renderOnContext(ctx)
    ctx.endFrame()
    time.sleep(0.005)

GRID    = 32
GRID_A  = 24   # the animated-field case's (smaller) grid
EXTENT  = 8.0
FLD_DIM = 64   # deliberately != the 512 default — the gradient oracle detects it


def obj_positions(path):
  out = []
  for line in open(path):
    if line.startswith("v "):
      _, x, y, z = line.split()[:4]
      out.append((float(x), float(y), float(z)))
  return out


def dump(mesh, ctx, tmpdir, name):
  p = os.path.join(tmpdir, name)
  lev2.hypermesh.dump_obj(mesh, ctx, p)
  return p


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  tmpdir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
      os.path.dirname(os.path.abspath(__file__)))))), ".tmp")
  os.makedirs(tmpdir, exist_ok=True)
  ok = False
  try:
    nverts = (GRID + 1) * (GRID + 1)

    ############################################################
    # 1. CONST oracle — exact, mode="y"
    ############################################################
    m   = Hypermesh()
    src = m.ripple(grid=GRID, amp=0.0, freq=1.0, extent=EXTENT)   # amp=0 -> flat grid at y=0
    fld = T.const(0.7)                                            # terrain module INTO the mesh graph
    m.displace(src, field=fld, amount=2.0, extent=EXTENT, mode="y", field_dim=FLD_DIM)
    mesh = m.materialize(ctx)
    assert mesh is not None and mesh.num_verts == nverts, "const-case bake failed"
    P = obj_positions(dump(mesh, ctx, tmpdir, "e1_const.obj"))
    assert len(P) == nverts
    for (x, y, z) in P:
      assert abs(y - 1.4) < 1e-4, "const displace: y=%g want 1.4" % y
      assert abs(x) <= EXTENT / 2 + 1e-4 and abs(z) <= EXTENT / 2 + 1e-4
    print("E1 const oracle PASS (all %d verts y==1.4)" % nverts, flush=True)

    ############################################################
    # 2. GRADIENT oracle — closed-form mapping (+ field_dim proof)
    ############################################################
    m   = Hypermesh()
    src = m.ripple(grid=GRID, amp=0.0, freq=1.0, extent=EXTENT)
    fld = T.gradient(dir_x=1.0, dir_y=0.0)                        # field value at texel x = x/dim
    m.displace(src, field=fld, amount=1.0, extent=EXTENT, mode="y", field_dim=FLD_DIM)
    mesh = m.materialize(ctx)
    P = obj_positions(dump(mesh, ctx, tmpdir, "e1_grad.obj"))
    assert len(P) == nverts
    worst = 0.0
    for (x, y, z) in P:
      u = min(1.0, max(0.0, x / EXTENT + 0.5))
      expect = u * (FLD_DIM - 1) / FLD_DIM        # bilinear of x/dim at uv -> u*(dim-1)/dim
      worst = max(worst, abs(y - expect))
    assert worst < 1e-4, "gradient mapping off by %g (field_dim not honored?)" % worst
    print("E1 gradient oracle PASS (worst err %.2e, dim=%d honored)" % (worst, FLD_DIM), flush=True)

    ############################################################
    # 3. FBM smoke — bounded + actually varying
    ############################################################
    m   = Hypermesh()
    src = m.ripple(grid=GRID, amp=0.0, freq=1.0, extent=EXTENT)
    fld = T.fbm(frequency=4.0)                                    # raw fbm in [0, 1]
    m.displace(src, field=fld, amount=1.5, extent=EXTENT, mode="normal", field_dim=FLD_DIM)
    graph = m.generatedflow()                                     # keep for the round-trip below
    mesh  = materialize_graph(graph, ctx)
    P  = obj_positions(dump(mesh, ctx, tmpdir, "e1_fbm_a.obj"))
    ys = [y for (_, y, _) in P]
    assert min(ys) >= -1e-4 and max(ys) <= 1.5 + 1e-4, "fbm displacement out of [0, amp*amount]"
    assert (max(ys) - min(ys)) > 0.05, "fbm field is flat — generator not running in the mesh graph?"
    print("E1 fbm smoke PASS (y range %.4f..%.4f)" % (min(ys), max(ys)), flush=True)

    ############################################################
    # 4. ROUND-TRIP — the mixed-family graph is a first-class serialized citizen
    ############################################################
    js    = graph.serializeJson()
    clone = Object.deserializeJson(js)
    assert clone is not None, "mixed graph failed to deserialize"
    assert clone.serializeJson() == js, "mixed graph re-serialization diverged (unreflected state)"
    mesh2 = materialize_graph(clone, ctx)
    a = open(dump(mesh,  ctx, tmpdir, "e1_fbm_b.obj")).read()
    b = open(dump(mesh2, ctx, tmpdir, "e1_fbm_clone.obj")).read()
    assert a == b, "clone baked DIFFERENT geometry than the original"
    live = materialize_live_graph(clone, ctx)                     # the ECS hosting path
    assert live is not None and live.mesh is not None and live.mesh.num_verts == nverts
    print("E1 roundtrip PASS (byte-identical serdes + bake; clone live-materializes)", flush=True)

    ############################################################
    # 5. ANIMATED FIELD (E.1b) — env-clock pan moves geometry live; pause holds it
    ############################################################
    from ork.hypergraph.ecs.scene.assets import Hypermesh as HmAsset, Ptex3d
    from ork.hypergraph.assets.materials.terrain.solid import Solid

    hwrap = HmAsset(dsl_class=ScrollField)
    hwrap.gendata.asset_name = "scroll_mesh"
    assert hwrap._animated, "offset_vel field must auto-report is_animated"
    mwrap = Ptex3d(dsl_class=Solid, vertex_source=GpuMeshRenderSource(), roughness=0.5)
    mwrap.gendata.asset_name = "scroll_mat"
    hmdd = hwrap.drawable_data(material=mwrap)
    asys = ecs.AssetSystemData()
    asys.declareAssetGen(mwrap.gendata)
    asys.declareAssetGen(hwrap.gendata)
    artifacts = asys.materializeAll(ctx)
    hmdd.resolved_material = artifacts["scroll_mat"]

    params = VarMap(); params.preset = "ForwardPBR"
    scene  = lev2.scenegraph.Scene(params)
    layer  = scene.createLayer("std_forward")
    layer.createDrawableNodeFromData("scroll", hmdd)
    cam = lev2.CameraData(); cam.perspective(0.1, 100.0, 45.0)
    cam.lookAt(vec3(8, 6, 8), vec3(0, 0, 0), vec3(0, 1, 0))
    camlut = lev2.CameraDataLut(); camlut.addCamera("spawncam", cam)

    render_frames(scene, ctx, camlut, 3)
    alive = hmdd.live
    assert alive is not None, "lazy bootstrap never fired"
    assert alive.clock_abstime > 0.0, "live clock did not advance"
    a1 = open(dump(alive.mesh, ctx, tmpdir, "e1_anim_a.obj")).read()
    render_frames(scene, ctx, camlut, 2)
    a2 = open(dump(alive.mesh, ctx, tmpdir, "e1_anim_b.obj")).read()
    assert a1 != a2, "animated FIELD froze: offset_vel pan not reaching the GPU"
    alive.paused = True
    render_frames(scene, ctx, camlut, 2)
    p1 = open(dump(alive.mesh, ctx, tmpdir, "e1_anim_p1.obj")).read()
    render_frames(scene, ctx, camlut, 2)
    p2 = open(dump(alive.mesh, ctx, tmpdir, "e1_anim_p2.obj")).read()
    assert p1 == p2, "paused field still moving (clock contract broken)"
    alive.paused = False
    print("E1 animated-field PASS (field scrolls live; pause holds; resume works)", flush=True)

    # the BAKE of the SAME animated graph is the t=0 snapshot: two bakes byte-identical.
    b1 = open(dump(materialize_graph(hwrap.gendata.graph, ctx), ctx, tmpdir, "e1_anim_t0a.obj")).read()
    b2 = open(dump(materialize_graph(hwrap.gendata.graph, ctx), ctx, tmpdir, "e1_anim_t0b.obj")).read()
    assert b1 == b2, "bake of an animated graph must be the deterministic t=0 snapshot"
    print("E1 bake-determinism PASS (animated graph bakes t=0, velocity ignored)", flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== hypermesh displace (E.1) gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
