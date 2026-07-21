#!/usr/bin/env ork.python
################################################################################
# Bug-1 GPU repro (offscreen): a supersample>0 PrimCanvas hosted BESIDE a
# compositor-SSAA SceneGraphViewport composites BLACK.
#
# Root cause: orkshader://blit's fragment uniform block (ublock_frg: FlipY /
# FlipX / ViewportDim) is keyed by the shared FxUniformBlock, so its shadow
# buffer is COMMON to every FreestyleMaterial that loads that shader. The
# compositor output node binds FlipY=1 for its own SSAA resolve; if the Surface
# SSAA resolve leaves those params unbound it inherits the stale flip against
# the wrong dimensions -> out-of-bounds texelFetch -> a black canvas.
#
# This harness renders a bright PrimCanvas (ss=3, LEFT) next to a lit
# SceneGraphViewport (ss=3, RIGHT), captures the main framebuffer, and asserts
# the canvas crop is non-black. RED (canvas_mean ~ 0) on the unfixed engine;
# GREEN (canvas_mean high, matching the bright bg) once Surface::_ssaaResolve
# binds FlipY/FlipX/ViewportDim explicitly.
################################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, numpy

from orkengine.core import *
from orkengine.lev2 import *

tokens = CrcStringProxy()

l2exdir = (lev2exdir() / "python").normalized.as_string
sys.path.append(l2exdir)
from lev2utils.cameras import setupUiCameraX
from lev2utils.primitives import createGridData

CANVAS_PROP = 0.4          # canvas occupies the LEFT 40% of the window
CANVAS_BG   = vec4(0.85, 0.85, 0.90, 1.0)
CAP_FRAME   = 40           # capture after the shared blit UBO is polluted
HARD_LIMIT  = 240          # fail-safe exit (harness also bounds wall-clock)
THRESH      = 0.05         # non-black; the bright bg lands well above this


# --visual: windowed/onscreen run with no capture assert — for DRM/monitor verification,
# where offscreen=True is ILLEGAL (offscreen does not stop DRM scanout; the combo crashes
# in adhoc graphics init). The eyeball IS the observable: canvas region bright = PASS.
VISUAL = "--visual" in sys.argv

class App:
  def __init__(self):
    self.ezapp = OrkEzApp.create(self, width=960, height=600, offscreen=not VISUAL)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg = self.ezapp.topLayoutGroup
    lg.clearColorStd = vec4(0, 0, 0, 1)

    vp_item = lg.makeChild(fill=True, margin=2,
                           uiclass=ui.SceneGraphViewport,
                           args=["Viewport", vec4(0.1, 0.1, 0.12, 1)])
    self.sgv = vp_item.widget
    # SSAA_SGV / SSAA_CANVAS env overrides (default 3): lets the bisect isolate which
    # resolve path blacks out (viewport=compositor SSAA vs canvas=Surface SSAA).
    self.sgv.supersample = int(os.environ.get("SSAA_SGV", "3"))

    canvas_item = lg.split(layout=vp_item.layout, proportion=CANVAS_PROP,
                           placement=tokens.LEFT, margin=2,
                           uiclass=ui.PrimCanvas, args=["canvas"])
    self.canvas = canvas_item.widget
    self.canvas.bg_color = CANVAS_BG
    self.canvas.draw_background = True
    self.canvas.supersample = int(os.environ.get("SSAA_CANVAS", "3"))  # Surface SSAA resolve — the path under test

    self.frame = 0
    self.captured = False
    self.canvas_mean = -1.0
    self.vp_mean = -1.0
    self._cap_pending = False
    self._cap_inflight = False
    self._cap_async = None
    self._cap_buf = None

  def onGpuInit(self, ctx):
    self.canvas.gpuInit(ctx)
    self.main_layer = self.canvas.createLayer("main")
    pipeline = self.canvas.pipelineSolid
    self.quad_prim = ui.QuadPrimitive(pipeline=pipeline)
    qd = ui.QuadData()
    qd.setPosition(20, 20)
    qd.setSize(180, 180)
    qd.setColor(vec4(1.0, 0.4, 0.1, 1.0))
    self.quad_prim.addQuad(qd)
    self.main_layer.addPrimitive(self.quad_prim)
    self.canvas.markDirty()

    self.cameralut = self.ezapp.vars.cameras
    sg_params = VarMap()
    sg_params.preset = "ForwardPBR"
    sg_params.SkyboxIntensity = 3.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(0.3)
    sg_params.dbufcontext = self.ezapp.vars.dbufcontext
    self.scenegraph = scenegraph.Scene(sg_params)
    self.layer = self.scenegraph.createLayer("std_forward")
    # Ownership is C++-held since the sgnode drawable-data lifetime fix: the impl
    # retains a shared_ptr<const GridDrawableData>, so passing a temporary is safe.
    self.grid_node = self.layer.createDrawableNodeFromData("grid", createGridData())

    self.camera, self.uicam = setupUiCameraX(cameralut=self.cameralut, camname="spawncam")
    self.uicam.lookAt(vec3(6, 6, 10), vec3(0, 0, 0), vec3(0, 1, 0))
    self.uicam.updateMatrices()
    self.camera.copyFrom(self.uicam.cameradata)
    self.sgv.cameraName = "spawncam"
    self.sgv.scenegraph = self.scenegraph

  def onUpdate(self, updinfo):
    self.scenegraph.updateScene(self.cameralut)
    self.canvas.markDirty()
    self.sgv.setDirty()
    self.frame += 1
    if self.frame == CAP_FRAME and not self.captured:
      self._cap_pending = True
    if VISUAL:
      return  # visual mode: run until killed; the monitor is the observable
    if self.frame > HARD_LIMIT and not self.captured:
      print("[bug1-repro] HARD_LIMIT reached without a completed capture", flush=True)
      self.ezapp.signalExit()

  def onGpuPostFrame(self, ctx):
    if self.captured:
      return
    if self._cap_pending and not self._cap_inflight:
      rtg = ctx.FBI.main_RTG
      self._cap_buf = CaptureBuffer()
      self._cap_async = ctx.FBI.captureAsFormat(rtg.buffer(0), self._cap_buf, "RGBA8")
      self._cap_inflight = True
    elif self._cap_inflight:
      if self._cap_async is None or bool(self._cap_async.is_ready):
        self._finish()

  def _finish(self):
    capbuf = self._cap_buf
    w, h = capbuf.width, capbuf.height
    arr = numpy.array(capbuf, dtype=numpy.uint8).reshape(h, w, 4)
    rgb = arr[..., :3].astype(numpy.float32) / 255.0

    y0, y1 = int(0.30 * h), int(0.70 * h)
    cx0, cx1 = int(0.05 * w), int((CANVAS_PROP - 0.08) * w)
    vx0, vx1 = int(0.55 * w), int(0.95 * w)
    self.canvas_mean = float(rgb[y0:y1, cx0:cx1].mean())
    self.vp_mean = float(rgb[y0:y1, vx0:vx1].mean())
    self.captured = True

    print(f"[bug1-repro] size={w}x{h} canvas_crop_mean={self.canvas_mean:.5f} "
          f"viewport_crop_mean={self.vp_mean:.5f}", flush=True)
    try:
      from PIL import Image
      out = os.environ.get("BUG1_PNG", "/tmp/bug1_repro.png")
      Image.fromarray(arr[..., :3][::-1]).save(out)
      print(f"[bug1-repro] wrote {out}", flush=True)
    except Exception as e:
      print(f"[bug1-repro] png save skipped: {e}", flush=True)
    self.ezapp.signalExit()


def main():
  app = App()
  app.ezapp.mainThreadLoop()
  ok = app.canvas_mean > THRESH
  verdict = "PASSED (canvas non-black)" if ok else "FAILED (canvas BLACK)"
  print(f"\n=== bug1 multisurface SSAA gate {verdict} : "
        f"canvas_mean={app.canvas_mean:.5f} viewport_mean={app.vp_mean:.5f} "
        f"thresh={THRESH} ===", flush=True)
  sys.exit(0 if ok else 1)


main()
