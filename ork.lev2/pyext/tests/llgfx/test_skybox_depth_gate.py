#!/usr/bin/env ork.python
################################################################################
# SKYBOX DEPTH GATE (seam S3) — the sky must not z-block distant geometry.
#
# THE DEFECT (fixed 2026-07-25): the forward skybox pass drew its fullscreen quad
# at NDC z 0.9999 WITH depth-write on. 0.9999 is not "far" — in a [0,1] depth
# buffer it is the finite eye distance n/(1-z) ~= 10^4*near (~4.8 km at near=0.5),
# so every depth-tested fragment beyond that was z-REJECTED (50-100 km cloud
# shells vanished; cloud decks had to ship depth_test=off, losing terrain
# occlusion). The written sky depth also seeded the NEXT frame's HZB with a false
# occluder, so the same distance cut applied to occlusion culling — implicitly
# covered here: a re-blocked sky fails this gate through either path.
#
# THE FIX lives in the TECHNIQUE STATE BLOCK (sb_skybox, orkshader://pbr:
# DepthMask=false), NOT in the rasterstate _render_skybox sets: on a priority tie
# the technique state block WINS over the rasterstate stack when the pipeline is
# built (VkFxInterface::_fetchPipeline), and FxPipeline never pushes its
# _rasterstate — so the pass code's setWriteMaskZ/setDepthTest reach the draw as
# the dynamic cull mode only. A "fix" applied to the C++ rasterstate alone does
# NOT change what the GPU does; this gate is what catches that.
# The quad z stays 0.9999: it still has to beat the 1.0 depth CLEAR for the
# technique's LESS test. Only the WRITE is gone.
#
# WHAT IS MEASURED: one static offscreen frame, camera far pinned to 100 km,
# two depth-tested balls scaled by their NODE matrix (authored geometry stays
# unit-scale, so the S4 multi-km vertex collapse is not in play):
#   GREEN control  at    60 m — renders in any build; guards against scoring an
#                               empty/black frame as a PASS.
#   RED   subject  at 20 000 m — INVISIBLE with the defect, visible without it.
# Teeth (proven at authoring): reverting the sb_skybox DepthMask makes this gate
# FAIL with far_px=0 while the control is untouched.
#
# S3_SKYBOX=0 runs the same scene with the skybox pass DISABLED — the reference
# mode: the far ball must render there in ANY build, which is what isolates the
# sky pass (rather than the far plane, culling, or the material) as the blocker.
# The normal run is skybox-ON and demands the far ball anyway.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import time
import numpy
from PIL import Image as PILImage
from orkengine.core import vec3, vec4, CrcStringProxy, Path as CorePath
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

tokens = CrcStringProxy()

WIDTH, HEIGHT = 512, 384
SETTLE_FRAMES = 90
ENV_WAIT_SECONDS = 90.0

FAR_Z   = 20000.0        # the distant subject — 4x past the defect's ~4.8 km cut
NEAR_Z  = 60.0           # the control
CAM_NEAR, CAM_FAR = 0.5, 100000.0
MIN_PIXELS = 500         # both balls measure ~2300-3600 px; a floor, not a match

ENABLE_SKYBOX = os.environ.get("S3_SKYBOX", "1") != "0"
OUT = os.environ.get("S3_OUT", "/tmp/skybox_depth_gate/capture.png")


class SkyboxDepthApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._built_frame = 0
    self._cap = 0
    self._done = False
    self._env_ready = False
    self._t_start = time.time()
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0, 2, 0), tgt=vec3(0, 2, 100), up=vec3(0, 1, 0),
        explicit_near_far=True,
        near=CAM_NEAR, far=CAM_FAR,
        grid_variant=None,     # no grid: the two balls are the only geometry
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0.3),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    self.ezapp.topWidget.enableUiDraw()
    SGC.pbr_common.enable_skybox = ENABLE_SKYBOX

    self.ctrl_node = SGC.createBallNode(
        "ctrl_near", ctx=ctx, position=vec3(-14, 2, NEAR_Z), scale=6.0,
        color=vec4(0, 1, 0, 1), metallic=0.0, roughness=0.6)
    self.far_node = SGC.createBallNode(
        "far_20km", ctx=ctx, position=vec3(4000, 500, FAR_Z), scale=1600.0,
        color=vec4(1, 0, 0, 1), metallic=0.0, roughness=0.6)

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 6.0
    sun.shadowCaster = False
    sun.lookAt(vec3(30, 60, -30), vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

    # setupUiCameraX copies the EDITOR-cam near/far (mfLoc*0.01, x10000) into the
    # render CameraData BEFORE explicit_near_far is applied, which would pin far
    # to 10 km — in front of the 20 km subject, i.e. a far-plane clip masquerading
    # as the defect. Pin the projection directly; no UI event reaches this
    # offscreen app, so nothing overwrites it.
    SGC.camera.perspective(CAM_NEAR, CAM_FAR, 45.0)
    SGC.camera.lookAt(vec3(0, 2, 0), vec3(0, 2, 100), vec3(0, 1, 0))

  def _onUpdate(self, updinfo):
    pass   # fully static: one deterministic frame

  ##############################################################

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._built_frame = self._frame
      return

    # The 4k .xir env map streams in asynchronously while an offscreen context
    # runs thousands of frames per second, so a frame budget is nowhere near a
    # load. Waiting in WALL time makes the sky a real image — and a resident
    # skybox is the precondition for the pass under test having run at all.
    if ENABLE_SKYBOX and not self._env_ready:
      maps = self.SGC.pbr_common.RadianceMaps
      if (maps is None) or (maps.specular is None):
        if (time.time() - self._t_start) > ENV_WAIT_SECONDS:
          verdict(False, "baked env map never became resident")
          self._exit_code = 1
          self._done = True
          self.ezapp.signalExit()
        return
      self._env_ready = True
      self._built_frame = self._frame
      return

    if self._frame < self._built_frame + SETTLE_FRAMES:
      return
    if not self._cap:
      d = os.path.dirname(OUT)
      if d:
        os.makedirs(d, exist_ok=True)
      ctx.FBI.captureToFile(self._rtg(ctx).buffer(0), CorePath(OUT))
      self._cap = self._frame
      return
    if self._frame < self._cap + 20:
      return
    self._emitVerdict()

  ##############################################################
  # observables
  ##############################################################

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    far_px = ctrl_px = -1
    mean = 0.0
    have = os.path.isfile(OUT) and os.path.getsize(OUT) > 0
    check("capture_present", have, OUT)
    if have:
      arr = numpy.asarray(PILImage.open(OUT).convert("RGB"), dtype=numpy.int16)
      r, g, b = arr[..., 0], arr[..., 1], arr[..., 2]
      # channel-dominance classification: the balls are pure red / pure green,
      # the sky is desaturated blue-white and the ambient-lit background never
      # reaches a 40-count dominance.
      far_px  = int(numpy.count_nonzero((r > g + 40) & (r > b + 40)))
      ctrl_px = int(numpy.count_nonzero((g > r + 40) & (g > b + 40)))
      mean = float(arr.mean())
      # the control proves the frame rendered at all — without it, "no red" would
      # score a black/broken frame as the defect (or worse, the reverse).
      check("control_ball_60m_visible", ctrl_px >= MIN_PIXELS, "ctrl_px=%d" % ctrl_px)
      check("far_ball_20km_visible", far_px >= MIN_PIXELS, "far_px=%d" % far_px)
      if ENABLE_SKYBOX:
        check("sky_nonblack", mean > 1.0, "mean=%.3f" % mean)

    ok = (len(failures) == 0)
    detail = ("skybox=%d far_px=%d ctrl_px=%d far_z=%.0f cam_far=%.0f mean=%.3f %s"
              % (int(ENABLE_SKYBOX), far_px, ctrl_px, FAR_Z, CAM_FAR, mean, OUT))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = SkyboxDepthApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the capture completed")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
