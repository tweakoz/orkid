#!/usr/bin/env ork.python
################################################################################
# SKYLIGHT lane A gate: dynamic directional sun + cascaded shadow maps.
#
# Mono offscreen scene: a lit ground plane (grid _V4 — runs the full forward
# lighting path) + an occluder model + ONE shadow-casting DynamicDirectionalLight
# angled so the occluder's shadow falls on visible ground. Observables:
#   (a) capture is nonblack / non-flat,
#   (b) a fixed shadowed-region crop is SIGNIFICANTLY darker than a fixed
#       lit-region crop (mean ratio),
#   (c) both at shadowCascadeCount=3 AND =2 (two captures from one process —
#       the count switch is applied live between captures, proving the
#       runtime 3→2 lever).
# Machine verdict line via the ork.testing protocol, emitted BEFORE teardown
# (known pre-existing defect D1: this scene class can SIGSEGV during
# mainThreadLoop teardown AFTER the verdict is flushed — verdict-first).
#
# Lifecycle: ComponentizedApplication + StandardSceneGraphComponent — the
# proven offscreen scenegraph-capture pattern (same hand-rolled-lifecycle
# reason as test_fwd_prologue_gate.py: capture_app cannot yet host a
# scenegraph composite offscreen).
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
    os.environ.get("TMPDIR", "/tmp"), "sun_cascades_gate")
SETTLE_FRAMES = 120   # model/skybox residency + cascade warm-up
SWITCH_FRAMES = 40    # settle window after the live 3->2 cascade switch

# Fixed crop rects (x0,y0,x1,y1) in the 640x480 frame — measured from the
# rendered composition (camera (5,8,10)->(0,2,0), sun from (30,50,20)): the
# occluder's shadow lands on the ground up-left of the model (crop mean ~83);
# the lit reference is open ground right of it at similar view depth (~204).
CROP_SHADOW = (250, 170, 305, 205)
CROP_LIT    = (430, 170, 485, 205)
RATIO_MIN   = 1.5   # lit/shadow mean ratio proving a real sun shadow (~2.4 measured)


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
    self._phase = 0        # 0: settle@3, 1: cap@3, 2: settle@2, 3: cap@2, 4: verdict
    self._phase_frame = 0
    self._done = False
    self._caps = {}        # cascade_count -> capture path
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

    # occluder — on fwd_layers (std_forward + depth_prepass): a drawable
    # sun-shadows only if it plays the depth_prepass role.
    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.drawable_model = model.createDrawable()
    self.modelnode = SGC.scenegraph.createDrawableNodeOnLayers(
        SGC.fwd_layers, "occluder", self.drawable_model)
    self.modelnode.worldTransform.translation = vec3(0, 2, 0)

    # THE sun — one shadow-casting directional light, fixed angle.
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 4.0
    sun.data.shadowBias = 0.05  # metres
    sun.data.shadowMapSize = 2048
    sun.data.shadowCascadeCount = 3
    sun.data.shadowMaxDistance = 250.0
    sun.data.pcfDither = 1.0
    sun.shadowCaster = True
    sun.lookAt(vec3(30, 50, 20), vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    lmgr = SGC.scenegraph.lightingmanager
    lmgr.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass  # fully static scene — deterministic captures

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _capture(self, ctx, count):
    path = os.path.join(self._outdir, "sun_cascades_%d.png" % count)
    ctx.FBI.captureToFile(self._rtg(ctx).buffer(0), CorePath(path))
    self._caps[count] = path

  def _verdict(self):
    import numpy
    from PIL import Image
    from ork.testing import verdict
    results = []
    ok = True
    for count in (3, 2):
      path = self._caps.get(count)
      if not (path and os.path.isfile(path) and os.path.getsize(path) > 0):
        results.append("count=%d MISSING" % count)
        ok = False
        continue
      arr = numpy.asarray(Image.open(path).convert("RGB"), dtype=numpy.float32)
      mx = int(arr.max())
      m_shadow = _crop_mean(arr, CROP_SHADOW)
      m_lit = _crop_mean(arr, CROP_LIT)
      ratio = m_lit / max(m_shadow, 1e-3)
      this_ok = (mx > 0) and (ratio >= RATIO_MIN)
      ok = ok and this_ok
      results.append("count=%d max=%d lit=%.1f shadow=%.1f ratio=%.2f %s"
                     % (count, mx, m_lit, m_shadow, ratio, path))
    self._verdict_code = verdict(ok, "sun cascade shadow gate | " + " | ".join(results))

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._built_frame = self._frame
        self._phase_frame = self._frame
      return
    if self._done:
      return
    if self._phase == 0:
      if self._frame >= self._phase_frame + SETTLE_FRAMES:
        self._capture(ctx, 3)
        self._phase = 1
        self._phase_frame = self._frame
    elif self._phase == 1:
      if self._frame >= self._phase_frame + 20:  # let the async write land
        self.sun.data.shadowCascadeCount = 2     # LIVE cascade-count switch
        self._phase = 2
        self._phase_frame = self._frame
    elif self._phase == 2:
      if self._frame >= self._phase_frame + SWITCH_FRAMES:
        self._capture(ctx, 2)
        self._phase = 3
        self._phase_frame = self._frame
    elif self._phase == 3:
      if self._frame >= self._phase_frame + 20:
        # verdict evidence computed + FLUSHED BEFORE teardown (D1 protocol)
        self._verdict()
        self._done = True
        self.ezapp.signalExit()


def main():
  from ork.testing import ensure_parent_dir, Watchdog
  os.makedirs(OUTDIR, exist_ok=True)
  wd = Watchdog(300.0, label="sun_cascades_gate").arm()
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
