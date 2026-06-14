#!/usr/bin/env python3
###############################################################################
# C.1a gate — the hypermesh LIVE recompute rides Drawable::onGpuUpdate (per-FRAME,
# view-independent), not onPreRender (per-VIEW). Renders a real scene headlessly through
# renderOnContext (the direct path) and asserts:
#   1. the live clock advanced  -> _liveRecompute actually FIRED on the render path
#      (the bake gates never exercise this hook; before this gate a dead hook = a frozen
#      viewer with every other gate green),
#   2. the global pause stops it -> the B.4 clock contract rides the relocated hook,
#   3. unpausing resumes it seamlessly.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs    # headless_appinit
from orkengine.core import vec3, VarMap

from ork.hypergraph.dflow.hypermesh import (Hypermesh, S, isolate, group, POLY, make_drawable)


class AnimAsset(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.box(size=1.0)
    n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
    n = self.extrude_faces(n, distance=0.3 + 0.1 * S.sin(S.time), slot=2)   # S.time -> animated
    self.output(n)


def render_frames(scene, ctx, camlut, n):
  for _ in range(n):
    scene.updateScene(camlut)        # enqueue the draw queue (main thread == render thread here)
    ctx.beginFrame()
    scene.renderOnContext(ctx)       # the DIRECT path: _renderIMPL -> gpuUpdate -> onGpuUpdate
    ctx.endFrame()                   # advances GetTargetFrame -> next gpuUpdate isn't deduped
    time.sleep(0.005)                # ensure a measurable steady-clock dt between frames


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    asset = AnimAsset()
    assert asset.is_animated, "S.time asset must report is_animated"
    live = asset.materialize_live(ctx)
    cdd, gmtl = make_drawable(live, ctx, animated=True)

    params = VarMap()
    params.preset = "ForwardPBR"
    scene = lev2.scenegraph.Scene(params)
    layer = scene.createLayer("std_forward")
    node  = layer.createDrawableNodeFromData("hm_live", cdd)

    cam = lev2.CameraData()
    cam.perspective(0.1, 100.0, 45.0)
    cam.lookAt(vec3(3, 2, 3), vec3(0, 0, 0), vec3(0, 1, 0))
    camlut = lev2.CameraDataLut()
    camlut.addCamera("spawncam", cam)

    assert live.clock_abstime == 0.0, "live clock must start at 0"
    render_frames(scene, ctx, camlut, 3)
    t_run = live.clock_abstime
    assert t_run > 0.0, "live clock did not advance: _liveRecompute never fired via onGpuUpdate"
    print("HOOK_FIRED clock_abstime=%.6f after 3 frames" % t_run, flush=True)

    # C.1b dirty-gate, geometry-level: unpaused animated geometry must MOVE between frames
    # (the gate must not over-skip), paused geometry must FREEZE (the gate must actually skip).
    tmpdir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))), ".tmp")
    os.makedirs(tmpdir, exist_ok=True)
    o1 = os.path.join(tmpdir, "livepath_a.obj")
    o2 = os.path.join(tmpdir, "livepath_b.obj")
    lev2.hypermesh.dump_obj(live.mesh, ctx, o1)
    render_frames(scene, ctx, camlut, 2)
    lev2.hypermesh.dump_obj(live.mesh, ctx, o2)
    assert open(o1).read() != open(o2).read(), "animated geometry froze while UNPAUSED (gate over-skips)"
    print("ANIM_MOVES geometry changed across unpaused frames", flush=True)

    lev2.hypermesh.set_clock_paused(True)
    render_frames(scene, ctx, camlut, 2)
    t_paused = live.clock_abstime
    assert t_paused > t_run, "clock should have advanced during the ANIM_MOVES frames"
    lev2.hypermesh.dump_obj(live.mesh, ctx, o1)
    render_frames(scene, ctx, camlut, 2)
    lev2.hypermesh.dump_obj(live.mesh, ctx, o2)
    assert open(o1).read() == open(o2).read(), "paused geometry still moving (gate not skipping)"
    assert live.clock_abstime == t_paused, "global pause must hold the live clock"
    print("PAUSE_HOLDS clock_abstime=%.6f geometry frozen" % t_paused, flush=True)

    lev2.hypermesh.set_clock_paused(False)
    render_frames(scene, ctx, camlut, 2)
    t_resumed = live.clock_abstime
    assert t_resumed > t_paused, "unpause must resume the live clock"
    print("RESUME_ADVANCES clock_abstime=%.6f" % t_resumed, flush=True)
    ok = True
  finally:
    lev2.hypermesh.set_clock_paused(False)
    ezapp.mainThreadEnd()
    print("=== hypermesh livepath gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
