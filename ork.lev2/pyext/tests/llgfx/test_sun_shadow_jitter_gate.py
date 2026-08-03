#!/usr/bin/env ork.python
################################################################################
# Gate — PER-SNAPSHOT LIGHT-SPACE JITTER supersamples the shadow edge, and
# touches nothing else.
#
# W7-S2b offsets each snapshot's (already texel-snapped) ortho origin by a
# sub-texel amount from an R2 sequence, so two snapshots blended by the flip
# crossfade are two different sub-texel samplings of the same penumbra. The
# claim is narrow and both halves of it can fail silently:
#   * if the offset never reaches the fit, the feature is a no-op that still
#     serializes and reads as armed;
#   * if it reaches more than the fit — texel SIZE, the refresh gate, the band
#     SELECT radii — it is the shimmer/crawl class this design exists to avoid,
#     and the symptom is motion, which a still capture cannot see. What a still
#     capture CAN see is the invariant that motion would break: everything that
#     is not a penumbra must be identical.
#
# Two captures from ONE process, the ONLY difference being the jitter knob
# (amortization and crossfade stay armed in both, so the snapshot cadence, the
# fade window and the frame counts are the same schedule):
#   (a) JITTERED — 1.0 texel amplitude (the maximum the engine accepts),
#   (b) PLAIN    — the same rig with jitter back at 0, switched LIVE.
# Pixels are classified from the PLAIN capture — no hand-placed crops to drift
# when the rig changes — and asserted:
#   1. both captures show a real sun shadow (lit/shadow mean ratio),
#   2. the PENUMBRA (the transition band) DIFFERS: the two snapshots being
#      blended were fit half a texel apart, so the edge lands elsewhere,
#   3. the fully-lit and fully-shadowed INTERIORS are stable: jitter moves the
#      sampling of an edge, never the shadow itself.
#
# The INTERIORS ARE GEOMETRIC, NOT AN INTENSITY CUT (2026-07-30). The ambient/
# skylight work abolished absolute-black shadows by design, so "core = dark
# pixels" first went empty and then, recalibrated to a relative cut, went WRONG:
# a skylight-lit shadow is all gradient, so every intensity cut admits penumbra
# pixels into the region that is supposed to contain none, and the "core" read
# the edge signal back (0.27 at a 0.05 bar). The core is instead the shadow mask
# ERODED by more than anything in the sampling chain can move an edge — interior
# BY CONSTRUCTION, whatever the floor does to absolute levels. See CORE_ERODE_PX.
#
# A 512² map on purpose: at 2048² a half-texel of band 0 is ~5mm of world and
# lands well inside one screen pixel, so the effect this gate measures would be
# real and invisible. The knob is not resolution-specific; the MEASUREMENT is.
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
    os.environ.get("TMPDIR", "/tmp"), "sun_shadow_jitter_gate")
# SETTLE (raised with the cadence fix, 2026-07-30): the flip crossfade now runs
# to its declared END instead of being retired by the next cadence start, so the
# FIRST snapshot — taken on frame 0, before the model is resident, i.e. an empty
# depth set — survives as the fade's PREV half for a whole window. A short settle
# then captures a blend of a real shadow with an empty one and reads a washed-out
# shadow core. The settle has to outlast several complete snapshot+fade cycles.
SETTLE_FRAMES = 1500  # model/skybox residency + several snapshot+fade cycles
# ...and the window after the live disarm has to outlast a WHOLE cycle, snapshot
# AND fade: with the fade now running to its declared end (0.2 s by default, ~100
# frames in an unthrottled loop) a 60-frame wait captured "plain" while the
# jittered snapshot was still the live one, and the two captures were the same
# image by construction.
SWITCH_FRAMES = 500   # a full snapshot+fade cycle after the live knob switch

MAP_SIZE         = 512   # see the note above — the texel must be visible on screen
CASCADES         = 4
SNAP_INTERVAL    = 0.0   # let the amortization set the cadence (same reason as the amortize gate:
                         #  a wall-clock hold would freeze the pre-residency snapshot)
BANDS_PER_FRAME  = 1
CROSSFADE_FRAMES = 12
JITTER_TEXELS    = 1.0   # the engine clamps here; the offset is +-half of it

# brightness classification of the PLAIN capture (0-255 mean of RGB)
PENUMBRA_LO     = 15.0   # the transition band, minus whatever the erosion below
PENUMBRA_HI     = 90.0   #  claims as interior (the three regions stay disjoint)

# SHADOW MASK: the PLAIN capture split at the MIDPOINT of its own shadow/lit crop
# means. Relative on purpose — the skylight floor sets absolute levels and moves
# with unrelated lighting work; the midpoint moves with it. Bleed at the mask
# boundary is harmless: nothing within CORE_ERODE_PX of it survives.
#
# CORE_ERODE_PX — the margin that turns "inside the mask" into "no penumbra can
# reach here". Budget of everything that can displace a shadow edge, in band-0
# shadow texels (this shadow is band 0; the outer bands are coarser but this
# scene has no caster in them):
#     jitter offset                     0.5   (JITTER_TEXELS 1.0, applied +-half)
#     PCF lattice, 1 step of radius_tex 1.0   (radius_tex ~1 here: pcfDither 1.0
#                                              dominates foot_tex ~0.8 and the
#                                              PCSS penumbra_tex ~0.6)
#     bilinear tap footprint            0.5
#     normal offset (1+radius_tex)*sin  2.0
#     PCSS blocker search, +-search/2   6.0   (search_tex 6..12: a jittered origin
#                                              can change the RADIUS DECISION this
#                                              far out, so it counts as reach)
#                                    = 10.0 texels
# band-0 texel = 2*10m / 512 = 39mm; the shadow sits ~14.8m out and the grid plane
# is oblique to the eye, so one texel spans at most ~1.5 screen px there
#   => 10 * 1.5 = 15 px, rounded UP to 16.
# MEASURED on this rig (armed vs unarmed, warmed captures): the diff is exactly
# 0.00 at every pixel >=14 px deep into the mask, and the mask itself only
# survives erosion to ~22 px — 16 leaves ~900 core pixels, well over the
# judgeability floor below. Under-erode and the core reads the edge (fails loud);
# over-erode and the region empties (fails loud). Both ends are guarded.
CORE_ERODE_PX   = 16

# READBACK WARM-UP. The FIRST captureToFile calls of a process do not hand back
# the frame they were asked for — they hand back content from far earlier in the
# run, and converge over the next few captures. Measured: first capture of a run,
# at frame 1502, shadow crop 128.0; second, at 1512, 95.6; third, at 1522, the
# settled 87.5 — while a run that had already captured ten times read the settled
# value AT FRAME 1500. The frame index is not what differs; the capture index is.
# An A/B whose first capture is stale therefore measures that transient and not
# the knob: with jitter forced OFF in BOTH captures this gate still reported
# penumbra 8.68 and core 16.20 (vs 8.89 / 16.91 armed — the whole "signal" was
# the artifact). Discarded captures ahead of each measured one flush it; the same
# forced-OFF control then comes back bit-identical (0.000 everywhere).
# NOT diagnosed below the pyext boundary — the counts here are empirical, and
# only their sufficiency was checked, not their minimality.
WARMUP_CAPS     = 4
WARMUP_SPACING  = 12     # frames between them — the path needs frames, not just calls
WARMUP_LEAD     = (WARMUP_CAPS + 1) * WARMUP_SPACING  # settle frames they consume

REGION_MIN_PX   = 200    # a region smaller than this asserts nothing

RATIO_MIN     = 1.5    # lit/shadow mean ratio proving a real sun shadow
EDGE_DIFF_MIN = 0.25   # mean abs 0-255 penumbra difference. With fades completing (ruling 9) successive captures hold a BLEND of two offsets - the jitter feature doing its temporal-supersampling job. Measured 0.373 on warmed captures; the 0.383 recorded at the interval-only merge came from an UNWARMED pair and was the readback transient, not the feature. A drop under 0.25 means jitter died.
CORE_DIFF_MAX = 0.05   # ...and the ceiling on both eroded interiors (measured: 0.000
                       #  and 0.000, max 0.00 — they come out bit-identical, and the
                       #  unarmed-vs-unarmed control floor is 0.000 as well)

# same fixed crops as test_sun_cascades_gate.py — same camera, same occluder
CROP_SHADOW = (250, 170, 305, 205)
CROP_LIT    = (430, 170, 485, 205)


def _crop_mean(arr, rect):
  x0, y0, x1, y1 = rect
  return float(arr[y0:y1, x0:x1].mean())


def _erode(numpy, ndi, mask, r):
  """morphological erosion by an (2r+1)² box — keeps only pixels whose whole
  neighbourhood is in the mask, which is what turns 'inside the mask' into
  'INTERIOR'. border_value=0: the frame edge is not evidence of interior."""
  return ndi.binary_erosion(mask, structure=numpy.ones((2 * r + 1, 2 * r + 1)),
                            border_value=0)


class GateApp(ComponentizedApplication):

  def __init__(self, outdir):
    super().__init__()
    self._outdir = outdir
    self._frame = 0
    self._built = False
    self._phase = 0        # 0: settle jittered, 1: cap, 2: settle plain, 3: cap, 4: verdict
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
    sun.data.shadowMapSize = MAP_SIZE
    sun.data.shadowCascadeCount = CASCADES
    sun.data.shadowMaxDistance = 250.0
    sun.data.pcfDither = 1.0
    sun.data.shadowSnapshotInterval      = SNAP_INTERVAL
    sun.data.shadowSnapshotBandsPerFrame = BANDS_PER_FRAME
    sun.data.shadowCrossfadeFrames       = CROSSFADE_FRAMES
    # the slice under test
    sun.data.shadowJitterTexels          = JITTER_TEXELS
    sun.shadowCaster = True
    sun.lookAt(vec3(30, 50, 20), vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    lmgr = SGC.scenegraph.lightingmanager
    lmgr.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass  # fully static scene — the jitter is the only thing that moves

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _capture(self, ctx, tag):
    path = os.path.join(self._outdir, "sun_jitter_%s.png" % tag)
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
    import scipy.ndimage as ndi
    from PIL import Image
    from ork.testing import verdict
    results = []
    ok = True
    arrays = {}
    crops = {}
    for tag in ("jittered", "plain"):
      path = self._caps.get(tag)
      if not (path and os.path.isfile(path) and os.path.getsize(path) > 0):
        results.append("%s MISSING" % tag)
        ok = False
        continue
      arr = numpy.asarray(Image.open(path).convert("RGB"), dtype=numpy.float32)
      arrays[tag] = arr
      m_shadow = _crop_mean(arr, CROP_SHADOW)
      m_lit = _crop_mean(arr, CROP_LIT)
      crops[tag] = (m_shadow, m_lit)
      ratio = m_lit / max(m_shadow, 1e-3)
      this_ok = (int(arr.max()) > 0) and (ratio >= RATIO_MIN)
      ok = ok and this_ok
      results.append("%s lit=%.1f shadow=%.1f ratio=%.2f %s"
                     % (tag, m_lit, m_shadow, ratio, path))
    if len(arrays) == 2:
      diff  = numpy.abs(arrays["jittered"] - arrays["plain"]).mean(axis=2)
      plain = arrays["plain"].mean(axis=2)
      # the UNARMED capture is the reference the regions are cut from — the armed
      # one is only ever the thing being compared against them.
      c_shadow, c_lit = crops["plain"]
      split = 0.5 * (c_shadow + c_lit)
      in_shadow = plain < split
      core_s = _erode(numpy, ndi, in_shadow, CORE_ERODE_PX)
      core_l = _erode(numpy, ndi, ~in_shadow, CORE_ERODE_PX)
      penumbra = ((plain > PENUMBRA_LO) & (plain < PENUMBRA_HI)) & ~core_s & ~core_l
      results.append("split=%.1f maskpx=%d" % (split, int(in_shadow.sum())))
      stats = {}
      for name, mask in (("penumbra", penumbra), ("shadow-core", core_s), ("lit-core", core_l)):
        n = int(mask.sum())
        stats[name] = (n, float(diff[mask].mean()) if n else 0.0,
                       float(diff[mask].max()) if n else 0.0)
        results.append("%s n=%d meandiff=%.3f maxdiff=%.2f" % ((name,) + stats[name]))
      # every class must actually EXIST, or the thresholds below assert nothing —
      # an erosion that ate its own region has to fail, not read 0 and pass
      for name in ("penumbra", "shadow-core", "lit-core"):
        if stats[name][0] < REGION_MIN_PX:
          results.append("%s region too small to judge" % name)
          ok = False
      edge_ok = stats["penumbra"][1] >= EDGE_DIFF_MIN
      core_ok = (stats["shadow-core"][1] <= CORE_DIFF_MAX
                 and stats["lit-core"][1] <= CORE_DIFF_MAX)
      ok = ok and edge_ok and core_ok
      results.append("penumbra differs>=%.2f:%s cores stable<=%.2f:%s"
                     % (EDGE_DIFF_MIN, edge_ok, CORE_DIFF_MAX, core_ok))
    self._verdict_code = verdict(ok, "sun shadow jitter gate | " + " | ".join(results))

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
        self._capture(ctx, "jittered")
        self._phase = 1
        self._phase_frame = self._frame
    elif self._phase == 1:
      if self._frame >= self._phase_frame + 20:  # let the async write land
        # LIVE disarm of the jitter ALONE — the amortization and the fade window
        # stay exactly as they were, so the two captures share one schedule.
        self.sun.data.shadowJitterTexels = 0.0
        self._warm = 0     # the second capture gets its own warm-up
        self._phase = 2
        self._phase_frame = self._frame
    elif self._phase == 2:
      if self._warming(ctx, self._phase_frame + SWITCH_FRAMES):
        return
      if self._frame >= self._phase_frame + SWITCH_FRAMES:
        self._capture(ctx, "plain")
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
  wd = Watchdog(300.0, label="sun_shadow_jitter_gate").arm()
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
