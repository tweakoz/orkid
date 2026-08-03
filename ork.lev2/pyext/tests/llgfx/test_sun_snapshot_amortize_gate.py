#!/usr/bin/env ork.python
################################################################################
# Gate — an AMORTIZED, crossfaded sun-cascade snapshot renders the SAME shadow
# the shipped single-frame snapshot does.
#
# The snapshot machinery (W7-S2a) draws one cascade band per frame into the set
# the shader is NOT sampling and publishes the whole set at once, then blends the
# outgoing snapshot's shadow factor out over a fade window. Every step of that
# has a silent failure mode: a wrong slice base samples an undrawn half of the
# depth array (no shadow at all), a publish that lands before the last band
# draws samples empty depth, and a fade weight that never decays leaves the
# scene permanently shaded by a snapshot that is no longer live. NONE of them
# error — they just render the wrong shadow, which is exactly what an image
# assertion catches and a build does not.
#
# Same scene, same crops and same lit/shadow ratio law as
# test_sun_cascades_gate.py (a static occluder over lit ground, one shadow
# casting directional light). Two captures from ONE process:
#   (a) ARMED   — 1 band per frame + a 12-frame flip crossfade (snapshots,
#       flips and fades cycling continuously through the settle window);
#   (b) SHIPPED — the same light with all three knobs back at 0 (single
#       buffered, whole snapshot in one frame, hard swap), switched LIVE.
# Asserts:
#   1. both captures show a real sun shadow (lit/shadow mean ratio),
#   2. they AGREE — the amortized path is a scheduling change, not a look
#      change, so on a static scene the two images must be the same image.
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
    os.environ.get("TMPDIR", "/tmp"), "sun_snapshot_amortize_gate")
# SETTLE (raised with the cadence fix, 2026-07-30): the flip crossfade now runs
# to its declared END instead of being retired by the next cadence start, so the
# FIRST snapshot — taken on frame 0, before the model is resident, i.e. an empty
# depth set — survives as the fade's PREV half for a whole window. A short settle
# then captures a blend of a real shadow with an empty one and reads a washed-out
# shadow core. The settle has to outlast several complete snapshot+fade cycles.
SETTLE_FRAMES = 1500  # model/skybox residency + several snapshot+fade cycles

# READBACK WARM-UP. The FIRST captureToFile calls of a process do not hand back
# the frame they were asked for — they hand back content from far earlier in the
# run and converge over the next few captures. Measured on the sibling jitter
# gate (identical scene and capture path): first capture of a run, at frame 1502,
# shadow crop 128.0; second, at 1512, 95.6; third, at 1522, the settled 87.5 —
# while a run that had already captured ten times read the settled value AT FRAME
# 1500. The frame index is not what differs; the capture index is. So an A/B
# whose first capture is stale compares a readback transient against a settled
# frame and calls the difference its knob. That gate reported the SAME numbers
# with its knob forced off in both captures, i.e. no signal at all; here the
# armed/shipped pair carried the same signature (armed shadow crop 115.6 vs
# shipped 84.7 — the transient's shape, not amortization's). Discarded captures
# ahead of each measured one flush it.
# NOT diagnosed below the pyext boundary — the counts are empirical, and only
# their sufficiency was checked, not their minimality.
WARMUP_CAPS    = 4
WARMUP_SPACING = 12     # frames between them — the path needs frames, not just calls
WARMUP_LEAD    = (WARMUP_CAPS + 1) * WARMUP_SPACING

# settle window after the live knobs-off switch. The warm-up is ADDED to the
# original 60 rather than taken out of it: the shipped path must still get its
# full settle before anything that gets measured is captured.
SWITCH_FRAMES = 60 + WARMUP_LEAD

# ARMED knob set. NO wall-clock interval on purpose: with one, an offscreen loop
# running at several hundred fps takes its snapshot on the FIRST frames — before
# the model is resident — and then holds it for the whole settle window, so the
# capture lands on an empty-scene snapshot (or, worse, mid-fade between the empty
# one and the first real one) and the gate reads a shadow that is missing or half
# strength. That is the hold working as designed, not a defect, and it is not
# what this gate is measuring. With the interval at 0 the amortization itself
# sets the cadence — a new snapshot starts as soon as the last one lands — so
# snapshots, flips and fade windows cycle continuously and the capture sees a
# fully settled, freshly published one.
SNAP_INTERVAL    = 0.0
BANDS_PER_FRAME  = 1   # a 4-band snapshot spread over 4 frames
CROSSFADE_FRAMES = 12
CASCADES         = 4

# Same fixed crops as test_sun_cascades_gate.py — same camera, same occluder.
CROP_SHADOW = (250, 170, 305, 205)
CROP_LIT    = (430, 170, 485, 205)
RATIO_MIN   = 1.5   # lit/shadow mean ratio proving a real sun shadow
DIFF_MAX    = 1.0   # mean abs 0-255 difference between the two captures


def _crop_mean(arr, rect):
  x0, y0, x1, y1 = rect
  return float(arr[y0:y1, x0:x1].mean())


class GateApp(ComponentizedApplication):

  def __init__(self, outdir):
    super().__init__()
    self._outdir = outdir
    self._frame = 0
    self._built = False
    self._phase = 0        # 0: settle armed, 1: cap armed, 2: settle shipped, 3: cap shipped, 4: verdict
    self._phase_frame = 0
    self._done = False
    self._warm = 0         # discarded captures taken so far in THIS settle
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
    sun.data.shadowMapSize = 2048
    sun.data.shadowCascadeCount = CASCADES
    sun.data.shadowMaxDistance = 250.0
    sun.data.pcfDither = 1.0
    # the slice under test: amortized snapshot + flip crossfade
    sun.data.shadowSnapshotInterval        = SNAP_INTERVAL
    sun.data.shadowSnapshotBandsPerFrame   = BANDS_PER_FRAME
    sun.data.shadowCrossfadeFrames         = CROSSFADE_FRAMES
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
    path = os.path.join(self._outdir, "sun_snapshot_%s.png" % tag)
    ctx.FBI.captureToFile(self._rtg(ctx).buffer(0), CorePath(path))
    self._caps[tag] = path

  def _warming(self, ctx, target):
    """Discarded captures flushing the readback path ahead of the MEASURED one at
    frame `target` — see WARMUP_CAPS. True while the warm-up still owes captures,
    so the caller must not advance its phase. One file, rewritten each time."""
    if self._frame < target - WARMUP_LEAD or self._warm >= WARMUP_CAPS:
      return False
    if (self._frame % WARMUP_SPACING) == 0:
      self._capture(ctx, "warmup")
      self._warm += 1
    return True

  def _verdict(self):
    import numpy
    from PIL import Image
    from ork.testing import verdict
    results = []
    ok = True
    arrays = {}
    for tag in ("armed", "shipped"):
      path = self._caps.get(tag)
      if not (path and os.path.isfile(path) and os.path.getsize(path) > 0):
        results.append("%s MISSING" % tag)
        ok = False
        continue
      arr = numpy.asarray(Image.open(path).convert("RGB"), dtype=numpy.float32)
      arrays[tag] = arr
      mx = int(arr.max())
      m_shadow = _crop_mean(arr, CROP_SHADOW)
      m_lit = _crop_mean(arr, CROP_LIT)
      ratio = m_lit / max(m_shadow, 1e-3)
      this_ok = (mx > 0) and (ratio >= RATIO_MIN)
      ok = ok and this_ok
      results.append("%s max=%d lit=%.1f shadow=%.1f ratio=%.2f %s"
                     % (tag, mx, m_lit, m_shadow, ratio, path))
    if len(arrays) == 2:
      diff = float(numpy.abs(arrays["armed"] - arrays["shipped"]).mean())
      same = diff <= DIFF_MAX
      ok = ok and same
      results.append("armed-vs-shipped mean abs diff=%.3f (max %.1f)" % (diff, DIFF_MAX))
    self._verdict_code = verdict(ok, "sun snapshot amortize gate | " + " | ".join(results))

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
      if self._warming(ctx, self._phase_frame + SETTLE_FRAMES):
        return
      if self._frame >= self._phase_frame + SETTLE_FRAMES:
        self._capture(ctx, "armed")
        self._phase = 1
        self._phase_frame = self._frame
    elif self._phase == 1:
      if self._frame >= self._phase_frame + 20:  # let the async write land
        # LIVE disarm: back to the shipped single-buffered, single-frame,
        # hard-swap path (a knob change is a rig change — it refits at once).
        self.sun.data.shadowSnapshotBandsPerFrame = 0
        self.sun.data.shadowCrossfadeFrames       = 0
        self._warm = 0     # the second capture gets its own warm-up
        self._phase = 2
        self._phase_frame = self._frame
    elif self._phase == 2:
      if self._warming(ctx, self._phase_frame + SWITCH_FRAMES):
        return
      if self._frame >= self._phase_frame + SWITCH_FRAMES:
        self._capture(ctx, "shipped")
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
  wd = Watchdog(300.0, label="sun_snapshot_amortize_gate").arm()
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
