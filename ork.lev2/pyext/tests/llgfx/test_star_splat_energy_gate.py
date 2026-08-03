#!/usr/bin/env ork.python
################################################################################
# STAR SPLAT — RENDERED ENERGY gate. The closed-form half of this law lives in
# obt.project/unittests/star_splat.py (pure CPU, no GPU); this is the half that
# holds the SHADER to it, on pixels it actually drew.
#
# ONE star is rendered (the material's limiting-magnitude plug cuts the rest of
# the catalog away) at TWO render resolutions, and its footprint is INTEGRATED AS
# RENDERED — the sum of the pixels the shader produced, never the analytic ideal.
# A gate that integrates the ideal passes while the picture is wrong.
#
# FOUR legs per resolution, each in its own child process (a fresh device and a
# fresh scene per leg; the resolution is fixed at ezapp creation):
#
#   base      the shipped configuration: the star's physical sigma is far below
#             the pixel floor, so the floor is what is in force at BOTH
#             resolutions. Sum must match across resolutions.
#   double    the same, at twice the flux gain. LINEARITY PROBE: if the capture
#             path is linear the sum doubles, and only then does comparing sums
#             across legs mean anything. A nonlinear capture fails HERE, loudly,
#             instead of silently invalidating everything below.
#   cross     an angular sigma deliberately BETWEEN the two resolutions' pixel
#             floors: the coarse leg is FLOORED (widened, amplitude cut) and the
#             fine leg is not. Different peaks, different footprints — the sums
#             must still match. This is the leg that actually tests the
#             energy-conserving renormalization; `base` alone is nearly trivial
#             because a floored star has the same pixel footprint everywhere.
#   overflow  an angular sigma too wide for the baked envelope quad. This is the
#             CLIPPING CHECK: the material must REFUSE (paint its out-of-gamut
#             magenta over the whole quad), not quietly draw a gaussian cut by
#             the quad rectangle. Proving the refusal fires is what makes the
#             three integrating legs trustworthy — a clipped star loses energy
#             silently, and no sum comparison can see it.
#
# TOLERANCE — 5% relative on a sum comparison, justified rather than tuned:
#   * the forward color buffer is RGBA32F (RGBA16F under msaa), so the intended
#     regime is linear HDR with fp accumulation error far below a percent. This
#     gate runs at msaa=0 and captures RGBA32F.
#   * if the capture path it finds is an 8-BIT surface instead, quantization
#     truncates every pixel under 1/255 — a tail loss that depends on the peak,
#     so it differs between legs with different peaks by up to ~2%. The linearity
#     probe will still pass in that regime, so the tolerance has to absorb it.
#     LEG_PEAK is printed for every leg so the regime is visible in the log.
#   * the envelope quad truncates the gaussian at a radius that differs slightly
#     between resolutions (~0.1% of the energy at the declared worst case).
#   * subpixel sampling of a sigma ~= 0.9px gaussian: the discrete-sum vs
#     integral error is exp(-2*pi^2*sigma^2) ~ 1e-7, negligible.
#
# THE CAPTURE SURFACE — an energy integral must read LINEAR HDR, and reaching a
# float surface takes three specific things. Every one of them was learned by
# running a probe; none is guessable:
#
#  1. NOT THROUGH THE VIEWPORT. SceneGraphViewport::DoDraw temporarily SWAPS the
#     technique's output node for its OWN RtGroupOutputCompositingNode(_rtgroup)
#     for the duration of the draw (viewport_scenegraph.cpp:152), and
#     ui::Surface::_doGpuInit hard-codes that rtgroup to RGBA8
#     (surface.cpp:156). The pybound `rtgroup` is read-only, so no viewport
#     render can ever land in a float surface. main_RTG is RGBA8 too
#     (OutputNodeScreen.cpp:140), and handing an 8-bit source to an RGBA32F
#     capture ABORTS the process (vulkan_fbi_capture.cpp:586).
#
#  2. THE SCENE'S OWN OUTPUT NODE. With an `outputRTG` scenegraph param,
#     presetForwardPBR builds an RtGroupOutputCompositingNode over THAT rtgroup
#     (compositordata.cpp:200-207) — created here as RGBA32F. The param only
#     reaches a HOST-BUILT scenegraph, so the child mirrors the shipped
#     hypergraph runtime (ork/hypergraph/ecs/runtime.py): build the scene from
#     the SceneData's own params, add outputRTG, inject via
#     createSimulation(scenegraph=...). Then render the scene ONCE MORE per
#     frame with sg.renderOnContext(ctx) — the DIRECT path, which uses the
#     scene's own output node rather than the viewport's substitute, and which
#     also realizes the target's vulkan buffer (the thing captureAsFormat
#     asserts on at vulkan_fbi_capture.cpp:402).
#
#  3. THE CAMERA NAME IS NOT A FREE CHOICE. SceneGraphSystem publishes the
#     camera driven by the UpdateCamera notify under exactly "spawncam"
#     (SceneGraphSystem.cpp:261). Any other name silently resolves to nothing
#     and EVERY surface comes back black — which reads exactly like "the float
#     redirect did not work", and cost a probe cycle to tell apart.
#
# --diag runs the readiness probe that found (1) and (3): it captures NOTHING
# and prints the observable state once a second (frames, repaints, sim, output
# node identity mine-vs-sim, buffer count).
#
# LIFECYCLE — two engine-shaped constraints, both learned the hard way:
#   * startSimulation() is deferred out of onUpdateInit into onUpdate, after
#     frames are already drawing. Starting it at the normal place lets the ECS
#     gpu-init rendezvous drain outside a frame, where LightManager::gpuInit's
#     texture-array upload null-derefs (ENGINE DEFECT, filed; the engine guards
#     the sibling call site at SceneGraphSystem.cpp:1264-1273 but not the
#     loading-phase path).
#   * readiness is counted in REPAINTS SINCE SIM START, never in frames since
#     process start. The capture target is realized and filled by the output
#     node INSIDE a repaint; a free-running offscreen loop draws many frames
#     that are not repaints. Capturing on a frame count caught the target
#     unrealized and aborted natively (vulkan_fbi_capture.cpp:402).
#
# NOT ork.testing capture_app: the harness hosts one PrimCanvas app and cannot
# run an ECS controller (the star dome is a scene entity with a python component
# driving its sidereal turn). The hand-rolled boot below is the proven
# SceneGraphViewport + ecs.Controller shape from test_lsystem_scene_roundtrip.py,
# and it honours the verdict-before-teardown protocol (#57).
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import argparse
import math
import subprocess
import time

# ---- the two configurations --------------------------------------------------
COARSE = (640, 480)
FINE   = (1920, 1080)
# THE FIELD CASE: a 640x360 window subtends 0.181 deg/px, which the ORIGINAL
# envelope (worst case 0.16 deg/px) could not hold — the whole sky came back as
# the magenta refusal. One extra `base` leg here is the rendered guard that the
# widened envelope actually covers it; the CPU gate holds the same line on the
# constants (unittests/star_splat.py).
FIELD  = (640, 360)
FOV_DEG = 65.0                     # vertical

# ---- the star ----------------------------------------------------------------
TARGET_HR = 2491                   # Sirius: the brightest star in the catalog
FLUX_GAIN = 0.4                    # peak ~0.30 at the floor: no clipping headroom worry
LUM_CUT_FRAC = 0.99                # keep only stars at/above 0.99 x the target's luminance

# ---- the legs ----------------------------------------------------------------
# `cross` sits between MIN_SIGMA_PX x (FOV/1080) and MIN_SIGMA_PX x (FOV/480) so
# the fine leg is physical-dominated and the coarse leg is floored.
CROSS_SIGMA_DEG = 0.085
# `overflow` needs CUTOFF_SIGMA x sigma_q > 1, i.e. sigma > envelope/3.
OVERFLOW_SIGMA_DEG = 0.40

# Predicted by the sizing law (obt.project/scripts/.../mesh/_splat_sizing.py) at
# FOV 65 deg, flux 0.4, Sirius L=3.837 — i.e. what these legs are set up to do.
# Every leg's footprint integrates to flux*L = 1.53484 whatever its sigma:
#          640x480 (.1354 d/px)   1920x1080 (.0602 d/px)   640x360 (.1806 d/px)
#   base     sigma_px .900 FLOOR   sigma_px .900 FLOOR      sigma_px .900 FLOOR
#   cross    sigma_px .900 FLOOR   sigma_px 1.412 physical  (not run)
#   overflow envelope EXCEEDED     envelope EXCEEDED        (not run)
# The cross leg is the one that tests the energy-conserving renormalization: it
# is floored at 640x480 and physical at 1920x1080, so the peaks differ 1.7x and
# the sums must not.
SUM_TOL = 0.05                     # see the tolerance note in the header
LINEARITY_TOL = 0.03
CROSS_PEAK_MIN_RATIO = 1.2         # the cross legs must really straddle the floor

# Readiness is measured in REPAINTS SINCE SIM START, not frames since process
# start: the compositor realizes and fills the capture target inside a repaint,
# and a free-running offscreen loop draws many frames that are not repaints.
CAM_NAME = "spawncam"              # the ONLY name SceneGraphSystem publishes (see below)
SIM_START_FRAME = 10               # drawn frames before startSimulation (see onUpdateInit)
SETTLE_REPAINTS = 60               # composited frames before the capture
READY_TIMEOUT_S = 90.0             # wall-clock ceiling on becoming capturable
DIAG_SECONDS = 60.0                # --diag: observe this long, capture nothing
CHILD_TIMEOUT = 180


def _legs():
  return {
    "base":     dict(flux_gain=FLUX_GAIN),
    "double":   dict(flux_gain=FLUX_GAIN * 2.0),
    "cross":    dict(flux_gain=FLUX_GAIN, star_sigma_deg=CROSS_SIGMA_DEG),
    "overflow": dict(flux_gain=FLUX_GAIN, star_sigma_deg=OVERFLOW_SIGMA_DEG),
  }


################################################################################
# SKY SEARCH — pure python, no engine: pick a clock at which the target star is
# well up AND the sun is well down, so the material's night fade is fully open
# and the horizon mask is not in play. Deterministic, so both resolutions and
# every leg render the same sky.
################################################################################

def find_sky(scripts_dir):
  sys.path.insert(0, scripts_dir)
  from ork.hypergraph.ecs.scene import _celestial
  from ork.hypergraph.assets.mesh import _bsc5

  stars, _stats = _bsc5.load_catalog(verbose=False)
  target = [s for s in stars if s.hr == TARGET_HR][0]

  latitude, day = 20.0, 20.0
  for step in range(96):                       # 24h in 15-minute steps
    tod = step * 0.25
    cfg = {"latitude_deg": latitude, "day_of_year": day,
           "time_of_day": tod, "time_scale": 0.0}
    snap = _celestial.CelestialModel.from_config(cfg).at(0.0)
    elev, azim = _celestial._alt_az(target.ra_deg, target.dec_deg,
                                    snap.sidereal_angle_deg, latitude)
    if elev > 35.0 and snap.sun_elevation_deg < -20.0:
      return cfg, elev, azim, target.luminance
  raise RuntimeError("no clock puts HR%d above 35 deg in a dark sky" % TARGET_HR)


################################################################################
# CHILD — one (resolution, leg): build the scene, render, integrate, print.
################################################################################

def run_child(width, height, leg, scripts_dir, diag=False):
  from orkengine import core                                  # core FIRST
  from orkengine import lev2
  from orkengine import ecs
  from orkengine.core import vec3, vec4, CrcStringProxy
  import numpy

  sys.path.insert(0, scripts_dir)
  from ork.hypergraph.ecs.scene import Scene, _celestial
  from ork.hypergraph.ecs.scene.assets import wire_scene_data
  from ork.hypergraph.assets.mesh import _splat_sizing as _sz

  tokens = CrcStringProxy()
  cfg, elev, azim, lum = find_sky(scripts_dir)
  look = _celestial._sky_vector(elev, azim)
  mp = dict(_legs()[leg])
  mp["lum_cut"] = lum * LUM_CUT_FRAC

  class StarGateScene(Scene):
    def __init__(self):
      super().__init__()
      # black everything: the star field must be the ONLY light in the frame, or
      # the footprint sum is measuring the sky.
      self.scenegraph(preset="ForwardPBR",
                      SkyboxIntensity=0.0,
                      DiffuseIntensity=0.0,
                      SpecularIntensity=0.0,
                      AmbientLight=vec3(0.0),
                      msaa=0, ssaa=0)
      self.sun(celestial=cfg)
      self.stars(material_params=mp)

  class ChildApp:
    def __init__(self):
      self.ezapp = lev2.OrkEzApp.createEx(
          self, [ecs.ecsInitCallback], name="starsplat_energy",
          width=width, height=height, offscreen=True, ssaa=0)
      self.ezapp.setRefreshPolicy(lev2.RefreshFastest, -1)
      self.ezapp.topWidget.enableUiDraw()
      lg = self.ezapp.topLayoutGroup
      lg.margin = 0
      self.sgv = lg.makeChild(fill=True, margin=0,
                              uiclass=lev2.ui.SceneGraphViewport,
                              args=["Viewport", vec4(0, 0, 0, 1)]).widget
      # "spawncam" is NOT a free choice: SceneGraphSystem publishes the camera the
      # UpdateCamera notify drives under exactly that name
      # (SceneGraphSystem.cpp:261, `_camlut["spawncam"] = _camera`). Any other
      # name fails to resolve and the frame renders EMPTY — every surface black.
      self.sgv.cameraName = CAM_NAME
      self.ctrl = None
      self.sysref = None
      self.sg = None
      self.frtg = None
      self._fatal = None
      self._frame = 0
      self._t0 = time.time()
      self._sim_start_t = None
      self._repaints_at_sim_start = None
      self._diag_next = 0.0
      self._realized = False
      self._inflight = None
      self.result = None
      self._exit_code = 1

    def onGpuInit(self, ctx):
      self.ctx = ctx
      scene = StarGateScene()
      sd = ecs.SceneData()
      scene.build(sd)
      wire_scene_data(sd, ezapp=self.ezapp)

      from lev2utils.cameras import setupUiCameraX
      self.cameralut = lev2.CameraDataLut()
      self.camera, self.uicam = setupUiCameraX(cameralut=self.cameralut,
                                               camname=CAM_NAME)
      self.uicam.fov = FOV_DEG * math.pi / 180.0
      self.uicam.near_min = 1.0
      # the sky shell sits at 30 km; the far plane must clear it
      self.uicam.far_max = 200000.0
      self.uicam.lookAt(vec3(0, 0, 0),
                        vec3(look[0], look[1], look[2]) * 1000.0,
                        vec3(0, 1, 0))
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)

      # ---- THE FLOAT OUTPUT SURFACE -----------------------------------------
      # An energy integral has to read LINEAR HDR. Every surface python can
      # normally reach for a rendered frame is RGBA8 (main_RTG, and the
      # SceneGraphViewport's own rtgroup), and handing one of those to an
      # RGBA32F capture is not a soft failure — it is a native abort
      # (vulkan_fbi_capture.cpp:586, `is_f32_source || is_f16_source`).
      #
      # The reachable float path is the scenegraph's `outputRTG` param: with it
      # set, presetForwardPBR builds an RtGroupOutputCompositingNode that blits
      # the composited frame into THAT rtgroup instead of the screen
      # (compositordata.cpp presetForwardPBR; OutputNodeRtGroup.cpp). The param
      # only reaches the scenegraph on a HOST-BUILT scene, so this mirrors the
      # shipped hypergraph runtime (ork/hypergraph/ecs/runtime.py
      # create_scenegraph + createSimulation(scenegraph=...)): build the scene
      # from the SceneData's own params, add outputRTG, inject it.
      self.frtg = lev2.RtGroup(ctx, width, height)
      self.frtg.name = "starsplat_f32"
      self.frtg.createBuffer(tokens.RGBA32F, tokens.color)

      sgp = sd.generateSceneGraphParams()
      sgp.outputRTG = self.frtg
      sg = lev2.scenegraph.Scene(sgp)
      # the layers the shipped runtime creates, in its order — the star dome
      # node is declared on std_transparent and would have nowhere to land.
      for _lyr in ("std_forward", "std_transparent", "hud_overlay"):
        sg.createLayer(_lyr)
      # the outputRTG param taking effect IS the float surface; if the preset
      # ignored it the frame goes to the screen and the capture would abort on
      # an 8-bit source. Name that here instead.
      onode = repr(sg.compositoroutputnode)
      if "RtGroupOutputCompositingNode" not in onode:
        print("LEG %s FAILED: outputRTG ignored — output node is %s"
              % (leg, onode), flush=True)
        self._fatal = "outputRTG ignored (%s)" % onode
      print("[star-splat] output node = %s  rtg=%dx%d RGBA32F"
            % (onode, width, height), flush=True)

      self.sg = sg
      self.sgv.scenegraph = sg
      self.sgv.forkDB()

      self.ctrl = ecs.Controller()
      self.ctrl.bindScene(sd)
      self.ctrl.createSimulation(scenegraph=sg)

    def onGpuUpdate(self, ctx):
      if self.ctrl:
        self.ctrl.gpuUpdate(ctx)

    def onUpdateInit(self):
      # DELIBERATELY EMPTY. startSimulation() belongs here (player main.cpp
      # onUpdateInit, and every sibling gate does it here) — but on this engine
      # the ECS gpu-init rendezvous can drain OUTSIDE a frame, and
      # LightManager::gpuInit's texture-array upload null-derefs when it does.
      # The engine guards the sibling call site (SceneGraphSystem.cpp:1264-1273)
      # but not the loading-phase path. ENGINE DEFECT, filed; the gate-side
      # workaround (verified under gdb) is to start the sim from onUpdate once
      # frames are already being drawn — see _maybeStartSim.
      pass

    def _maybeStartSim(self):
      """Start the sim only after real frames are on the wire (see
      onUpdateInit for why), and stamp the repaint clock we settle against."""
      if self.sysref is not None or self._frame < SIM_START_FRAME:
        return
      self.ctrl.startSimulation()
      self.sysref = self.ctrl.findSystem("SceneGraphSystem")
      self._sim_start_t = time.time()
      self._repaints_at_sim_start = int(self.sgv.repaint_count)
      print("[star-splat] sim started at frame %d (repaints=%d)"
            % (self._frame, self._repaints_at_sim_start), flush=True)

    def onUpdate(self, updinfo):
      if self.ctrl is None:
        return
      self._maybeStartSim()
      if self.sysref is None:
        return
      UIC = self.uicam.cameradata
      self.ctrl.systemNotify(self.sysref, tokens.UpdateCamera,
                             {tokens.eye: UIC.eye, tokens.tgt: UIC.target,
                              tokens.up: UIC.up, tokens.near: UIC.near,
                              tokens.far: UIC.far, tokens.fovy: UIC.fovy})
      # updateSimulation() — the ecs.Controller binding's update entry point
      # (ork.ecs/pyext/pyext_controller.cpp; there is no bare update()).
      self.ctrl.updateSimulation()
      self.sgv.setDirty()

    def _repaints_since_sim(self):
      if self._repaints_at_sim_start is None:
        return 0
      return int(self.sgv.repaint_count) - self._repaints_at_sim_start

    def _capture_ready(self):
      """Is the composite target SAFE to capture yet?

      PROBED, never attempted: both failure modes here abort natively rather
      than raise, so there is nothing to catch — captureAsFormat asserts that
      the RtBuffer has a realized vulkan impl (vulkan_fbi_capture.cpp:402) and
      that the source is float (:586).

      The realized impl is NOT introspectable from python (RtBuffer exposes
      only texture/clearColor/autoclear, and `texture` is built CPU-side in the
      RtBuffer ctor — non-None long before the GPU image exists, so it is NOT a
      readiness signal). What IS both introspectable and exactly equivalent is
      the surface's REPAINT CLOCK: the rtgroup is realized by the output node
      pushing it, which happens inside a repaint, so N repaints since sim start
      means the compositor has blitted into this rtgroup N times. ui/surface.h
      says as much at the binding: "wait for it to ADVANCE before capturing
      this surface's rtgroup. Counting frames instead is a bug — a free-running
      loop can render many frames with no repaint at all." The previous run
      counted frames from PROCESS start and captured an unrealized buffer."""
      if self.sysref is None or self.frtg is None:
        return False
      if self.frtg.numBuffers < 1:
        return False
      return self._repaints_since_sim() >= SETTLE_REPAINTS

    def _diagLine(self):
      """Everything python can see about the readiness question, once a second.
      Captures NOTHING: the two ways this can be wrong both abort natively, so
      the probe must never touch captureAsFormat."""
      sim = self.ctrl.simulation if self.ctrl else None
      sgs = None
      try:
        sgs = sim.sceneGraphScene if sim else None
      except Exception as e:
        sgs = "ERR:%r" % (e,)
      # decisive injection check: does the SIM's scenegraph carry the SAME
      # output node object as the one we built with outputRTG? if not, the
      # compositor is drawing somewhere else and our rtgroup is never touched.
      mine = repr(self.sg.compositoroutputnode) if self.sg else "-"
      theirs = "-"
      if sgs is not None and not isinstance(sgs, str):
        try:
          theirs = repr(sgs.compositoroutputnode)
        except Exception as e:
          theirs = "ERR:%r" % (e,)
      nb = int(self.frtg.numBuffers) if self.frtg else -1
      tex0 = "-"
      if self.frtg and nb > 0:
        try:
          tex0 = "yes" if (self.frtg.buffer(0).texture is not None) else "no"
        except Exception as e:
          tex0 = "ERR:%r" % (e,)
      print("DIAG t=%6.2f frame=%6d repaints=%6d since_sim=%6d simstarted=%d "
            "sim=%d sgscene=%s nb=%d rtg=%dx%d tex0=%s onode_mine=%s onode_sim=%s "
            "match=%d"
            % (time.time() - self._t0, self._frame,
               int(self.sgv.repaint_count), self._repaints_since_sim(),
               int(self.sysref is not None), int(sim is not None),
               ("none" if sgs is None else ("str" if isinstance(sgs, str) else "obj")),
               nb,
               int(self.frtg.width) if self.frtg else -1,
               int(self.frtg.height) if self.frtg else -1,
               tex0, mine, theirs, int(mine == theirs and mine != "-")),
            flush=True)

    def _fail(self, why):
      print("LEG %s FAILED: %s" % (leg, why), flush=True)
      self.result = False
      self._exit_code = 1
      self.ezapp.signalExit()

    def onGpuPostFrame(self, ctx):
      self._frame += 1
      if self.result is not None:
        return
      if self._fatal is not None:
        self._fail(self._fatal)
        return
      if diag:
        now = time.time()
        if now >= self._diag_next:
          self._diag_next = now + 1.0
          self._diagLine()
        if (now - self._t0) > DIAG_SECONDS:
          print("DIAG done (no capture attempted)", flush=True)
          self.result = True
          self._exit_code = 0
          self.ezapp.signalExit()
        return
      if self._inflight is None:
        if not self._capture_ready():
          # bounded wait in WALL time (repaint rate is not a frame rate), then a
          # NAMED failure. Never a fallback surface, never a blind attempt.
          started = self._sim_start_t
          if started is None:
            if (time.time() - self._t0) > READY_TIMEOUT_S:
              self._fail("sim never started (frame=%d)" % self._frame)
            return
          if (time.time() - started) > READY_TIMEOUT_S:
            self._fail("composite target never became capturable: "
                       "repaints=%d (want %d) buffers=%d frames=%d"
                       % (self._repaints_since_sim(), SETTLE_REPAINTS,
                          int(self.frtg.numBuffers if self.frtg else -1),
                          self._frame))
          return
        # THE VIEWPORT DOES NOT COMPOSITE INTO OUR TARGET. SceneGraphViewport::DoDraw
        # temporarily SWAPS the technique's output node for its OWN
        # RtGroupOutputCompositingNode(_rtgroup) for the duration of the draw
        # (viewport_scenegraph.cpp:152), and ui::Surface::_doGpuInit hard-codes that
        # rtgroup to RGBA8 (surface.cpp:156). So the viewport render can never reach a
        # float surface. Render the scene ONCE MORE through its OWN compositor — which
        # still carries the outputRTG node we installed — straight into our RGBA32F
        # target. This also realizes the target's vulkan buffer.
        self.sg.renderOnContext(ctx)
        if not self._realized:
          # REALIZE THE TARGET, don't hope it is realized. rtGroupInit is the
          # pybound PushRtGroup+PopRtGroup pair (pyext_gfx.cpp:191-196) — the
          # same operation the output node's composite performs, which is what
          # builds the vulkan RtBuffer impl that captureAsFormat asserts on
          # (vulkan_fbi_capture.cpp:402). Issued from onGpuPostFrame, which the
          # stack says runs inside EzTopWidget::DoDraw, i.e. in-frame.
          ctx.FBI.rtGroupInit(self.frtg)
          self._realized = True
          print("[star-splat] rtGroupInit issued (frame=%d repaints=%d)"
                % (self._frame, self._repaints_since_sim()), flush=True)
          return
        buf = lev2.CaptureBuffer()
        fut = ctx.FBI.captureAsFormat(self.frtg.buffer(0), buf, "RGBA32F")
        self._inflight = (fut, buf)
        return
      fut, buf = self._inflight
      if not bool(fut.is_ready):
        return
      w, h = buf.width, buf.height
      if (w, h) != (width, height):
        # a resize behind our back means the sums are over different framings
        self._fail("capture is %dx%d, asked for %dx%d"
                   % (w, h, width, height))
        return
      img = numpy.array(buf, dtype=numpy.float32).reshape(h, w, 4)[..., :3].copy()
      self._measure(img, w, h)

    def _measure(self, img, w, h):
      lum = img.max(axis=2)
      peak = float(lum.max())
      # the refusal signature: out-of-gamut magenta (r == b, g == 0) over a whole
      # quad. Clamps to (1,0,1) if the capture path turns out to be 8-bit, so the
      # test is written to survive either regime.
      r, g, b = img[..., 0], img[..., 1], img[..., 2]
      refusal_px = int(((r >= 0.99) & (b >= 0.99) & (g <= 0.02)).sum())
      # background: with a black sky and one star this is 0; subtracting it keeps
      # an accidental ambient from being counted as starlight.
      bg = float(numpy.median(lum))
      total = float(lum.sum()) - bg * w * h
      # the footprint window: angular-equivalent at both resolutions (0.9 deg
      # half-width), centered on the brightest pixel.
      cy, cx = numpy.unravel_index(int(lum.argmax()), lum.shape)
      win = max(8, int(round(0.9 / (FOV_DEG / float(h)))))
      y0, y1 = max(0, cy - win), min(h, cy + win + 1)
      x0, x1 = max(0, cx - win), min(w, cx + win + 1)
      sub = lum[y0:y1, x0:x1]
      wsum = float(sub.sum()) - bg * sub.size
      print("LEG %s %dx%d sum=%.8e wsum=%.8e peak=%.6f refusal_px=%d "
            "at=(%d,%d) win=%d bg=%.3e frames=%d repaints=%d"
            % (leg, w, h, total, wsum, peak, refusal_px, cx, cy, win, bg,
               self._frame, self._repaints_since_sim()),
            flush=True)
      self.result = True
      self._exit_code = 0
      self.ezapp.signalExit()

  app = ChildApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  if app.result is None:
    print("LEG %s FAILED: loop exited before the capture completed" % leg, flush=True)
    return 1
  return app._exit_code


################################################################################
# PARENT
################################################################################

def _spawn(width, height, leg, scripts_dir):
  cmd = [sys.executable, os.path.abspath(__file__), "--child",
         "--width", str(width), "--height", str(height), "--leg", leg]
  print("[star-splat] %s %dx%d" % (leg, width, height), flush=True)
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=CHILD_TIMEOUT)
  sys.stdout.write(p.stdout)
  if p.returncode != 0:
    sys.stdout.write(p.stderr)
  out = {}
  for line in p.stdout.splitlines():
    if line.startswith("LEG %s " % leg):
      for tok in line.split():
        if "=" in tok:
          k, v = tok.split("=", 1)
          try:
            out[k] = float(v)
          except ValueError:
            out[k] = v
  return out


def main():
  from ork.testing import verdict
  scripts_dir = os.environ.get("STARSPLAT_SCRIPTS") or _scripts_dir()
  shots = {}
  for (w, h) in (COARSE, FINE):
    for leg in _legs():
      shots[(leg, w, h)] = _spawn(w, h, leg, scripts_dir)
  # the field case needs only the shipped configuration: does a 640x360 window
  # draw STARS (and the same total energy), or a refusal?
  shots[("base", FIELD[0], FIELD[1])] = _spawn(FIELD[0], FIELD[1], "base",
                                               scripts_dir)

  failures = []

  def check(name, ok, detail=""):
    print("  [%s] %s %s" % ("PASS" if ok else "FAIL", name, detail), flush=True)
    if not ok:
      failures.append(name)

  def got(leg, res, key):
    d = shots.get((leg, res[0], res[1]), {})
    return d.get(key)

  ##########################################################
  # every leg produced a measurement at all
  ##########################################################
  for key, d in shots.items():
    check("captured_%s_%dx%d" % key, "sum" in d, repr(sorted(d)))
  if failures:
    return verdict(False, "one or more legs produced no measurement: "
                          + ",".join(failures))

  ##########################################################
  # the star is on screen, unclipped, and the window holds it
  ##########################################################
  for res, legs_here in ((COARSE, ("base", "double", "cross")),
                         (FINE,   ("base", "double", "cross")),
                         (FIELD,  ("base",))):
    for leg in legs_here:
      pk = got(leg, res, "peak")
      check("signal_%s_%d" % (leg, res[1]), pk > 0.02, "peak=%.4f" % pk)
      check("unclipped_%s_%d" % (leg, res[1]), pk < 0.98, "peak=%.4f" % pk)
      s, ws = got(leg, res, "sum"), got(leg, res, "wsum")
      check("window_holds_the_footprint_%s_%d" % (leg, res[1]),
            s > 0.0 and ws >= 0.99 * s, "window=%.6e frame=%.6e" % (ws, s))

  ##########################################################
  # the sky is BLACK: the frame must contain starlight and nothing else, or the
  # sum is measuring a skybox. Named here so a scenegraph-params regression
  # (e.g. the preset falling back to the default catalog skybox) reads as itself
  # instead of as an energy mismatch.
  ##########################################################
  for (leg, w, h) in sorted(shots):
    bg = shots[(leg, w, h)].get("bg")
    check("background_is_black_%s_%d" % (leg, h), bg is not None and bg <= 1.0e-3,
          "median=%s" % (bg,))

  ##########################################################
  # LINEARITY of the capture path — without this the sums are meaningless
  ##########################################################
  for res in (COARSE, FINE):
    a, b = got("base", res, "wsum"), got("double", res, "wsum")
    ratio = (b / a) if a else 0.0
    check("capture_is_linear_%d" % res[1], abs(ratio - 2.0) <= 2.0 * LINEARITY_TOL,
          "double/base=%.4f (want 2.000)" % ratio)

  ##########################################################
  # ENERGY: the same star integrates to the same total at both resolutions
  ##########################################################
  for leg in ("base", "cross"):
    lo, hi = got(leg, COARSE, "wsum"), got(leg, FINE, "wsum")
    rel = abs(hi - lo) / max(abs(lo), 1e-12)
    check("energy_invariant_%s" % leg, rel <= SUM_TOL,
          "%dx%d=%.6e %dx%d=%.6e rel=%.4f" %
          (COARSE[0], COARSE[1], lo, FINE[0], FINE[1], hi, rel))
  # the field window carries the same star energy as the other two
  fld, ref = got("base", FIELD, "wsum"), got("base", FINE, "wsum")
  rel = abs(fld - ref) / max(abs(ref), 1e-12)
  check("energy_invariant_base_field", rel <= SUM_TOL,
        "%dx%d=%.6e %dx%d=%.6e rel=%.4f" %
        (FIELD[0], FIELD[1], fld, FINE[0], FINE[1], ref, rel))

  ##########################################################
  # the cross leg really straddled the floor (else it retests `base`)
  ##########################################################
  plo, phi = got("cross", COARSE, "peak"), got("cross", FINE, "peak")
  pr = max(plo, phi) / max(min(plo, phi), 1e-12)
  check("cross_leg_straddles_the_floor", pr >= CROSS_PEAK_MIN_RATIO,
        "peaks %.4f vs %.4f (ratio %.2f)" % (plo, phi, pr))

  ##########################################################
  # THE CLIPPING CHECK: an envelope overflow REFUSES, and only then
  ##########################################################
  # THE FIELD REGRESSION, by name: 640x360 must draw stars, not a magenta sky.
  check("no_refusal_in_the_640x360_field_case",
        got("base", FIELD, "refusal_px") == 0,
        "refusal_px=%d at %.4f deg/px"
        % (int(got("base", FIELD, "refusal_px")), FOV_DEG / float(FIELD[1])))
  for res in (COARSE, FINE):
    check("refusal_fires_on_overflow_%d" % res[1],
          got("overflow", res, "refusal_px") > 0,
          "refusal_px=%d" % int(got("overflow", res, "refusal_px")))
    for leg in ("base", "double", "cross"):
      check("no_refusal_%s_%d" % (leg, res[1]),
            got(leg, res, "refusal_px") == 0,
            "refusal_px=%d" % int(got(leg, res, "refusal_px")))

  ok = (len(failures) == 0)
  detail = ("base %.4e/%.4e cross %.4e/%.4e" %
            (got("base", COARSE, "wsum"), got("base", FINE, "wsum"),
             got("cross", COARSE, "wsum"), got("cross", FINE, "wsum")))
  if failures:
    detail += " failed=" + ",".join(failures)
  return verdict(ok, detail)      # returns 0 pass / 1 fail


def _scripts_dir():
  here = os.path.dirname(os.path.abspath(__file__))
  root = os.path.abspath(os.path.join(here, "..", "..", "..", ".."))
  return os.path.join(root, "obt.project", "scripts")


if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--child", action="store_true")
  ap.add_argument("--width", type=int, default=COARSE[0])
  ap.add_argument("--height", type=int, default=COARSE[1])
  ap.add_argument("--leg", default="base")
  ap.add_argument("--diag", action="store_true",
                  help="single-leg readiness PROBE: never captures, prints the "
                       "observable readiness state once a second")
  args = ap.parse_args()
  if args.child:
    _sd = os.environ.get("STARSPLAT_SCRIPTS") or _scripts_dir()
    sys.exit(run_child(args.width, args.height, args.leg, _sd, diag=args.diag))
  sys.exit(main())
