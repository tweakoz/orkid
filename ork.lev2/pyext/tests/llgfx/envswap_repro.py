#!/usr/bin/env ork.python
################################################################################
# envswap_repro.py — offscreen reproduction of the interactive 'e' envmap switch
# (ork.hypermesh.viewer.py). Waits for the INITIAL skybox to become resident
# (mirrors the user looking at a lit scene), captures a BEFORE frame, performs
# the EXACT switch the 'e' handler does (requestRadianceMapsAsync +
# pbr_common.RadianceMaps = sky), then polls (with a generous timeout) for the
# NEW filtered specular to become resident, capturing an AFTER frame. Prints the
# live radiance-map pointer + specular-residency at each stage so the swap can be
# trace-proven non-visually.
################################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time
from orkengine.core import vec3, CrcStringProxy, Path as CorePath
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

DIM         = 512
SWITCH_TO   = os.environ.get("ENVSWAP_TO", "<ork_envmaps2>/arena4k.xir")
INIT_TMO_S  = float(os.environ.get("ENVSWAP_INIT_TMO",  "30"))
SWAP_TMO_S  = float(os.environ.get("ENVSWAP_SWAP_TMO",  "30"))
BEFORE_PNG  = os.environ.get("ENVSWAP_BEFORE", "/tmp/envswap_before.png")
AFTER_PNG   = os.environ.get("ENVSWAP_AFTER",  "/tmp/envswap_after.png")


def _specular(pbc):
  try:
    return pbc.RadianceMaps.specular
  except Exception:
    return None


class ReproApp(ComponentizedApplication):
  def __init__(self):
    super().__init__()
    self._frame = 0
    self._phase = "wait_init"     # wait_init -> switched -> wait_swap -> done
    self._t0 = 0.0
    self._before_ptr = None
    self._after_ptr = None
    self.SGC = self.addComponent("std_scenegraph", StandardSceneGraphComponent,
                                 eye=vec3(0, 1, 4), tgt=vec3(0, 1, 0), up=vec3(0, 1, 0),
                                 grid_variant=None)
    self.createEzApp(enable_lockstep_ups=True, enable_lockstep_fps=True,
                     enable_freerun_ups=True, enable_freerun_fps=True,
                     freerun=False, target_ups=60, target_fps=60,
                     width=DIM, height=DIM,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  def _onGpuInit(self, ctx):
    self._ctx = ctx
    self.ezapp.topWidget.enableUiDraw()
    self.SGC.pbr_common.enable_skybox = True
    self._t0 = time.time()
    print("[ENVSWAP] gpuinit done; skybox enabled; waiting for initial residency", flush=True)

  def _onUpdate(self, updinfo):
    self.SGC.scenegraph.updateScene(self.SGC.cameralut)

  def _capture(self, ctx, path):
    rtg = getattr(self.SGC.SGVPW, "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      print("[ENVSWAP] viewport RTG not ready for capture", flush=True)
      return
    ctx.FBI.captureToFile(rtg.buffer(0), CorePath(path))
    print("[ENVSWAP] captured -> %s" % path, flush=True)

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    pbc = self.SGC.pbr_common
    now = time.time()

    if self._phase == "wait_init":
      if _specular(pbc) is not None:
        self._before_ptr = repr(pbc.RadianceMaps)
        print("[ENVSWAP] INITIAL resident at frame %d (t=%.1fs) rm=%s" %
              (self._frame, now - self._t0, self._before_ptr), flush=True)
        self._capture(ctx, BEFORE_PNG)
        # --- the exact 'e'-handler switch ---
        print("[ENVSWAP] === SWITCH to %s ===" % SWITCH_TO, flush=True)
        sky = lev2.PbrCommon.requestRadianceMapsAsync(SWITCH_TO)
        self.SGC.pbr_common.RadianceMaps = sky
        self._after_ptr = repr(pbc.RadianceMaps)
        print("[ENVSWAP] post-switch(t=0) rm=%s specular=%s" %
              (self._after_ptr, "RESIDENT" if _specular(pbc) is not None else "None"), flush=True)
        self._phase = "wait_swap"
        self._t0 = now
      elif now - self._t0 > INIT_TMO_S:
        print("[ENVSWAP] FAIL: initial skybox never became resident in %.0fs" % INIT_TMO_S, flush=True)
        self._phase = "done"; self.ezapp.signalExit()
      return

    if self._phase == "wait_swap":
      if _specular(pbc) is not None:
        print("[ENVSWAP] SWAP COMPLETE at frame %d (t=%.1fs after switch) rm=%s specular=RESIDENT" %
              (self._frame, now - self._t0, repr(pbc.RadianceMaps)), flush=True)
        self._capture(ctx, AFTER_PNG)
        print("[ENVSWAP] before_ptr=%s after_ptr=%s changed=%s" %
              (self._before_ptr, self._after_ptr, self._before_ptr != self._after_ptr), flush=True)
        self._phase = "done"; self.ezapp.signalExit()
      elif now - self._t0 > SWAP_TMO_S:
        print("[ENVSWAP] FAIL: SWITCH specular NEVER became resident in %.0fs (t after switch) rm=%s" %
              (SWAP_TMO_S, repr(pbc.RadianceMaps)), flush=True)
        self._capture(ctx, AFTER_PNG)
        self._phase = "done"; self.ezapp.signalExit()
      return


def main():
  app = ReproApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  return 0


if __name__ == "__main__":
  sys.exit(main())
