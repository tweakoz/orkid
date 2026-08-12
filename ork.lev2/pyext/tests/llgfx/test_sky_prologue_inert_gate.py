#!/usr/bin/env ork.python
################################################################################
# SKYLIGHT lane B slice B1 — ARMED-PROLOGUE INERTNESS gate.
#
# The Hillaire LUT build is a NEW step in ForwardPbrNodeImpl::_render_prologue,
# after _update_env_probes and inside the prologue's pushCPD/popCPD bracket. Two
# things must hold in slice B1 and both are asserted here, in ONE process so the
# two captures share an identical warm shader cache:
#
#   1. ARMING IT DOESN'T CRASH. With an atmosphere attached the step runs every
#      composited frame inside the real CPD bracket — it opens render passes,
#      transitions the LUT images for sampling, and reuses the prologue's RCFD.
#      All of that is exercised for real here, not against a synthetic context.
#
#   2. ARMING IT DOESN'T MOVE A PIXEL. Nothing consumes the sky-view LUT yet
#      (FWD_SKYBOX_PROC is a later slice), so an armed frame must be BYTE
#      IDENTICAL to a disarmed one. This is the committed, in-repo half of the
#      byte-identity requirement — the blessed spot_shadow/probe canaries cover
#      the disarmed build, this covers arm-vs-disarm on one warm process.
#
# Capture A is taken with pbr_common.atmosphere unset; the atmosphere is then
# attached live and capture B taken after the same settle margin. Static scene,
# no time-driven animation, so the only difference between the two frames is the
# prologue step itself.
#
# NOTE: as of slice B2 the sky DOES reach the screen — but only when the scene's
# sky SOURCE is switched (pbr_common.sky_source = "procedural"), which this gate
# never does. Arming an atmosphere alone still moves no pixel, so check #2 stands
# exactly as written. If it ever fails, something started consuming the LUTs
# unconditionally; re-point this gate at the new expectation rather than deleting
# it. The switched-on case is covered by test_sky_proc_gate.py.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import numpy
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

tokens = CrcStringProxy()

SETTLE_FRAMES = 60   # after scene build, before capture A
ARM_SETTLE    = 30   # after arming the atmosphere, before capture B


class SkyPrologueInertApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._built_frame = 0
    self._phase = 0          # 0 settle -> 1 capA -> 2 arm+settle -> 3 capB -> 4 done
    self._phase_frame = 0
    self._done = False
    self._armed_ok = None
    self._shots = {}
    self._inflight = None
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(5, 8, 10), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant="_V4",
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0.1),
        })
    self.createEzApp(width=512, height=384, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = True

    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.drawable_model = model.createDrawable()
    self.modelnode = SGC.scenegraph.createDrawableNodeOnLayers(
        SGC.fwd_layers, "occluder", self.drawable_model)
    self.modelnode.worldTransform.translation = vec3(0, 2, 0)

    # a fixed sun: the prologue's LUT step picks its direction off the first
    # enumerated directional light, so this exercises the real sun-pick path
    # rather than the overhead fallback.
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 4.0
    sun.data.shadowBias = 0.05  # metres
    sun.data.shadowMapSize = 2048
    sun.data.shadowCascadeCount = 3
    sun.shadowCaster = True
    sun.lookAt(vec3(30, 50, 20), vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass   # fully static — the two captures differ only by the prologue step

  ##############################################################

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _issueCapture(self, ctx):
    buf = lev2.CaptureBuffer()
    fut = ctx.FBI.captureAsFormat(self._rtg(ctx).buffer(0), buf, "RGBA8")
    self._inflight = (fut, buf)

  def _collect(self, key):
    fut, buf = self._inflight
    if not bool(fut.is_ready):
      return False
    w, h = buf.width, buf.height
    self._shots[key] = numpy.array(buf, dtype=numpy.uint8).reshape(h, w, 4)[..., :3].copy()
    self._inflight = None
    print("[sky-inert] captured %s %dx%d mean=%.4f" %
          (key, w, h, float(self._shots[key].mean())), flush=True)
    return True

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._built_frame = self._frame
        self._phase_frame = self._frame
      return

    if self._phase == 0:
      if self._frame >= self._phase_frame + SETTLE_FRAMES:
        self._issueCapture(ctx)
        self._phase = 1
      return

    if self._phase == 1:
      if self._collect("disarmed"):
        # ARM the procedural atmosphere live
        try:
          self.SGC.pbr_common.atmosphere = lev2.SkyAtmosphereData()
          self._armed_ok = (self.SGC.pbr_common.atmosphere is not None)
        except Exception as e:
          print("[sky-inert] ARM FAILED: %r" % (e,), flush=True)
          self._armed_ok = False
        print("[sky-inert] atmosphere armed=%s" % (self._armed_ok,), flush=True)
        self._phase = 2
        self._phase_frame = self._frame
      return

    if self._phase == 2:
      if self._frame >= self._phase_frame + ARM_SETTLE:
        self._issueCapture(ctx)
        self._phase = 3
      return

    if self._phase == 3:
      if self._collect("armed"):
        self._phase = 4
        self._emitVerdict()
      return

  ##############################################################

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    a = self._shots.get("disarmed")
    b = self._shots.get("armed")

    check("atmosphere_armed", bool(self._armed_ok))
    check("captures_present", a is not None and b is not None)

    diff_pixels = -1
    if a is not None and b is not None:
      check("disarmed_nonblack", float(a.mean()) > 1.0, "mean=%.4f" % float(a.mean()))
      check("armed_nonblack", float(b.mean()) > 1.0, "mean=%.4f" % float(b.mean()))
      check("same_extent", a.shape == b.shape, "%s vs %s" % (a.shape, b.shape))
      if a.shape == b.shape:
        diff = (a.astype(numpy.int16) - b.astype(numpy.int16))
        diff_pixels = int((numpy.abs(diff).sum(axis=2) > 0).sum())
        check("armed_frame_byte_identical", diff_pixels == 0,
              "differing_pixels=%d maxdelta=%d" % (diff_pixels, int(numpy.abs(diff).max())))

    ok = (len(failures) == 0)
    detail = "differing_pixels=%d" % diff_pixels
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = SkyPrologueInertApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before both captures completed")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
