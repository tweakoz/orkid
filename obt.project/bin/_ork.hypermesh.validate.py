#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
################################################################################
# _ork.hypermesh.validate.py — OFFSCREEN hypermesh asset validator (the subprocess ork.hypermesh.viewer.py
# --watch spawns on a file change). It loads the asset, renders it offscreen through the SAME scene path
# the viewer uses (headless StandardSceneGraphComponent), captures the rendered frame, runs a cheap
# content check, and EXITS with a code the parent reads to decide whether the asset is safe to live-reload:
#
#   exit 0  PASS    — rendered, content present   -> parent reloads
#   exit 2  BLANK   — rendered but empty/flat      -> parent skips (asset is structurally fine but draws nothing)
#   exit 3  ERROR   — exception while loading/rendering
#   exit 4  TIMEOUT — GPU capture never completed
# It also prints "HMVALIDATE_RESULT=PASS|BLANK|ERROR|TIMEOUT" to stdout BEFORE teardown, so the decision
# survives even if GPU teardown SIGABRTs (exit-134 history) — the parent prefers that token.
#
#   ork.python obt.project/bin/_ork.hypermesh.validate.py RippleGrid ; echo "rc=$?"
#
# -o ROUTES BY EXTENSION (works headless with ANY hypermesh asset, search- or -i path-mode):
#   -o foo.obj  -> materialize the asset + dump the MESH to foo.obj (the headless equivalent of the
#                  viewer's [O] key; no GUI needed). Skips the pixel/content check.
#   -o foo.png  -> dump the captured RENDER frame to foo.png (the original render diagnostic).
#   ./_ork.hypermesh.validate.py racer -o /tmp/racer.obj
################################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time
from orkengine.core import vec3, CrcStringProxy   # core MUST import before lev2
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.hypergraph.dflow.hypermesh import make_drawable
from ork.hypergraph.assets.hypermesh._resolve import (
    resolve_asset, load_asset_from_path, EXIT_OK, EXIT_BLANK, EXIT_LOAD_ERROR, EXIT_TIMEOUT, EXIT_VETFAIL,
    RESULT_TOKEN)

tokens = CrcStringProxy()

DIM            = 512    # offscreen render dimension
WARMUP_FRAMES  = 4      # minimal frames before the first capture poll
POLL_EVERY     = 8      # frames between capture+inspect polls
SETTLE_S       = 0.75   # once a frame is non-black, keep rendering this long before finalizing (lets the
                        # async IBL envmap finish loading + any blocking shader recompiles settle) — time
                        # ALONE is unreliable, so we gate on actual pixel content, then settle from there
APPEAR_TIMEOUT_S = 15.0 # max wall-time waiting for ANY content to appear -> else BLANK
CAP_TIMEOUT_S  = 6.0    # max wait for a single GPU readback -> TIMEOUT
NB_MIN         = 0.005  # >=0.5% of sampled px must differ from the (corner-estimated) background.
                        # NOT 2%: sparse-but-REAL geometry reads low (delete_demo's open bowl = a dim
                        # interior, 1.4% of samples) and was false-negatived as BLANK; a true blank is
                        # 0% and a flat fill is still rejected by SPREAD_MIN, so 0.5% loses no safety.
SPREAD_MIN     = 0.03   # AND luminance spread >= this (rejects a flat single-color fill)


class ValidatorApp(ComponentizedApplication):
  def __init__(self, asset_name, asset_path=None, png_path=None, obj_path=None):
    super().__init__()
    self._asset_name = asset_name
    self._asset_path = asset_path   # -i: load from this explicit .py path (overrides search)
    self._label      = asset_path or asset_name
    self._png_path   = png_path     # -o *.png: dump the captured frame to this PNG (diagnostic)
    self._obj_path   = obj_path     # -o *.obj: dump the materialized mesh to this OBJ (any asset)
    self._obj_written = False
    self._result     = EXIT_LOAD_ERROR   # pessimistic until proven otherwise
    self._token      = "ERROR"
    self._frame       = 0
    self._future      = None
    self._capbuf      = None
    self._cap_start   = 0.0
    self._done        = False
    self._want_exit   = False  # set by _finish; the actual signalExit() is issued from onGpuPostFrame, so
                               # an error during _onGpuInit doesn't signalExit before the loop is running
                               # (which crashes teardown) — it exits cleanly on the first frame instead
    self._start_time  = 0.0    # when rendering began (first post-frame)
    self._next_poll   = 0      # next frame index to start a capture poll
    self._saw_content = False  # have we seen a non-black frame yet?
    self._content_at  = 0.0    # wall-time of the first non-black frame
    def _env_vec3(name, dflt):                        # HMVAL_EYE / HMVAL_TGT = "x,y,z" to orbit the inspect cam
      s = os.environ.get(name)
      if not s: return dflt
      p = [float(c) for c in s.replace(" ", "").split(",")]
      return vec3(p[0], p[1], p[2])
    self.SGC = self.addComponent("std_scenegraph", StandardSceneGraphComponent,
                                 eye=_env_vec3("HMVAL_EYE", vec3(6, 5, 9)),
                                 tgt=_env_vec3("HMVAL_TGT", vec3(0, 0, 0)), up=vec3(0, 1, 0),
                                 grid_variant=None)
    # lockstep mode (mirrors the working ork.lev2/pyext/tests/movie/complex.py): forces offscreen AND
    # composites each frame into fbi->_main_rtg — the buffer the movie recorder / our capture reads.
    self.createEzApp(enable_lockstep_ups=True, enable_lockstep_fps=True,
                     enable_freerun_ups=True, enable_freerun_fps=True,
                     freerun=False, target_ups=60, target_fps=60,
                     width=DIM, height=DIM,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  def _finish(self, code, token):
    if self._done:
      return
    self._result    = code
    self._token     = token
    self._done      = True
    self._want_exit = True                    # signalExit() issued from onGpuPostFrame (loop running)
    print(RESULT_TOKEN + token, flush=True)   # emit the decision BEFORE any teardown (survives SIGABRT)
    if self._obj_written:                      # the kept OBJ = the last poll's write (= the captured frame)
      print("hmvalidate: dumped OBJ -> %s (verts=%d faces=%d)"
            % (self._obj_path, self._live.mesh.num_verts, self._live.mesh.num_faces), flush=True)

  def _onGpuInit(self, ctx):
    self._ctx = ctx
    self.ezapp.topWidget.enableUiDraw()   # OFFSCREEN: a windowed app enables UI draw by default; we must
                                          # do it explicitly so the SGC viewport composites into _main_rtg.
    try:
      asset_cls   = (load_asset_from_path(self._asset_path) if self._asset_path
                     else resolve_asset(self._asset_name))     # -i path OR search-mode stem
      self._asset_cls = asset_cls                              # meshvet: VET expectations + baseline location
      import inspect
      try:    self._asset_file = self._asset_path or inspect.getsourcefile(asset_cls)
      except Exception: self._asset_file = self._asset_path
      self._asset = asset_cls()
      self._live  = self._asset.materialize_live(ctx)            # may raise on a bad graph -> ERROR
      cdd, _gmtl  = make_drawable(self._live, ctx, animated=False, material_cls=None,
                                  instances=getattr(self._asset, "instances", None))  # asset opts into instancing
      self.node   = self.SGC.layer_fwd.createDrawableNodeFromData("hmvalidate", cdd)
      # keep the skybox LOADED (it's the IBL light source — the geometry must be lit to be visible) but
      # DON'T render it, so the background stays black for the content check.
      self.SGC.pbr_common.enable_skybox = False
      print("hmvalidate: %s materialized verts=%d faces=%d"
            % (self._label, self._live.mesh.num_verts, self._live.mesh.num_faces), flush=True)
    except Exception as e:
      import traceback; traceback.print_exc()
      print("hmvalidate: load/materialize error: %s" % e, flush=True)
      self._finish(EXIT_LOAD_ERROR, "ERROR")

  def _onUpdate(self, updinfo):
    if not self._done:
      self.SGC.scenegraph.updateScene(self.SGC.cameralut)

  # onGpuPostFrame is wired to the CONTEXT post-frame callback (ezapp.cpp:1172) — the exact point the movie
  # recorder captures fbi->_main_rtg from (ezapp.cpp:1393). Capturing main_RTG here (after super() lets the
  # SGC composite) gets the finished frame; capturing earlier (e.g. _onGpuUpdate) reads an unrendered buffer.
  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)        # broadcast to components (SGC renders the scene into its viewport RTG)
    if self._want_exit:                # deferred clean exit (issued here, never from _onGpuInit)
      self._want_exit = False
      self.ezapp.signalExit()
      return
    if self._done:
      return
    self._frame += 1
    now = time.time()
    if self._start_time == 0.0:
      self._start_time = now
    try:
      # --- no capture in flight: start a poll every POLL_EVERY frames ---
      if self._future is None:
        if self._frame >= WARMUP_FRAMES and self._frame >= self._next_poll:
          # the SGC viewport renders the scene into its OWN surface RTG (not the window's _main_rtg).
          rtg = getattr(self.SGC.SGVPW, "rtgroup", None)
          if rtg is None or rtg.numBuffers < 1:
            if now - self._start_time > APPEAR_TIMEOUT_S:
              print("hmvalidate: viewport RTG never ready", flush=True)
              self._finish(EXIT_BLANK, "BLANK")
            return
          self._capbuf = lev2.CaptureBuffer()
          self._future = ctx.FBI.captureAsFormat(rtg.buffer(0), self._capbuf, "RGBA8")
          # -o dumps on the SAME frame as the capture (last poll's write = the kept one = the settled frame),
          # so the OBJ mesh corresponds exactly to the rendered frame / PNG (matters for animated assets).
          if self._png_path:
            from orkengine.core import Path as _CorePath
            ctx.FBI.captureToFile(rtg.buffer(0), _CorePath(self._png_path))
          if self._obj_path:
            lev2.hypermesh.dump_obj(self._live.mesh, ctx, self._obj_path)
            self._obj_written = True
          self._cap_start = now
        return
      # --- capture in flight: wait for the readback ---
      if not self._future.is_ready:
        if now - self._cap_start > CAP_TIMEOUT_S:
          print("hmvalidate: capture timeout", flush=True)
          self._finish(EXIT_TIMEOUT, "TIMEOUT")
        return
      # --- readback ready: inspect this poll ---
      has = _content_check(self._capbuf)
      self._future = None
      self._capbuf = None
      self._next_poll = self._frame + POLL_EVERY
      if has:
        if not self._saw_content:
          self._saw_content = True
          self._content_at  = now
          print("hmvalidate: first content at frame %d (t=%.2fs) — settling %.2fs"
                % (self._frame, now - self._start_time, SETTLE_S), flush=True)
        elif now - self._content_at >= SETTLE_S:         # content present + settled -> safe
          self._finish(EXIT_OK, "PASS")
      else:
        self._saw_content = False                        # require content to be STABLE across the settle
        if now - self._start_time > APPEAR_TIMEOUT_S:    # never got (stable) content -> blank
          self._finish(EXIT_BLANK, "BLANK")
    except Exception as e:
      import traceback; traceback.print_exc()
      print("hmvalidate: capture/render error: %s" % e, flush=True)
      self._finish(EXIT_LOAD_ERROR, "ERROR")


def _content_check(capbuf):
  """True if the captured RGBA8 frame shows actual geometry: >=NB_MIN of sampled pixels differ from the
  corner-estimated background AND luminance spread >= SPREAD_MIN (rejects a flat single-color fill)."""
  w, h = capbuf.width, capbuf.height
  data = bytes(capbuf)                       # buffer protocol -> RGBA8, len == w*h*4
  if w <= 0 or h <= 0 or len(data) < w * h * 4:
    print("hmvalidate: capture buffer empty (w=%d h=%d len=%d)" % (w, h, len(data)), flush=True)
    return False
  def px(x, y):
    o = (y * w + x) * 4
    return data[o], data[o + 1], data[o + 2]
  cor = [px(0, 0), px(w - 1, 0), px(0, h - 1), px(w - 1, h - 1)]   # background = the 4 corners
  bg  = (sum(c[0] for c in cor) // 4, sum(c[1] for c in cor) // 4, sum(c[2] for c in cor) // 4)
  sx, sy = max(1, w // 32), max(1, h // 32)
  n = nb = 0
  lmin, lmax = 1e9, -1e9
  y = 0
  while y < h:
    x = 0
    while x < w:
      r, g, b = px(x, y)
      n += 1
      if max(abs(r - bg[0]), abs(g - bg[1]), abs(b - bg[2])) > 12:
        nb += 1
      lum  = 0.299 * r + 0.587 * g + 0.114 * b
      lmin = min(lmin, lum); lmax = max(lmax, lum)
      x += sx
    y += sy
  nb_frac = nb / max(1, n)
  spread  = (lmax - lmin) / 255.0
  return (nb_frac >= NB_MIN) and (spread >= SPREAD_MIN)


def main():
  import argparse
  ap = argparse.ArgumentParser(description="offscreen hypermesh asset validator")
  ap.add_argument("asset", nargs="?", help="asset filename stem (search mode)")
  ap.add_argument("-i", "--input", default=None, help="explicit path to an asset .py (overrides search)")
  ap.add_argument("-o", "--out", default=None,
                  help="dump output, routed by extension: <name>.obj -> the materialized MESH as OBJ "
                       "(works with ANY hypermesh asset); <name>.png -> the captured frame (render diagnostic)")
  ap.add_argument("--bless", action="store_true",
                  help="freeze the CURRENT (visually approved) mesh as the golden baseline next to the asset "
                       "(<asset_dir>/.vet/<stem>.vetbase.npz); subsequent vet runs FAIL on drift until re-blessed")
  ap.add_argument("--no-vet", action="store_true", help="skip the meshvet geometric tier (escape hatch)")
  a = ap.parse_args()
  if not a.input and not a.asset:
    print("usage: _ork.hypermesh.validate.py <ASSET_NAME> | -i <path.py>  [-o <out.obj|out.png>] [--bless]", flush=True)
    return EXIT_LOAD_ERROR
  out = a.out
  obj_path = out if (out and out.lower().endswith(".obj")) else None      # -o *.obj  -> mesh OBJ dump
  png_path = out if (out and not obj_path) else None                      # -o *.png  -> frame capture
  app = ValidatorApp(a.asset, asset_path=a.input, png_path=png_path, obj_path=obj_path)
  app.ezapp.mainThreadLoop()    # runs until _finish() -> signalExit()
  app.ezapp.shutdown()
  rc = app._result
  # -o *.obj: run the dumped mesh through the MESHVET geometric tier (structural + crossing self-
  # intersection + buried/flip parity + collapse/sliver + fan-fold + VET expectations + golden baseline).
  # A FAIL must BLOCK the watch live-reload: the parent prefers the LAST result token in stdout, so we
  # OVERRIDE the earlier render-tier PASS with VETFAIL (crash-after-render still fails open, as designed).
  if obj_path and rc == EXIT_OK and app._obj_written and not a.no_vet:
    from ork.hypergraph.dflow.hypermesh import meshvet
    stem = (os.path.splitext(os.path.basename(app._asset_file))[0]
            if getattr(app, "_asset_file", None) else (a.asset or "asset"))
    expectations = dict(getattr(getattr(app, "_asset_cls", None), "VET", None) or {})
    verdict, report = meshvet.run(obj_path,
                                  asset_py=getattr(app, "_asset_file", None), stem=stem,
                                  expectations=expectations, bless=a.bless)
    print(report, flush=True)
    if verdict == meshvet.FAIL:
      print("\033[1;31mHMVALIDATE_GEOMETRY=FAIL\033[0m — the mesh is not well-formed (see above).", flush=True)
      print(RESULT_TOKEN + "VETFAIL", flush=True)        # LAST token wins in the parent -> blocks live-reload
      return EXIT_VETFAIL
    print("HMVALIDATE_GEOMETRY=%s" % verdict, flush=True)
  return rc


if __name__ == "__main__":
  sys.exit(main())
