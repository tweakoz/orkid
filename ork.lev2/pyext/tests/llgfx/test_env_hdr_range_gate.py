#!/usr/bin/env ork.python
################################################################################
# ENV-LIGHTING HDR RANGE gate (procsky wave4 slice W4-S5, "ldr-clamp").
#
# The environment (captured-IBL) contribution used to be clamped to [0,1] twice
# inside pbrEnvironmentLightingWithF0 while the analytic punctual/sun paths
# returned unclamped. Anything the sky delivered above 1.0 was discarded BEFORE
# the tone curve ran, so a bright sky arrived at the tonemapper already flat.
# This gate measures the surviving range at both ends of it.
#
# WHAT IS MEASURED, and where. Two legs in one warm process, one static scene,
# and — at every capture — one and the same sun position. The ONLY thing that
# differs between the legs is the sky's luminance (sky_exposure, 1000x apart);
# the sky's SHAPE is identical, which is what makes the response oracle below
# exact rather than approximate.
#
#   DIM     a low-luminance sky. Oracle: the IBL-lit ball carries NONZERO
#           radiance. A [0,1] clamp is a ceiling, not a floor, so this leg
#           reads the SAME number before and after the fix BY CONSTRUCTION
#           (measured: 0.0735 both ways) — it is the "faint sky energy is not
#           crushed" floor and the denominator of the response oracle.
#
#   BRIGHT  a bright, sun-adjacent sky. Oracle: the lit ball carries radiance
#           well ABOVE the old ceiling. Pre-fix the frame maxed at EXACTLY
#           2.000000 — diffuse and specular each pinned at 1.0 and summed —
#           and nothing but a clamp produces that number.
#
#   RESPONSE  the strongest of the three, because it needs no absolute
#           calibration: the env path is LINEAR in sky radiance, so the
#           bright/dim ratio measured on the lit ball must equal the
#           bright/dim ratio of the SOURCE (the equirect snapshot's own
#           cosine-weighted upper-hemisphere irradiance). A ceiling anywhere in
#           between collapses the shaded ratio while the source ratio climbs;
#           a PARTIAL ceiling, or a re-clamp at some other value, shows up here
#           even when both absolute oracles still pass.
#
# CAPTURE-SIDE EVIDENCE (the same snapshot readback) is reported alongside: the
# sky snapshot is RGBA16F and carries its HDR peak in both legs regardless of
# this fix — capture -> convolution was never the clamp, and this gate says so
# in numbers rather than by assertion.
#
# THE FLOAT SURFACE — a >1.0 claim cannot be read off an 8-bit frame, and
# reaching a float one takes a specific detour (the recipe and its three
# failure modes are documented at length in test_star_splat_energy_gate.py):
# SceneGraphViewport::DoDraw swaps in its OWN RGBA8 rtgroup for the duration of
# the viewport draw (viewport_scenegraph.cpp / surface.cpp), and handing an
# 8-bit source to an RGBA32F capture ABORTS the process rather than failing
# soft (vulkan_fbi_capture.cpp). So the scenegraph is built with an `outputRTG`
# param pointing at an RGBA32F RtGroup and rendered ONCE MORE per capture frame
# through its OWN compositor (renderOnContext), which still carries the
# RtGroupOutputCompositingNode we installed. presetForwardPBR has no tonemap
# node and no PostFxChain is attached here, so that surface holds the LINEAR
# pre-tonemap radiance the forward node wrote.
#
# NOT ork.testing capture_app: that harness drives one mainThreadLoop capture,
# and this gate needs a multi-leg state machine (waiting on engine-driven
# refilter publishes) inside one warm process. The verdict-before-teardown
# protocol is honoured.
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
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

tokens = CrcStringProxy()

WIDTH, HEIGHT = 512, 384
CAM_DIST      = 7.0

SETTLE_FRAMES = 24       # after a publish, before capturing
WAIT_SECONDS  = 90.0     # hard ceiling on any wait-for-engine phase

DIFFUSE_BALL_POS = vec3(0, 2, 0)
MIRROR_BALL_POS  = vec3(2.6, 2, 0)

# ONE sun position for both legs — see the response oracle in the header. The
# park elevation is where each leg walks the sun to and back from, which is how
# an exposure change reaches a published snapshot at all: the shipped refilter
# triggers are sun-ANGLE thresholds, so a static sun publishes exactly once.
SUN_ELEV_DEG  = 20.0
SUN_ELEV_PARK = 60.0

# The legs, dim and bright, 1000x apart in sky luminance. Neither end is a free
# choice: the bright end has to clear the old [0,1]-per-channel ceiling by
# enough that a partial recovery is still a failure, and the dim end has to stay
# clear of fp16 DENORMALS in the radiance maps. Measured at the shipped exposure
# with the sun below the horizon, the sky peaks near 1e-4 — under the fp16
# minimum normal (6.1e-5) once the convolution spreads it — and the receiver
# came back EXACTLY 0. That is the map format's flush-to-zero, nothing to do
# with this slice's clamp, and it is not what this gate is here to measure.
EXPOSURE_DIM    = 4.0
EXPOSURE_BRIGHT = 4000.0
LEGS = [
    dict(key="dim",    exposure=EXPOSURE_DIM),
    dict(key="bright", exposure=EXPOSURE_BRIGHT),
]

# oracle constants
DIM_FLOOR = 1.0e-6   # lit radiance the dim leg must clear
# The old ceiling is 2.0, not 1.0: pbrEnvironmentLightingWithF0 saturated its
# diffuse and specular channels SEPARATELY and the fragment stage sums them, so
# a pre-fix frame could reach 2 and no further (measured pre-fix peak: exactly
# 2.000000 on the bright leg).
HDR_BAR = 2.0
RESPONSE_TOLERANCE = 2.0     # shaded/source response ratio, allowed factor either way


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class FloatOutSGC(StandardSceneGraphComponent):
  """StandardSceneGraphComponent whose scenegraph composites into an RGBA32F
  RtGroup. The param has to be in sg_params BEFORE the base class builds the
  Scene, and the RtGroup needs a context — which is exactly what _onGpuInit
  hands us, one call before the Scene is constructed."""

  def _onGpuInit(self, ctx):
    rtg = lev2.RtGroup(ctx, WIDTH, HEIGHT)
    rtg.name = "envhdr_f32"
    rtg.createBuffer(tokens.RGBA32F, tokens.color)
    self.float_rtg = rtg
    self.sg_params.outputRTG = rtg
    super()._onGpuInit(ctx)


class EnvHdrRangeApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._leg = 0
    self._state = 0
    self._state_frame = 0
    self._state_time = time.time()
    self._done = False
    self._exit_code = None
    self._realized = False
    self._inflight = None
    self._shots = {}
    self._marks = {}
    self.SGC = self.addComponent(
        "std_scenegraph", FloatOutSGC,
        eye=vec3(0, 2, -CAM_DIST), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
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
    # NO SKY ON SCREEN: this gate measures the IBL's contribution to lit
    # surfaces, so the sky itself must stay out of the frame.
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    # HARD SWAP, no crossfade. The fade window is AUTO-SIZED to the previous
    # cycle's frame span, and an offscreen context free-runs at thousands of
    # frames per second — so the window is hundreds of frames wide and a
    # capture taken a settle after the publish reads mostly the PREVIOUS sky
    # (measured: fade weight 0.15, i.e. 85% of the light came from the leg
    # before). This gate measures dynamic range, not the fade, so it takes the
    # published maps whole.
    self.atmo.ibl_crossfade_frames = 0
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    self.ball_diffuse = SGC.createBallNode("recv_diffuse", ctx=ctx, position=DIFFUSE_BALL_POS,
                                           color=vec4(1, 1, 1, 1), metallic=0.0, roughness=1.0,
                                           scale=1.2)
    self.ball_mirror = SGC.createBallNode("recv_mirror", ctx=ctx, position=MIRROR_BALL_POS,
                                          color=vec4(1, 1, 1, 1), metallic=1.0, roughness=0.05,
                                          scale=1.2)

    # the sun is a DIRECTION SOURCE ONLY: intensity 0 keeps every photon in the
    # frame image-based, while the prologue still bakes the sky-view LUT (and
    # therefore the snapshot) with this direction.
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(SUN_ELEV_DEG)
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass   # static: the only frame-to-frame differences are the ones we make

  ##############################################################

  def _aimSun(self, elevation_deg):
    # DirectionalLight::direction() is the light's TRAVEL direction, so aiming
    # it from the sun's position at the origin makes dir_to_sun the negation —
    # exactly what the prologue's LUT step reads.
    d = dir_to_sun(0.0, elevation_deg)
    self.sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))

  def _pbr(self):
    return self.SGC.pbr_common

  def _note(self, txt):
    print("[env-hdr] %s" % txt, flush=True)

  def _fail(self, why):
    verdict(False, why)
    self._exit_code = 1
    self._done = True
    self.ezapp.signalExit()

  ##############################################################
  # capture plumbing
  ##############################################################

  def _issueCapture(self, ctx, key, rtg, fmt):
    buf = lev2.CaptureBuffer()
    fut = ctx.FBI.captureAsFormat(rtg.buffer(0), buf, fmt)
    self._inflight = (fut, buf, key, fmt)

  def _collect(self):
    fut, buf, key, fmt = self._inflight
    if not bool(fut.is_ready):
      return None
    w, h = buf.width, buf.height
    img = numpy.array(buf, dtype=numpy.float32).reshape(h, w, 4)[..., :3].copy()
    self._shots[key] = img
    self._inflight = None
    print("[env-hdr] captured %s %dx%d mean=%.6g max=%.6g" %
          (key, w, h, float(img.mean()), float(img.max())), flush=True)
    return key

  ##############################################################
  # per-leg state machine
  ##############################################################

  def _restate(self, s):
    self._state = s
    self._state_frame = self._frame
    self._state_time = time.time()

  def _published(self, key, mark, count):
    """has the feed published `count` generations past the mark, with nothing
    still running? Loud, bounded failure if it never does — a silent stall here
    would otherwise read as a legitimate capture of the previous leg's sky."""
    want = self._marks[mark + key] + count
    gen = int(self._pbr().sky_ibl_generation)
    if (gen >= want) and (not bool(self._pbr().sky_ibl_inflight)):
      return True
    if (time.time() - self._state_time) > WAIT_SECONDS:
      self._fail("leg %s: generation never reached %d (%.1f s, %d frames, gen=%d inflight=%s)"
                 % (key, want, time.time() - self._state_time,
                    self._frame - self._state_frame, gen,
                    bool(self._pbr().sky_ibl_inflight)))
    return False

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      # PRIME: let the first-ever refilter finish before any leg opens. That
      # cycle starts on its own the moment the procedural source is selected,
      # and its snapshot IS the keyframe every later sun-move trigger is
      # measured against — so a leg that parks the sun while it is still
      # running can end up returning the sun to the very direction the
      # keyframe already holds, and the return move triggers nothing at all
      # (measured: 127k frames waiting on a publish that could never come).
      if bool(self._pbr().sky_ibl_ready) and (not bool(self._pbr().sky_ibl_inflight)):
        self._built = True
        self._restate(0)
      elif (time.time() - self._state_time) > WAIT_SECONDS:
        self._fail("the first procedural refilter never published")
      return
    if self._leg >= len(LEGS):
      return

    leg = LEGS[self._leg]
    key = leg["key"]

    ##########################################################
    # 0: apply this leg's sky luminance, and PARK THE SUN. The shipped feed
    #    only refilters when the sun MOVES past a threshold, and an exposure
    #    change on a static sun publishes nothing at all — so each leg walks
    #    the sun away and back. Away first, because the return move is the one
    #    whose snapshot this gate reads, and it has to land on the SAME sky
    #    shape both legs measured (see the response oracle).
    ##########################################################
    if self._state == 0:
      self._marks["gen0_" + key] = int(self._pbr().sky_ibl_generation)
      self.atmo.sky_exposure = leg["exposure"]
      self._aimSun(SUN_ELEV_PARK)
      self._note("leg %s: sky_exposure %.1f, sun parked at %.1f deg (generation was %d)"
                 % (key, leg["exposure"], SUN_ELEV_PARK, self._marks["gen0_" + key]))
      self._restate(1)
      return

    ##########################################################
    # 1: the park publish
    ##########################################################
    if self._state == 1:
      if not self._published(key, "gen0_", 1):
        return
      self._aimSun(SUN_ELEV_DEG)
      self._marks["gen1_" + key] = int(self._pbr().sky_ibl_generation)
      self._restate(2)
      return

    ##########################################################
    # 2: the return publish — the snapshot this leg is measured on
    ##########################################################
    if self._state == 2:
      if not self._published(key, "gen1_", 1):
        return
      self._marks["gen_" + key] = int(self._pbr().sky_ibl_generation)
      self._restate(3)
      return

    ##########################################################
    # 3: settle, so the published maps are the ones being sampled
    ##########################################################
    if self._state == 3:
      fade = float(self._pbr().sky_ibl_fade_weight)
      if fade < 0.9999:
        # self-defence: with ibl_crossfade_frames=0 this is 1.0 on the swap
        # frame, so anything else means the hard-swap knob stopped taking
        # effect and every number below would be a blend of two skies.
        if (time.time() - self._state_time) > WAIT_SECONDS:
          self._fail("leg %s: crossfade never settled (weight=%.4f)" % (key, fade))
        return
      if (self._frame - self._state_frame) >= SETTLE_FRAMES:
        self._restate(4)
      return

    ##########################################################
    # 4: render through the scene's OWN compositor (the viewport's substitute
    #    output node can never reach a float surface) and capture it
    ##########################################################
    if self._state == 4:
      self.SGC.scenegraph.renderOnContext(ctx)
      if not self._realized:
        # REALIZE THE TARGET, don't hope it is realized: rtGroupInit is the
        # pybound PushRtGroup/PopRtGroup pair that builds the vulkan RtBuffer
        # impl captureAsFormat asserts on — an unrealized target aborts the
        # process natively rather than raising.
        ctx.FBI.rtGroupInit(self.SGC.float_rtg)
        self._realized = True
        return
      # the state every number in this leg is conditioned on, sampled at the
      # frame of the capture rather than assumed: any nonzero ambient or any
      # level other than 1.0 would put light in the frame that is not the sky's
      p = self._pbr()
      self._note("%s: fade=%.4f maps=%s ambient=%s diffuseLevel=%.3f "
                 "specularLevel=%.3f skyboxLevel=%.3f"
                 % (key, float(p.sky_ibl_fade_weight), repr(p.active_radiance_maps),
                    repr(p.ambientLevel), float(p.diffuseLevel),
                    float(p.specularLevel), float(p.skyboxLevel)))
      self._issueCapture(ctx, "lit_" + key, self.SGC.float_rtg, "RGBA32F")
      self._restate(5)
      return

    ##########################################################
    # 5: collect the lit frame, then capture the SOURCE (equirect snapshot)
    ##########################################################
    if self._state == 5:
      if self._collect() is None:
        return
      rtg = self._pbr().sky_ibl_snapshot_rtgroup
      if rtg is None:
        self._fail("leg %s: no equirect snapshot RTG after a published cycle" % key)
        return
      self._issueCapture(ctx, "snap_" + key, rtg, "RGBA32F")
      self._restate(6)
      return

    ##########################################################
    # 6: collect the snapshot, advance
    ##########################################################
    if self._collect() is None:
      return
    self._leg += 1
    self._restate(0)
    if self._leg >= len(LEGS):
      self._emitVerdict()

  ##############################################################
  # observables
  ##############################################################

  def _ballBox(self, wpos, img):
    """image-space box over the ball at `wpos`, projected through the ENGINE's
    camera rather than guessed from the framing."""
    h, w, _ = img.shape
    ndc = self.SGC.camera.project(float(w) / float(h), wpos)
    cx = (ndc.x * 0.5 + 0.5) * w
    cy = (ndc.y * 0.5 + 0.5) * h
    half = int(h * 0.10)
    return (max(int(cy) - half, 0), min(int(cy) + half, h),
            max(int(cx) - half, 0), min(int(cx) + half, w))

  def _litMean(self, img):
    """mean linear radiance over the lambertian receiver. The diffuse ball, not
    the mirror one: its response to the env is the diffuse irradiance map
    straight through, which is what makes the response oracle a clean ratio."""
    r0, r1, c0, c1 = self._ballBox(DIFFUSE_BALL_POS, img)
    return float(img[r0:r1, c0:c1].mean())

  def _hemiIrradiance(self, snap):
    """cosine-weighted hemispherical irradiance of the equirect snapshot, above
    and below the horizon. Row v maps to the polar angle v*pi from +Y (the
    convention skyEquirectUV2Dir writes with), so the sin(theta) solid-angle
    weight and the |cos(theta)| projection are both explicit here. This is the
    SOURCE in radiance units, before anything in the shader can compress it."""
    h, _, _ = snap.shape
    lum = snap.astype(numpy.float64)[..., :3].mean(axis=2).mean(axis=1)
    th = (numpy.arange(h) + 0.5) / h * math.pi
    contrib = lum * numpy.abs(numpy.cos(th)) * numpy.sin(th)
    return float(contrib[th < math.pi * 0.5].sum()), float(contrib[th >= math.pi * 0.5].sum())

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    have = all(("lit_" + L["key"]) in self._shots and ("snap_" + L["key"]) in self._shots
               for L in LEGS)
    check("captures_present", have)
    if not have:
      self._fail("missing captures")
      return

    lit_l = self._shots["lit_dim"]
    lit_h = self._shots["lit_bright"]
    snap_l = self._shots["snap_dim"]
    snap_h = self._shots["snap_bright"]

    l_mean = self._litMean(lit_l)
    h_mean = self._litMean(lit_h)
    h_max = float(lit_h.max())
    e_up_l, _ = self._hemiIrradiance(snap_l)
    e_up_h, _ = self._hemiIrradiance(snap_h)

    ##########################################################
    # CAPTURE SIDE: the sky snapshot is HDR in both legs, before and after this
    # slice. Reported as evidence and asserted only as a floor — capture ->
    # convolution was never the clamp, and this gate says so in numbers.
    ##########################################################
    print("=== source (equirect sky snapshot, RGBA16F) ===", flush=True)
    print("  dim     max=%.6g  E_up=%.6g" % (float(snap_l.max()), e_up_l), flush=True)
    print("  bright  max=%.6g  E_up=%.6g" % (float(snap_h.max()), e_up_h), flush=True)
    check("source_dim_nonzero", e_up_l > 0.0, "E_up=%.6g" % e_up_l)
    check("source_bright_is_hdr", float(snap_h.max()) > HDR_BAR,
          "max=%.6g" % float(snap_h.max()))

    ##########################################################
    # LOW END: faint sky energy is not crushed to zero
    ##########################################################
    print("=== lit receiver, pre-tonemap linear radiance ===", flush=True)
    for nm, im in (("dim", lit_l), ("bright", lit_h)):
      bb = self._ballBox(DIFFUSE_BALL_POS, im)
      mb = self._ballBox(MIRROR_BALL_POS, im)
      print("  %-7s background=%.6g  diffuse_ball mean=%.6g max=%.6g  "
            "mirror_ball mean=%.6g max=%.6g  frame max=%.6g"
            % (nm, float(im[0:24, 0:24].mean()),
               float(im[bb[0]:bb[1], bb[2]:bb[3]].mean()),
               float(im[bb[0]:bb[1], bb[2]:bb[3]].max()),
               float(im[mb[0]:mb[1], mb[2]:mb[3]].mean()),
               float(im[mb[0]:mb[1], mb[2]:mb[3]].max()),
               float(im.max())),
            flush=True)
    check("dim_energy_survives", l_mean > DIM_FLOOR,
          "ball_mean=%.6g (floor %.6g)" % (l_mean, DIM_FLOOR))

    ##########################################################
    # HIGH END: radiance above the old ceiling reaches the shaded surface
    ##########################################################
    check("bright_frame_exceeds_ldr_ceiling", h_max > HDR_BAR,
          "frame_max=%.6g (bar %.6g)" % (h_max, HDR_BAR))
    check("bright_receiver_exceeds_ldr_ceiling", h_mean > HDR_BAR,
          "ball_mean=%.6g (bar %.6g)" % (h_mean, HDR_BAR))

    ##########################################################
    # RESPONSE: same sky, two luminances, so the lit ball's bright/dim ratio
    # must equal the source's. A ceiling collapses the former while the latter
    # keeps climbing.
    ##########################################################
    r_source = e_up_h / max(e_up_l, 1.0e-30)
    r_shaded = h_mean / max(l_mean, 1.0e-30)
    ratio = r_shaded / max(r_source, 1.0e-30)
    print("=== response ===", flush=True)
    print("  source bright/dim=%.6g   shaded bright/dim=%.6g   agreement=%.4g"
          % (r_source, r_shaded, ratio), flush=True)
    check("shaded_response_tracks_source",
          (ratio > 1.0 / RESPONSE_TOLERANCE) and (ratio < RESPONSE_TOLERANCE),
          "shaded/source=%.4g (allowed %.2g..%.2g)"
          % (ratio, 1.0 / RESPONSE_TOLERANCE, RESPONSE_TOLERANCE))

    ok = (len(failures) == 0)
    detail = ("dim_ball=%.6g bright_ball=%.6g bright_max=%.6g source_ratio=%.6g "
              "shaded_ratio=%.6g agreement=%.4g"
              % (l_mean, h_mean, h_max, r_source, r_shaded, ratio))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = EnvHdrRangeApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
