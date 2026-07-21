################################################################################
# ork.testing.capture — proven offscreen capture, verdict-before-teardown clean.
#
# Builds on the app.py lifecycle machinery (the env/DRM guards, asset preflight,
# output-dir creation, and async settle) and the ONE capture path that both renders
# a non-black frame AND tears the update thread down cleanly: the mainThreadLoop /
# onGpuPostFrame idiom (the code_view / player os_snapdrain pattern).
#
# WHY not an inline "clear + read back" or an iter-driven grab: both were tried and
# both are footguns here — the inline no-loop grab aborts, and the manual mainThreadIter
# teardown DEADLOCKS on the un-joined update thread (#60) while also reading main_RTG
# black. mainThreadLoop's binding joins the update side before shutdown, so it is the
# only teardown-clean capture path.
#
# Ordering guarantee (capture -> settle -> readback-complete -> BEFORE teardown):
# the frame is rendered, a settle margin of frames is drawn, the async readback is
# issued and polled to completion, the PNG is written (its parent dir created first —
# #71), and a machine-readable CAPTURE= line is FLUSHED — all BEFORE signalExit lets
# the loop exit into teardown. So even a teardown SIGSEGV leaves the PNG + the readback
# line on record (pairs with the verdict protocol / #57).
#
# ssaa defaults to 0: ssaa=2 hits a transient-FBO resolve SIGSEGV (#70); a higher value
# is an explicit, documented opt-in.
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy   # core FIRST (import-order law)
from orkengine import lev2

import contextlib
import os
import sys

from ork.testing.app import apply_env_guards, preflight_assets, ensure_parent_dir

tokens = CrcStringProxy()


def _default_setup(app, ctx):
  """The default scene: none — the fill PrimCanvas draws its solid bg_color, a real,
  asset-free, camera-free, deterministically non-black offscreen frame. A caller that
  wants a real scene passes setup=/update= (and typically its own viewport widget)."""
  return None


class _CaptureApp(object):
  """The internal capture driver: a topWidget UI hosting one fill PrimCanvas (a solid
  non-black surface by default), an optional test-built scene, and an onGpuPostFrame
  state machine that settles, grabs main_RTG, writes the PNG, and only then signals
  exit. The PrimCanvas path renders non-black with no assets and no camera — the grid /
  skybox scene is deferred to the test via setup= (it needs a camera + asset loads)."""

  def __init__(self, cfg):
    self._cfg = cfg
    self.ezapp = lev2.OrkEzApp.create(
        self, width=cfg["width"], height=cfg["height"],
        offscreen=True, ssaa=cfg["ssaa"])
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    lg.clearColorStd = cfg["clear_color"]
    self.canvas = lg.makeChild(
        fill=True, uiclass=lev2.ui.PrimCanvas, args=["cap_canvas"]).widget
    cc = cfg["clear_color"]
    self.canvas.clearColor = vec3(cc.x, cc.y, cc.z)   # the Surface clear = the frame fill
    self.canvas.bg_color = cc
    self.canvas.draw_background = True
    self.viewport = self.canvas   # alias for a setup= that expects a render surface

    self.scene = None
    self.cameralut = None

    self._frame = 0
    self._target_path = None
    self._settle_frames = cfg["settle_frames"]
    self._cap_armed = False
    self._cap_inflight = False
    self._cap_async = None
    self._cap_buf = None
    self.result = None

  # ---- lifecycle callbacks (mainThreadLoop drives these) ----

  def onGpuInit(self, ctx):
    self.ctx = ctx
    setup = self._cfg["setup"] or _default_setup
    setup(self, ctx)

  def onUpdate(self, updinfo):
    upd = self._cfg["update"]
    if upd is not None:
      upd(self, updinfo)
    elif self.scene is not None and self.cameralut is not None:
      self.scene.updateScene(self.cameralut)
    mark = getattr(self.canvas, "markDirty", None)
    (mark or self.canvas.setDirty)()

  def onGpuPostFrame(self, ctx):
    self._frame += 1
    if self.result is not None or self._target_path is None:
      return
    if not self._cap_armed:
      if self._frame >= self._settle_frames:
        self._cap_armed = True
      return
    if not self._cap_inflight:
      self._cap_buf = lev2.CaptureBuffer()
      rtg = ctx.FBI.main_RTG
      self._cap_async = ctx.FBI.captureAsFormat(rtg.buffer(0), self._cap_buf, "RGBA8")
      self._cap_inflight = True
      return
    # readback in flight — complete it BEFORE any teardown.
    if self._cap_async is None or bool(self._cap_async.is_ready):
      self._finishCapture()

  def _finishCapture(self):
    import numpy
    capbuf = self._cap_buf
    w, h = capbuf.width, capbuf.height
    arr = numpy.array(capbuf, dtype=numpy.uint8).reshape(h, w, 4)
    rgb = arr[..., :3]
    mean = float(rgb.mean())
    mx = int(rgb.max())
    nonblack = mx > 0 and mean > 1.0     # a real frame, not the zero buffer
    path = self._target_path
    ensure_parent_dir(path)              # #71: create the dir BEFORE the write
    wrote = False
    try:
      from PIL import Image
      Image.fromarray(rgb).transpose(Image.FLIP_TOP_BOTTOM).save(path)
      wrote = True
    except Exception as e:
      sys.stdout.write("CAPTURE_PNG_ERROR path=%s err=%r\n" % (path, e))
      sys.stdout.flush()
    self.result = {"path": path, "width": w, "height": h,
                   "mean": mean, "max": mx, "nonblack": nonblack, "wrote": wrote}
    # readback-complete line, FLUSHED, BEFORE teardown (#57 evidence).
    sys.stdout.write("CAPTURE=%s width=%d height=%d mean=%.4f max=%d nonblack=%d\n"
                     % (path, w, h, mean, mx, int(nonblack)))
    sys.stdout.flush()
    self._cap_inflight = False
    self._cap_async = None
    self._cap_buf = None
    self.ezapp.signalExit()              # NOW let the loop exit into its clean teardown

  def capture(self, path):
    """Render, settle, grab main_RTG, write `path`, and return the result dict. The
    whole capture->settle->readback->write happens before teardown; mainThreadLoop's
    binding then joins the update side and shuts down cleanly on return."""
    self._target_path = str(path)
    self.ezapp.mainThreadLoop()          # self-terminating on the in-callback signalExit
    if self.result is None:
      raise RuntimeError("ork.testing capture: loop exited before the readback completed "
                         "(path=%s)" % (path,))
    return self.result


@contextlib.contextmanager
def capture_app(width=512, height=384, clear_color=vec4(0.15, 0.35, 0.55, 1),
                assets=None, setup=None, update=None,
                ssaa=0, settle_frames=16, drm_guard=True):
  """Offscreen capture context. Enter: env/DRM guards + asset preflight (fail-loud,
  #54-class) BEFORE engine init, then build the offscreen render app. Yields a driver
  whose .capture(path) renders and writes a non-black PNG with the proven ordering.

  setup(app, ctx) / update(app, updinfo): build + animate your own scene (default is a
  lit grid). ssaa=0 by default — ssaa=2 is the #70 resolve-crash footgun (opt in
  explicitly if you must). assets is the explicit preflight list."""
  if ssaa == 2:
    sys.stdout.write("ork.testing capture_app: ssaa=2 requested — KNOWN BUG #70 "
                     "(transient-FBO resolve SIGSEGV). Proceeding as explicit opt-in.\n")
    sys.stdout.flush()
  if drm_guard:
    apply_env_guards(offscreen=True)
  preflight_assets(assets)

  cfg = {"width": int(width), "height": int(height),
         "clear_color": clear_color, "assets": assets,
         "setup": setup, "update": update,
         "ssaa": int(ssaa), "settle_frames": int(settle_frames)}
  app = _CaptureApp(cfg)
  try:
    yield app
  finally:
    # mainThreadLoop (inside capture()) already ran the clean teardown; if capture()
    # was never called, signal the loop is unused and let the app drop.
    if app.result is None:
      try:
        app.ezapp.signalExit()
      except Exception:
        pass
