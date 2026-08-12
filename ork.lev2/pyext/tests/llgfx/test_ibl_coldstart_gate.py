#!/usr/bin/env ork.python
################################################################################
# BAKED IBL COLD START gate: the FIRST lit frame must already be lit by the
# scene's baked environment.
#
# The defect this pins: a baked skybox (.xir) is decoded on the concurrent
# queue, uploaded on the loader thread, and published to the render thread
# behind the loader's own fence poll — a chain whose completion is bounded by
# the LOADER's frame pump, not by the render loop's. Nothing about rendering
# more frames makes it land sooner, so any scene that starts FAST (warm shader
# cache) used to reach its first lit frames with the radiance maps still empty
# and light everything off a black IBL. On this bench that was ~0.66 s of
# black-environment frames, and a capture taken on a frame count kept them.
#
# TWO ARMS, because either alone is blind in one direction:
#   STATE  - on the FIRST composited frame the bound radiance maps must already
#            carry their samplers (pbr_common.active_radiance_maps.specular /
#            .diffuse non-null). This is the publish itself, asked at the exact
#            frame the defect leaves it unanswered.
#   IMAGE  - the SAME ground crop, early vs fully settled, within a few percent.
#            The env is ~40% of that crop's brightness, so a late publish reads
#            far darker early than settled. Golden-less: no reference image and
#            no absolute level, only the scene compared against itself.
# A scene whose environment NEVER publishes is uniformly dark and would satisfy
# the image arm alone - the state arm is what refuses it.
#
# NEGATIVE CONTROL (how to see this gate fail): run it with
# ORKID_IBL_COLDSTART_CEILING_SECS=0. That drops the cold-start drain's wall
# ceiling to zero, which is exactly the pre-fix ordering — the first frames
# render against an unpublished set, the engine prints its loud
# [IBL-COLDSTART] FAILED line, and the ratio collapses.
#
# Lifecycle: ComponentizedApplication + StandardSceneGraphComponent — the
# proven offscreen scenegraph-capture pattern (capture_app cannot yet host a
# scenegraph composite offscreen). Verdict emitted BEFORE teardown (defect D1:
# this scene class can SIGSEGV during mainThreadLoop teardown after the verdict
# has flushed).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys

# prepend THIS checkout's scripts dir so ork.testing / ork.app resolve from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine.core import vec3, vec4, mtx4, CrcStringProxy, Path as CorePath  # core FIRST
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

OUTDIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.environ.get("TMPDIR", "/tmp"), "ibl_coldstart_gate")

FIRST_OFFSET  = 2     # see onGpuPostFrame — capture-in-flight, not a settle window
SETTLE_FRAMES = 200   # far past any plausible publish latency
RATIO_TOL     = 0.05  # first vs settled crop mean, fractional

# Open ground, no occluder: the crop must measure IBL + sun on the ground plane
# and nothing whose own residency could confound the first frame (a model load
# is a second async chain). Camera (5,8,10) -> (0,2,0), same rig as the sun
# cascade gate, so the crop sits on lit ground at mid view depth.
CROP_GROUND = (430, 170, 485, 205)


def _crop_mean(arr, rect):
  x0, y0, x1, y1 = rect
  return float(arr[y0:y1, x0:x1].mean())


class GateApp(ComponentizedApplication):

  def __init__(self, outdir):
    super().__init__()
    self._outdir = outdir
    self._frame = 0
    self._built = False
    self._built_frame = 0
    self._phase = 0   # 0: capture first, 1: settle, 2: capture settled, 3: verdict
    self._done = False
    self._caps = {}
    self._published_at_first = None   # STATE arm, sampled on the first composited frame
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
    self.createEzApp(width=640, height=480, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = True

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 4.0
    sun.data.shadowBias = 0.05  # metres
    sun.data.shadowMapSize = 2048
    sun.data.shadowCascadeCount = 3
    sun.data.shadowMaxDistance = 250.0
    sun.shadowCaster = True
    sun.lookAt(vec3(30, 50, 20), vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    lmgr = SGC.scenegraph.lightingmanager
    lmgr.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass  # static scene — deterministic captures

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _capture(self, ctx, tag):
    path = os.path.join(self._outdir, "ibl_coldstart_%s.png" % tag)
    ctx.FBI.captureToFile(self._rtg(ctx).buffer(0), CorePath(path))
    self._caps[tag] = path

  def _verdict(self):
    import numpy
    from PIL import Image
    from ork.testing import verdict
    means = {}
    ok = True
    details = []
    for tag in ("first", "settled"):
      path = self._caps.get(tag)
      if not (path and os.path.isfile(path) and os.path.getsize(path) > 0):
        details.append("%s MISSING" % tag)
        ok = False
        continue
      arr = numpy.asarray(Image.open(path).convert("RGB"), dtype=numpy.float32)
      means[tag] = _crop_mean(arr, CROP_GROUND)
      details.append("%s=%.1f" % (tag, means[tag]))
    published = bool(self._published_at_first)
    details.append("published_at_first=%d" % int(published))
    ok = ok and published
    if len(means) == 2:
      ratio = means["first"] / max(means["settled"], 1e-3)
      matched = abs(1.0 - ratio) <= RATIO_TOL
      ok = ok and matched
      details.append("first/settled=%.3f tol=%.2f" % (ratio, RATIO_TOL))
    self._verdict_code = verdict(
        ok, "baked IBL cold start gate | " + " | ".join(details))

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._built_frame = self._frame
      return
    if self._published_at_first is None:
      # STATE arm, on the very first composited frame — before any capture
      # latency can blur which frame is being talked about.
      maps = self.SGC.pbr_common.active_radiance_maps
      self._published_at_first = bool(
          maps is not None and maps.specular is not None and maps.diffuse is not None)
    if self._done:
      return
    if self._phase == 0:
      # THE observable: the earliest READABLE composited frame. FIRST_OFFSET is
      # 2, not 1, because an offscreen capture issued at postframe N resolves a
      # frame still in flight — offset 1 hands back pixels from before the
      # scene's first composite whatever the lighting did. Nothing is given
      # away: the defect this pins left the environment unpublished for ~0.66 s
      # (hundreds of frames at this frame rate), so offset 2 is deep inside the
      # black window it produced.
      if (self._frame - self._built_frame) >= FIRST_OFFSET:
        self._capture(ctx, "first")
        self._phase = 1
        self._phase_frame = self._frame
    elif self._phase == 1:
      if self._frame >= self._phase_frame + SETTLE_FRAMES:
        self._capture(ctx, "settled")
        self._phase = 2
        self._phase_frame = self._frame
    elif self._phase == 2:
      if self._frame >= self._phase_frame + 20:  # let the async writes land
        self._verdict()
        self._done = True
        self.ezapp.signalExit()


def main():
  from ork.testing import Watchdog
  os.makedirs(OUTDIR, exist_ok=True)
  wd = Watchdog(300.0, label="ibl_coldstart_gate").arm()
  app = GateApp(os.path.abspath(OUTDIR))
  app.ezapp.mainThreadLoop()
  wd.disarm()
  code = getattr(app, "_verdict_code", None)
  if code is None:
    from ork.testing import verdict
    code = verdict(False, "loop exited before captures (no frame evidence)")
  app.ezapp.shutdown()
  sys.exit(code)


if __name__ == "__main__":
  main()
