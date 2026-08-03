#!/usr/bin/env ork.python
################################################################################
# SKYLIGHT lane B slice B3 — IBL-FROM-SKY FEED gate (spec §4.B step 5).
#
# One warm process, one static scene, six captures. The scene deliberately draws
# NO skybox (enable_skybox=False) and carries NO direct light (the sun is a
# direction source at intensity 0), so every lit pixel is image-based lighting
# and nothing else — a change in the frame IS a change in the IBL:
#
#   a) SWITCH      with the sky source procedural, the prologue snapshots the sky
#                  and the sliced refilter publishes into a SECOND RadianceMaps;
#                  pbr_common.active_radiance_maps flips to it and flips back in
#                  baked mode.
#   b) RECEIVER    the balls are NONBLACK under the procedural IBL and differ
#                  materially from their baked-IBL appearance; the snapshot is
#                  SKY OVER GROUND and the mirror ball is lit from the same side
#                  as it is under the baked IBL — which a vertically flipped
#                  equirect snapshot would invert.
#
# EXPECTATION UPDATE 2026-07-25 (sky floor: skySampleSkyView no longer returns
# black below the horizon, so the procedural snapshot has a LIT lower hemisphere
# — see test_sky_floor_gate.py). Two oracles in (b) were measuring quantities
# that a black lower hemisphere had made artificially easy; both were replaced
# with STRONGER ones rather than loosened:
#   * frame delta: mean-vs-mean (|mean(proc)-mean(baked)| > 10%) -> mean ABSOLUTE
#     per-pixel delta > 2 levels. The two means are no longer far apart (baked
#     33.15 vs proc 32.93 measured) because the procedural IBL now carries a
#     ground bounce, but the frames themselves differ hugely (mean |delta| 12.6,
#     21% of pixels by more than 4 levels). A mean-vs-mean test cannot tell "the
#     same image" from "a different image with the same average".
#   * which way is up: the DIFFUSE ball's vertical asymmetry (-5.4 -> +9.9 across
#     the fix) was a 1-7% signal on a receiver whose top sits in the compressed
#     end of the tonemap, so a physically modest ground bounce flips its sign.
#     Replaced by (i) the snapshot's own cosine-weighted hemispherical
#     irradiance, up/down measured 2.9 (a flipped snapshot reads ~0.35), and
#     (ii) the MIRROR ball's asymmetry, a 50-170 level signal that stays firmly
#     positive under baked (+51), pre-fix procedural (+167) and post-fix
#     procedural (+82) alike.
#   c) ONE CYCLE   (chaining off, see the setup) a sub-threshold nudge starts NO
#                  cycle; a sun move past
#                  IblRefilterAngleDeg starts EXACTLY one (generation +1,
#                  cycles_started +1), and the snapshot's sun glow moves by the
#                  azimuth the sun moved.
#   d) BYTE-EXACT  switching back to the baked source reproduces the first baked
#                  capture byte for byte (L6: the baked path is untouched).
#
# The §2 snapshot law shows up as cycles_started never running ahead of
# generation: a cycle can only begin after the previous one has published.
#
# Not ork.testing capture_app: that harness drives ONE mainThreadLoop capture and
# this gate needs a multi-capture state machine (with waits on engine-driven
# async completion) inside one warm process. The verdict-before-teardown
# protocol (#57) is honoured.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing
# resolves from the same tree as this test (worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import math
import time
import numpy
from orkengine.core import vec3, vec4, asyncWorkSummary
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

WIDTH, HEIGHT = 512, 384
CAM_DIST      = 7.0

# SETTLES ARE IN FRAMES, WAITS ARE IN SECONDS, and the distinction matters: an
# offscreen context here runs several thousand frames per second, so a frame
# count is a fine "let the change reach the pipeline" delay and a useless "let
# the engine finish something asynchronous" one (900 frames elapse in 0.1s,
# while a 4k .xir env map takes seconds to stream in).
SETTLE_FRAMES  = 30      # after a load / a publish, before capturing
STEP_SETTLE    = 12      # after a live state change, before capturing
NUDGE_SETTLE   = 60      # long enough that a wrongly-triggered cycle would show
WAIT_SECONDS   = 90.0    # hard ceiling on any wait-for-engine phase

DIFFUSE_BALL_POS = vec3(0, 2, 0)
MIRROR_BALL_POS  = vec3(2.6, 2, 0)   # the orientation probe — projected, not guessed

SUN_ELEV_DEG   = 20.0
SUN_AZIM_A     = 0.0
SUN_AZIM_NUDGE = 0.5     # BELOW the default 2.0 degree refilter threshold
SUN_AZIM_B     = 25.0    # well above it

# The snapshot extent is a KNOB (SkyAtmosphereData.ibl_snapshot_width/height) and
# COMFORT-1 moved its default, so this gate reads the knob the engine is actually
# running with rather than restating a number. Every extent-dependent oracle below
# (the sun-glow column shift) is expressed as a FRACTION of that width.


def pending_texture_uploads():
  """count of async-tracker markers held by in-flight GPU texture uploads.

  The TAG, not asyncWorkPending(), on purpose: this gate only has to know that
  the sky-IBL publish's uploads have landed, and a whole-registry poll would
  also hang on any unrelated producer (the terrain texbake never completes
  offscreen, for instance)."""
  for field in asyncWorkSummary().split():
    tag, _, count = field.partition(":")
    if tag == "texture_upload":
      return int(count)
  return 0


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


def snapshot_u_for_azimuth(azimuth_deg):
  """equirect column coordinate of a horizontal bearing, in the convention the
  radiance prefilter reads the snapshot with (envtools env_equirectangularN2UVa:
  phi = atan2(z,x), u = (phi/PI + 1)/2)."""
  d = dir_to_sun(azimuth_deg, 0.0)
  phi = math.atan2(d.z, d.x)
  return (phi / math.pi + 1.0) * 0.5


class SkyIblApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._done = False
    self._shots = {}
    self._inflight = None
    self._marks = {}
    self._phase_time = time.time()
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0, 2, -CAM_DIST), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          # the BAKED IBL this scene starts from (and returns to)
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          # no flat ambient: the receivers must be lit by the env maps alone
          "AmbientLevel":     vec3(0),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    # NO SKY ON SCREEN: this gate measures the IBL, so the sky itself must stay
    # out of the frame. The feed lives in the prologue and is unaffected.
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    # the knob values this run is gating against (engine defaults, untouched)
    self._snap_wh = (self.atmo.ibl_snapshot_width, self.atmo.ibl_snapshot_height)
    # A8 knobs. The sky-view LUT sits near 2e-2 mid-sky and tops out near 0.5 at
    # this sun elevation; the exposure lifts that into a range where an 8-bit
    # capture of a lit ball is well clear of the noise floor.
    self.atmo.sky_exposure = 40.0
    # ONE CYCLE (oracle c) is the CHAINING-OFF policy: it asserts the
    # IblRefilterAngleDeg threshold, which only governs the feed when continuous
    # chaining is off. With chaining on (the engine default) a cycle starts on
    # any sun step past ibl_chain_min_angle_deg the moment the last fade settles,
    # so a sub-threshold nudge SHOULD start one and this gate's counts would be
    # measuring the wrong policy.
    self.atmo.ibl_continuous_chain = False
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "baked"

    # Receivers, lit ONLY by the env maps: a lambertian ball dead center (the
    # unambiguous one, used for the directional oracles) and a near-mirror ball
    # off to one side (which contributes the specular half of the frame delta).
    self.ball_diffuse = SGC.createBallNode("recv_diffuse", ctx=ctx, position=DIFFUSE_BALL_POS,
                                           color=vec4(1, 1, 1, 1), metallic=0.0, roughness=1.0,
                                           scale=1.2)
    self.ball_mirror = SGC.createBallNode("recv_mirror", ctx=ctx, position=MIRROR_BALL_POS,
                                          color=vec4(1, 1, 1, 1), metallic=1.0, roughness=0.05,
                                          scale=1.2)

    # The sun is a DIRECTION SOURCE ONLY: intensity 0 keeps every photon in the
    # frame image-based, while the prologue still bakes the sky-view LUT (and
    # therefore the snapshot) with this direction.
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(SUN_AZIM_A)
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass   # static: the only frame-to-frame differences are the ones we make

  ##############################################################

  def _aimSun(self, azimuth_deg):
    # DirectionalLight::direction() is the light's TRAVEL direction, so aiming it
    # from the sun's position at the origin makes dir_to_sun the negation —
    # exactly what the prologue's LUT step reads.
    d = dir_to_sun(azimuth_deg, SUN_ELEV_DEG)
    self.sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))

  def _pbr(self):
    return self.SGC.pbr_common

  def _note(self, txt):
    print("[sky-ibl] %s" % txt, flush=True)

  ##############################################################
  # capture plumbing
  ##############################################################

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _issueCapture(self, ctx, key, rtg=None, fmt="RGBA8"):
    buf = lev2.CaptureBuffer()
    target = self._rtg(ctx) if rtg is None else rtg
    fut = ctx.FBI.captureAsFormat(target.buffer(0), buf, fmt)
    self._inflight = (fut, buf, key, fmt)

  def _collect(self):
    fut, buf, key, fmt = self._inflight
    if not bool(fut.is_ready):
      return None
    w, h = buf.width, buf.height
    if fmt == "RGBA8":
      img = numpy.array(buf, dtype=numpy.uint8).reshape(h, w, 4)[..., :3].copy()
    else:
      img = numpy.array(buf, dtype=numpy.float32).reshape(h, w, 4)[..., :3].copy()
    self._shots[key] = img
    self._inflight = None
    print("[sky-ibl] captured %s %dx%d mean=%.4f max=%.4f" %
          (key, w, h, float(img.mean()), float(img.max())), flush=True)
    return key

  ##############################################################
  # phases: wait for the engine, settle, capture, record, act
  ##############################################################

  def _phases(self):
    return [
        # 0: baked reference, with the procedural feed still completely inert.
        #    The wait is NOT decorative: the baked .xir streams in asynchronously
        #    and a warm shader cache reaches frame 60 before it lands, which
        #    captures a black reference (and then a false byte-identity failure
        #    against the round trip at the end).
        dict(key="baked_a", wait=self._bakedMapsResident, settle=SETTLE_FRAMES, snap=False,
             after=self._goProcedural),
        # 1: the first procedural cycle must publish
        dict(key="proc_a", wait=lambda: bool(self._pbr().sky_ibl_ready), settle=STEP_SETTLE,
             snap=True, after=self._nudgeSun),
        # 2: a sub-threshold nudge must NOT start a cycle
        dict(key="proc_nudge", wait=None, settle=NUDGE_SETTLE, snap=False,
             after=self._moveSun),
        # 3: a past-threshold move starts exactly one more cycle
        dict(key="proc_b", wait=lambda: int(self._pbr().sky_ibl_generation) >= 2,
             settle=STEP_SETTLE, snap=True, after=self._restoreBaked),
        # 4: the baked round trip. TWO engine waits, because this capture is the
        #    only byte-strict one and it is downstream of two different async
        #    mechanisms: the procedural cycle's publish uploads must be
        #    sampleable (_publishDrained), and the viewport must have actually
        #    REPAINTED since sky_source flipped back (_switchRepainted). The
        #    second one is the repaint contract in ui/surface.h: this gate reads
        #    the scenegraph viewport's RTG, which only repaints on dirty frames,
        #    so a 12-frame settle can contain ZERO repaints and hand back the
        #    pre-switch (procedural) pixels — measured as 0 repaints on every
        #    failing run and 1 on every passing one.
        dict(key="baked_b", wait=self._bakedRoundTripReady, settle=STEP_SETTLE, snap=False, after=None),
    ]

  def _bakedMapsResident(self):
    maps = self._pbr().RadianceMaps
    return (maps is not None) and (maps.specular is not None) and bool(maps.sh_valid)

  def _publishDrained(self):
    return (not bool(self._pbr().sky_ibl_inflight)) and (pending_texture_uploads() == 0)

  def _switchRepainted(self):
    """has the viewport repainted since sky_source flipped back to baked?

    Depth is ONE repaint (ui/surface.h): the render path reads the active
    radiance maps live at bind time, so the first repaint after the flip already
    renders the baked IBL."""
    return int(self.SGC.SGVPW.repaint_count) > self._marks["repaints_at_restore"]

  def _bakedRoundTripReady(self):
    return self._publishDrained() and self._switchRepainted()

  def _goProcedural(self):
    self._marks["t_trigger"] = time.time()
    self._marks["f_trigger"] = self._frame
    self._pbr().sky_source = "procedural"
    self._note("sky_source=procedural (cycle 1 starts at the next prologue)")

  def _nudgeSun(self):
    self._aimSun(SUN_AZIM_NUDGE)
    self._note("sun nudged to azimuth %.1f deg (below the %.2f deg threshold)" %
               (SUN_AZIM_NUDGE, self.atmo.ibl_refilter_angle_deg))

  def _moveSun(self):
    self._marks["cycles_after_nudge"] = int(self._pbr().sky_ibl_cycles_started)
    self._marks["t_trigger2"] = time.time()
    self._marks["f_trigger2"] = self._frame
    self._aimSun(SUN_AZIM_B)
    self._note("sun moved to azimuth %.1f deg" % SUN_AZIM_B)

  def _restoreBaked(self):
    self._aimSun(SUN_AZIM_A)
    # sampled BEFORE the flip and read by _switchRepainted: this runs in
    # onGpuPostFrame, after this frame's repaint, so any advance past it is a
    # repaint that saw the new sky source.
    self._marks["repaints_at_restore"] = int(self.SGC.SGVPW.repaint_count)
    self._pbr().sky_source = "baked"
    self._note("sky_source=baked (restored)")

  ##############################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_frame = self._frame
        self._phase_time = time.time()
      return

    phases = self._phases()
    step = self._phase // 3
    if step >= len(phases):
      return
    ph = phases[step]
    sub = self._phase % 3

    ########################################
    # sub 0: wait for the engine, then settle, then issue the capture
    ########################################
    if sub == 0:
      key_ready = "f_ready_" + ph["key"]
      if (ph["wait"] is not None) and (key_ready not in self._marks):
        if not ph["wait"]():
          if (time.time() - self._phase_time) > WAIT_SECONDS:
            self._fail("engine never satisfied the wait for phase '%s' (%.1f s, %d frames)" %
                       (ph["key"], time.time() - self._phase_time, self._frame - self._phase_frame))
          return
        self._marks[key_ready] = self._frame
        self._marks["t_ready_" + ph["key"]] = time.time()
        self._phase_frame = self._frame
        self._phase_time = time.time()
        return
      if (self._frame - self._phase_frame) >= ph["settle"]:
        self._issueCapture(ctx, ph["key"])
        self._phase += 1
      return

    ########################################
    # sub 1: collect it, and (where asked for) issue the snapshot capture
    ########################################
    if sub == 1:
      if self._collect() is None:
        return
      self._record(ph)
      if ph["snap"]:
        rtg = self._pbr().sky_ibl_snapshot_rtgroup
        if rtg is None:
          self._fail("no equirect snapshot RTG after a published cycle")
          return
        self._issueCapture(ctx, "snap_" + ph["key"], rtg=rtg, fmt="RGBA32F")
        self._phase += 1
        return
      self._phase += 2
      self._advance(ph)
      return

    ########################################
    # sub 2: collect the snapshot
    ########################################
    if self._collect() is None:
      return
    self._phase += 1
    self._advance(ph)

  def _record(self, ph):
    """engine-visible state, sampled at the frame the capture belongs to (before
    the phase's action can move it)."""
    pbr = self._pbr()
    key = ph["key"]
    self._marks["maps_" + key] = repr(pbr.active_radiance_maps)
    self._marks["gen_" + key] = int(pbr.sky_ibl_generation)
    self._marks["cyc_" + key] = int(pbr.sky_ibl_cycles_started)
    self._marks["ready_" + key] = bool(pbr.sky_ibl_ready)
    self._marks["repaints_" + key] = int(self.SGC.SGVPW.repaint_count)

  def _advance(self, ph):
    if ph["after"] is not None:
      ph["after"]()
    self._phase_frame = self._frame
    self._phase_time = time.time()
    if (self._phase // 3) >= len(self._phases()):
      self._emitVerdict()

  ##############################################################
  # observables
  ##############################################################

  def _centerBox(self):
    """image-space box over the center (lambertian) ball, sized from the capture
    (the scenegraph viewport RTG is inset from the requested window extent)."""
    img = self._shots["baked_a"]
    h, w, _ = img.shape
    half = int(h * 0.14)
    return (h // 2 - half, h // 2 + half, w // 2 - half, w // 2 + half)

  def _ballBox(self, wpos):
    """image-space box over the ball at `wpos`, projected through the ENGINE's
    camera so the mirror ball is found wherever the viewport inset puts it."""
    img = self._shots["baked_a"]
    h, w, _ = img.shape
    ndc = self.SGC.camera.project(float(w) / float(h), wpos)
    cx = (ndc.x * 0.5 + 0.5) * w
    cy = (ndc.y * 0.5 + 0.5) * h
    half = int(h * 0.14)
    return (max(int(cy) - half, 0), min(int(cy) + half, h),
            max(int(cx) - half, 0), min(int(cx) + half, w))

  def _vertAsym(self, img, box=None):
    """(high-row half) - (low-row half) of a ball's box. Only its SIGN is used
    — see the orientation check."""
    r0, r1, c0, c1 = box if box is not None else self._centerBox()
    mid = (r0 + r1) // 2
    return float(img[mid:r1, c0:c1].mean()) - float(img[r0:mid, c0:c1].mean())

  def _hemiIrradiance(self, snap):
    """cosine-weighted hemispherical irradiance of the equirect snapshot, above
    and below the horizon. Row v maps to the polar angle v*pi from +Y (the
    convention skyEquirectUV2Dir writes with), so the sin(theta) solid-angle
    weight and the |cos(theta)| projection are both explicit here. This is the
    snapshot's ORIENTATION, measured in radiance units before any tonemap can
    compress it."""
    h, _, _ = snap.shape
    lum = snap.astype(numpy.float64)[..., :3].mean(axis=2).mean(axis=1)
    th = (numpy.arange(h) + 0.5) / h * math.pi
    contrib = lum * numpy.abs(numpy.cos(th)) * numpy.sin(th)
    return float(contrib[th < math.pi * 0.5].sum()), float(contrib[th >= math.pi * 0.5].sum())

  def _sunColumn(self, snap):
    """column of the brightest band in the equirect snapshot — the near-sun
    horizon glow, i.e. where the sun's azimuth landed."""
    lum = snap.astype(numpy.float32).sum(axis=2)
    return float(numpy.argmax(lum.sum(axis=0)))

  def _fail(self, why):
    verdict(False, why)
    self._exit_code = 1
    self._done = True
    self.ezapp.signalExit()

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    baked_a = self._shots.get("baked_a")
    baked_b = self._shots.get("baked_b")
    proc_a = self._shots.get("proc_a")
    proc_b = self._shots.get("proc_b")
    snap_a = self._shots.get("snap_proc_a")
    snap_b = self._shots.get("snap_proc_b")

    have = all(s is not None for s in (baked_a, baked_b, proc_a, proc_b, snap_a, snap_b))
    check("captures_present", have)
    if not have:
      self._fail("missing captures")
      return

    r0, r1, c0, c1 = self._centerBox()

    ##########################################################
    # a) the IBL source switched, and switched back
    ##########################################################
    print("=== IBL source switch ===", flush=True)
    check("inert_while_baked",
          (self._marks["gen_baked_a"] == 0) and (not self._marks["ready_baked_a"]),
          "generation=%d ready=%s" % (self._marks["gen_baked_a"], self._marks["ready_baked_a"]))
    check("proc_maps_published",
          self._marks["ready_proc_a"] and (self._marks["gen_proc_a"] == 1),
          "generation=%d cycles_started=%d" % (self._marks["gen_proc_a"], self._marks["cyc_proc_a"]))
    check("accessor_switched_to_proc",
          self._marks["maps_proc_a"] != self._marks["maps_baked_a"],
          "baked=%s proc=%s" % (self._marks["maps_baked_a"], self._marks["maps_proc_a"]))
    check("accessor_switched_back",
          self._marks["maps_baked_b"] == self._marks["maps_baked_a"],
          "baked_a=%s baked_b=%s" % (self._marks["maps_baked_a"], self._marks["maps_baked_b"]))

    ##########################################################
    # b) the receivers are lit by the sky, from above
    ##########################################################
    print("=== sky-lit receivers ===", flush=True)
    d_baked = float(baked_a[r0:r1, c0:c1].mean())
    d_proc = float(proc_a[r0:r1, c0:c1].mean())
    f_baked = float(baked_a.mean())
    f_proc = float(proc_a.mean())
    check("baked_receiver_nonblack", d_baked > 2.0, "diffuse_mean=%.3f" % d_baked)
    check("proc_receiver_nonblack", d_proc > 2.0, "diffuse_mean=%.3f" % d_proc)
    check("proc_receiver_differs_from_baked",
          abs(d_proc - d_baked) > 0.15 * max(d_proc, d_baked),
          "baked=%.3f proc=%.3f" % (d_baked, d_proc))
    # THE FRAME CHANGED — per pixel, not on average (see the expectation update
    # in the header: the two frame means now sit within 2% of each other while
    # the images are wildly different).
    frame_delta = float(numpy.abs(proc_a.astype(numpy.int16) - baked_a.astype(numpy.int16)).mean())
    check("proc_frame_differs_from_baked", frame_delta > 2.0,
          "mean_abs_delta=%.3f (frame means baked=%.4f proc=%.4f)" %
          (frame_delta, f_baked, f_proc))

    # WHICH WAY IS UP. Both IBLs are equirect maps of a sky over a ground, so the
    # ball's vertical lighting gradient must lean the SAME way under both — and
    # the baked path is the blessed reference for the equirect convention the
    # prefilter reads with. Comparing the two signs asserts the snapshot's
    # vertical mapping without this test having to know which end of a capture
    # row range is the top of the frame.
    asym_baked = self._vertAsym(baked_a)
    asym_proc = self._vertAsym(proc_a)
    check("baked_receiver_has_vertical_gradient", abs(asym_baked) > 1.0,
          "asymmetry=%.3f" % asym_baked)
    check("proc_receiver_has_vertical_gradient", abs(asym_proc) > 1.0,
          "asymmetry=%.3f" % asym_proc)

    # the SOURCE's orientation, in radiance: sky above, ground below. A flipped
    # snapshot inverts this ratio outright (the measured ground bounce is ~0.35
    # of the sky, so a flip reads ~0.35 against a 2.0 bar).
    e_up, e_dn = self._hemiIrradiance(snap_a)
    check("snapshot_is_sky_over_ground", e_up > 2.0 * e_dn,
          "E_up=%.4f E_down=%.4f up/down=%.3f" % (e_up, e_dn, e_up / max(e_dn, 1.0e-12)))

    # and the orientation the PREFILTER + shader deliver, read off the mirror
    # ball (a 50-170 level signal, unlike the diffuse ball's 1-7% one).
    mbox = self._ballBox(MIRROR_BALL_POS)
    masym_baked = self._vertAsym(baked_a, mbox)
    masym_proc = self._vertAsym(proc_a, mbox)
    check("proc_mirror_lit_from_the_same_side_as_baked",
          (masym_baked * masym_proc) > 0.0,
          "baked=%.3f proc=%.3f (opposite signs = vertically flipped snapshot)" %
          (masym_baked, masym_proc))

    ##########################################################
    # c) cycles: none below threshold, exactly one above
    ##########################################################
    print("=== refilter cycles ===", flush=True)
    check("no_cycle_below_threshold", self._marks["cycles_after_nudge"] == 1,
          "cycles_started after a %.1f deg nudge = %d (expected 1)" %
          (SUN_AZIM_NUDGE, self._marks["cycles_after_nudge"]))
    check("exactly_one_cycle_above_threshold",
          (self._marks["cyc_proc_b"] == 2) and (self._marks["gen_proc_b"] == 2),
          "cycles_started=%d generation=%d" % (self._marks["cyc_proc_b"], self._marks["gen_proc_b"]))
    check("snapshot_law_no_overlap", self._marks["cyc_proc_b"] == self._marks["gen_proc_b"],
          "cycles_started never ran ahead of completed generations")

    h, w, _ = snap_a.shape
    col_a = self._sunColumn(snap_a)
    col_b = self._sunColumn(snap_b)
    pred_shift = abs(snapshot_u_for_azimuth(SUN_AZIM_B) - snapshot_u_for_azimuth(SUN_AZIM_A)) * w
    meas_shift = abs(col_b - col_a)
    check("snapshot_extent", (w, h) == self._snap_wh,
          "%dx%d (knob %dx%d)" % (w, h, self._snap_wh[0], self._snap_wh[1]))
    check("snapshot_nonblack", float(snap_a.max()) > 1.0e-3, "max=%.4f" % float(snap_a.max()))
    check("snapshot_tracks_sun_azimuth", abs(meas_shift - pred_shift) < 0.05 * w,
          "measured=%.1f predicted=%.1f (px)" % (meas_shift, pred_shift))

    ##########################################################
    # d) baked mode is untouched (L6)
    ##########################################################
    print("=== baked-mode byte identity ===", flush=True)
    # The precondition of the byte comparison below, asserted rather than
    # assumed: with zero repaints since the flip, baked_b is the PROCEDURAL
    # frame and the comparison measures the harness, not the engine. This check
    # is what keeps that failure mode from ever coming back silently.
    repaints_since_switch = self._marks["repaints_baked_b"] - self._marks["repaints_at_restore"]
    check("viewport_repainted_after_switch_back", repaints_since_switch >= 1,
          "repaints_since_sky_source_flip=%d" % repaints_since_switch)

    diff = baked_a.astype(numpy.int16) - baked_b.astype(numpy.int16)
    diff_pixels = int((numpy.abs(diff).sum(axis=2) > 0).sum())
    check("baked_roundtrip_byte_identical", diff_pixels == 0,
          "differing_pixels=%d maxdelta=%d" % (diff_pixels, int(numpy.abs(diff).max())))

    ##########################################################
    # cycle cost — REPORTED, not asserted: an offscreen context's microtask
    # scheduler is UNBOUNDED, so this is the whole-cycle cost with no realtime
    # pacing, which is exactly the number a budget conversation needs.
    ##########################################################
    print("=== cycle cost ===", flush=True)
    dt1 = self._marks["t_ready_proc_a"] - self._marks["t_trigger"]
    df1 = self._marks["f_ready_proc_a"] - self._marks["f_trigger"]
    dt2 = self._marks["t_ready_proc_b"] - self._marks["t_trigger2"]
    df2 = self._marks["f_ready_proc_b"] - self._marks["f_trigger2"]
    print("  cycle1 %.3f s over %d frames   cycle2 %.3f s over %d frames" %
          (dt1, df1, dt2, df2), flush=True)

    ok = (len(failures) == 0)
    detail = ("gen=%d cycles=%d cycle_wall=%.3f/%.3f s frames=%d/%d baked_diff_pixels=%d" %
              (self._marks["gen_proc_b"], self._marks["cyc_proc_b"], dt1, dt2, df1, df2, diff_pixels))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = SkyIblApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
