#!/usr/bin/env ork.python
################################################################################
# Gate — PER-BAND SHADOW RESOLUTION renders the same shadow in the same place.
#
# W7-S2b keeps ONE cascade array at the near band's dim and renders band i into
# the top-left (dim/ratio^i)² corner of its slice, then scales that band's uv —
# and its world texel size — to match. The scaling is the whole risk: get it
# wrong and the band samples the wrong part of its own slice, which moves its
# shadow by up to half a viewport or drops it into the cleared region (= no
# shadow at all) — in the OUTER bands only, so the near-field of a capture still
# looks perfect and nothing errors.
#
# Two captures from ONE process, the only difference being the knob:
#   (a) STEPPED — band res ratio 2 (2048/1024/512/256 across four bands),
#   (b) UNIFORM — the same rig at ratio 1 (every band 2048), switched LIVE
#       (a per-band dim change is a rig change: it refits on the spot).
# The band geometry is deliberately TIGHT (3.5m, ratio 2 -> 3.5/7/14/28m about
# the viewer) so that a band boundary crosses the occluder's shadow on screen
# instead of sitting past the far end of the scene, which is what makes a
# per-band error visible at all. Asserts:
#   1. both captures show a real sun shadow (lit/shadow mean ratio);
#   2. the shadow is the SAME SHADOW: its mask's centroid and area agree. A
#      mis-scaled band shifts or loses its share of the mask, and neither
#      survives this even though a resolution step legitimately softens edges;
#   3. no LOCAL blow-up: the largest 16x16 block-mean difference between the two
#      images is bounded. A band that disagrees about where its shadow goes
#      concentrates its error in one place, so the worst block finds it —
#      self-locating, unlike a hand-placed crop at a boundary whose screen
#      position moves with the camera.
# The stepped bands genuinely SOFTEN (band 3 draws at 256² = 22cm texels here,
# and that softening is visible in both the whole-image mean difference and the
# worst block — see the threshold note). That is the knob's cost, not a defect,
# which is why the placement asserts, not the difference magnitude, are what
# proves the uv scaling: a shadow drawn at the wrong place cannot keep its
# centroid, however it is filtered.
#
# Machine verdict line via the ork.testing protocol, emitted BEFORE teardown
# (known pre-existing defect D1: this scene class can SIGSEGV during
# mainThreadLoop teardown AFTER the verdict is flushed — verdict-first).
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
    os.environ.get("TMPDIR", "/tmp"), "sun_band_resolution_gate")
SETTLE_FRAMES = 120   # model/skybox residency
SWITCH_FRAMES = 60    # settle window after the live knob switch

MAP_SIZE       = 2048
CASCADES       = 4
BAND_RADIUS    = 3.5   # tight bands: 3.5/7/14/28m about the viewer, so the
BAND_RATIO     = 2.0   #  band-2/3 boundary crosses the occluder's shadow
BAND_RES_RATIO = 2.0   # the slice under test: 2048/1024/512/256

SHADOW_MASK_MAX = 50.0  # brightness below which a ground pixel counts as shadowed
RATIO_MIN       = 1.5   # lit/shadow mean ratio proving a real sun shadow
CENTROID_MAX_PX = 3.0   # how far the shadow's centre of mass may move
AREA_REL_MAX    = 0.25  # ...and how much its area may change (softening, not shifting)
BLOCK_DIFF_MAX  = 45.0  # worst 16x16 block mean abs difference. Measured 28.8 on the
                        #  correct implementation — that block is the SOFTENING of the
                        #  stepped outer band (22cm texels vs 5.5cm), which is the
                        #  feature working; a band sampling the wrong part of its slice
                        #  puts shadow where light is and blows past 100.
BLOCK           = 16

# same fixed crops as test_sun_cascades_gate.py — same camera, same occluder
CROP_SHADOW = (250, 170, 305, 205)
CROP_LIT    = (430, 170, 485, 205)


def _crop_mean(arr, rect):
  x0, y0, x1, y1 = rect
  return float(arr[y0:y1, x0:x1].mean())


class GateApp(ComponentizedApplication):

  def __init__(self, outdir):
    super().__init__()
    self._outdir = outdir
    self._frame = 0
    self._built = False
    self._phase = 0        # 0: settle stepped, 1: cap, 2: settle uniform, 3: cap, 4: verdict
    self._phase_frame = 0
    self._done = False
    self._caps = {}        # tag -> capture path
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

    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.drawable_model = model.createDrawable()
    self.modelnode = SGC.scenegraph.createDrawableNodeOnLayers(
        SGC.fwd_layers, "occluder", self.drawable_model)
    self.modelnode.worldTransform.translation = vec3(0, 2, 0)

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 4.0
    sun.data.shadowBias = 2e-4
    sun.data.shadowMapSize = MAP_SIZE
    sun.data.shadowCascadeCount = CASCADES
    sun.data.shadowMaxDistance = 250.0
    sun.data.pcfDither = 1.0
    sun.data.shadowBandRadius = BAND_RADIUS
    sun.data.shadowBandRatio  = BAND_RATIO
    # the slice under test
    sun.data.shadowBandResRatio = BAND_RES_RATIO
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

  def _capture(self, ctx, tag):
    path = os.path.join(self._outdir, "sun_band_res_%s.png" % tag)
    ctx.FBI.captureToFile(self._rtg(ctx).buffer(0), CorePath(path))
    self._caps[tag] = path

  def _verdict(self):
    import numpy
    from PIL import Image
    from ork.testing import verdict
    results = []
    ok = True
    arrays = {}
    for tag in ("stepped", "uniform"):
      path = self._caps.get(tag)
      if not (path and os.path.isfile(path) and os.path.getsize(path) > 0):
        results.append("%s MISSING" % tag)
        ok = False
        continue
      arr = numpy.asarray(Image.open(path).convert("RGB"), dtype=numpy.float32)
      arrays[tag] = arr
      m_shadow = _crop_mean(arr, CROP_SHADOW)
      m_lit = _crop_mean(arr, CROP_LIT)
      ratio = m_lit / max(m_shadow, 1e-3)
      this_ok = (int(arr.max()) > 0) and (ratio >= RATIO_MIN)
      ok = ok and this_ok
      results.append("%s lit=%.1f shadow=%.1f ratio=%.2f %s"
                     % (tag, m_lit, m_shadow, ratio, path))
    if len(arrays) == 2:
      grays = {t: a.mean(axis=2) for t, a in arrays.items()}
      # 2 — same shadow, same place
      cents, areas = {}, {}
      for tag, g in grays.items():
        mask = g <= SHADOW_MASK_MAX
        n = int(mask.sum())
        areas[tag] = n
        ys, xs = numpy.nonzero(mask)
        cents[tag] = (float(xs.mean()), float(ys.mean())) if n else (0.0, 0.0)
      dc = ((cents["stepped"][0] - cents["uniform"][0]) ** 2
            + (cents["stepped"][1] - cents["uniform"][1]) ** 2) ** 0.5
      da = abs(areas["stepped"] - areas["uniform"]) / max(areas["uniform"], 1)
      place_ok = (areas["uniform"] > 500) and (dc <= CENTROID_MAX_PX) and (da <= AREA_REL_MAX)
      ok = ok and place_ok
      results.append("mask area %d/%d rel=%.3f centroid delta=%.2fpx placed:%s"
                     % (areas["stepped"], areas["uniform"], da, dc, place_ok))
      # 3 — no seam: worst block, and the whole-image mean for context
      diff = numpy.abs(grays["stepped"] - grays["uniform"])
      h, w = diff.shape
      bh, bw = h // BLOCK, w // BLOCK
      blocks = diff[:bh * BLOCK, :bw * BLOCK].reshape(bh, BLOCK, bw, BLOCK).mean(axis=(1, 3))
      worst = float(blocks.max())
      by, bx = numpy.unravel_index(int(blocks.argmax()), blocks.shape)
      seam_ok = worst <= BLOCK_DIFF_MAX
      ok = ok and seam_ok
      results.append("mean diff=%.3f worst %dx%d block=%.1f at (%d,%d) bounded:%s"
                     % (float(diff.mean()), BLOCK, BLOCK, worst,
                        int(bx * BLOCK), int(by * BLOCK), seam_ok))
    self._verdict_code = verdict(ok, "sun band resolution gate | " + " | ".join(results))

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_frame = self._frame
      return
    if self._done:
      return
    if self._phase == 0:
      if self._frame >= self._phase_frame + SETTLE_FRAMES:
        self._capture(ctx, "stepped")
        self._phase = 1
        self._phase_frame = self._frame
    elif self._phase == 1:
      if self._frame >= self._phase_frame + 20:  # let the async write land
        self.sun.data.shadowBandResRatio = 1.0   # LIVE: every band back to the full dim
        self._phase = 2
        self._phase_frame = self._frame
    elif self._phase == 2:
      if self._frame >= self._phase_frame + SWITCH_FRAMES:
        self._capture(ctx, "uniform")
        self._phase = 3
        self._phase_frame = self._frame
    elif self._phase == 3:
      if self._frame >= self._phase_frame + 20:
        # verdict evidence computed + FLUSHED BEFORE teardown (D1 protocol)
        self._verdict()
        self._done = True
        self.ezapp.signalExit()


def main():
  from ork.testing import Watchdog
  os.makedirs(OUTDIR, exist_ok=True)
  wd = Watchdog(300.0, label="sun_band_resolution_gate").arm()
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
