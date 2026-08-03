#!/usr/bin/env ork.python
################################################################################
# PAUSE-MUST-PUBLISH gate (procsky wave4 slice W4-S1b).
#
# THE CLAIM UNDER TEST: the sky's publish chain and the luminance it measures
# live on the RENDER PROLOGUE, not on the simulation tick. Put the sim in the
# GENUINE PAUSE state — Controller::pauseSimulation, a clock hold: events keep
# servicing, the renderer keeps drawing, gameTime stops — and the environment
# the shading reads must stay a real published environment, with a valid
# measured luminance, for as long as the pause lasts.
#
# WHY IT MATTERS. A paused game still draws. If publishing or measuring rode
# the tick, the first pause would either freeze the feed at whatever it last
# had (survivable) or hand the shading an UNMEASURED environment — luminance
# negative or zero — which the scene adaptation reads as a black world and
# grades to its floor. The pause would look like a bug in the sky.
#
# NOT stopSimulation. That is the EDIT-mode teardown path (deactivate, event
# queues cleared, transport barrier); using it here would test a different
# mechanism and would prove nothing about a pause.
#
# THE THREE WINDOWS, each measured in wall time so the wall-clock rebake
# cadence has room to fire, and each summarized per PUBLISHED GENERATION:
#
#   RUN    the sim ticking. The celestial sun sweeps (time_scale below), so
#          successive publishes carry DIFFERENT skies: the run window's
#          luminance spread is the gate's SENSITIVITY PROOF — without it, a
#          frozen-looking pause window would mean nothing. The window stays
#          open until that spread is there, so a slow boot costs seconds
#          rather than a verdict.
#   PAUSE  pauseSimulation(). Generations keep coming (declared cadence is a
#          wall clock, ibl_snapshot_interval), and every one of them must
#          carry a VALID luminance. The sun is frozen by the pause, so the
#          published sky is the same sky every cycle and the spread collapses
#          to zero — which is the evidence that the pause really engaged and
#          not that the sky stopped being rendered.
#   RESUME resumeSimulation(). Frames stay clean and the feed keeps publishing
#          a valid luminance; the sun moving again is not re-asserted here (the
#          run window already proved the sensitivity).
#
# WHAT IS ASSERTED vs WHAT IS RECORDED. Validity is asserted in every window.
# Generation ADVANCEMENT while paused is asserted too — it is the load-bearing
# half of the claim (a feed that merely kept its last publish would satisfy
# "valid" while having stopped), and it was deterministic on three consecutive
# mac runs at the time of writing: 8 fresh cycles inside each 2.5s pause, with
# the settled ones byte-identical (spread exactly 0).
#
# WARMUP PINNED as the sibling sky gates do: ibl_crossfade_frames 0 and the
# fade weight asserted at 1.0 before the first reading, so no reading straddles
# two cycles' maps.
#
# BOOT: the SceneGraphViewport + ecs.Controller shape (as
# test_star_splat_energy_gate.py), not ork.testing's capture_app — the harness
# hosts one PrimCanvas app and cannot run an ECS controller, and an ECS
# controller is exactly what has to be paused. startSimulation is deferred into
# onUpdate for the reason that gate documents (the ECS gpu-init rendezvous
# draining outside a frame null-derefs LightManager::gpuInit).
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import time
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from orkengine import ecs
from ork.testing import verdict
from ork.testing.watchdog import Watchdog

tokens = CrcStringProxy()

WIDTH, HEIGHT = 256, 192
CAM_NAME = "spawncam"              # the only camera name SceneGraphSystem publishes

SIM_START_FRAME = 10               # drawn frames before startSimulation (see boot note)
SNAPSHOT_INTERVAL = 0.05           # declared rebake cadence: a WALL clock, not the sim's

# THE SKY CLOCK, and why it is not faster. The sim clock starts at
# startSimulation and runs through however long the boot to the first publish
# takes — measured here at 2 to 11 wall seconds, machine-load dependent. At
# 3600x (one sim hour per wall second) that slop alone walked the sun past
# sunset on a loaded machine, into the moonless night where the measured
# luminance is EXACTLY constant and the sensitivity leg below has nothing to
# read. 600x (ten sim minutes per wall second) puts the same slop under two sim
# hours, so a 7am start is still a rising morning sun when the windows run.
TIME_OF_DAY  = 7.0
TIME_SCALE   = 600.0
LATITUDE_DEG = 45.0
DAY_OF_YEAR  = 220.0

WINDOW_SECS  = 2.5                 # per window (the run window extends, see below)
MIN_GENS     = 3                   # published cycles a window must collect
# The cycle already IN FLIGHT when the pause lands froze its source BEFORE the
# pause, so the first publish after pauseSimulation() still carries a moving
# sun. That one reading is dropped from the frozen-sky check (never from the
# validity check) — the same "wait for a FRESH publish" discipline the sibling
# luminance gate applies after a sky edit. Cadence gates on fadeSettled(), so
# at most ONE cycle can be in flight; a second would fail the check loudly
# rather than be absorbed.
PAUSE_SETTLE_GENS = 1
RESUME_SECS  = 1.5
RESUME_GENS  = 2
WAIT_SECONDS = 60.0                # ceiling on first publish / on a window filling

# The run window must MOVE (sensitivity), the pause window must be FROZEN (the
# pause engaged). Both are read as relative spread over the window's generations.
# The run window is NOT a fixed span: it stays open until the sky has moved by
# RUN_SPREAD_MIN, so a slow boot that leaves the sun somewhere flatter costs
# time instead of costing a verdict, and a sky that never moves at all fails by
# its own name rather than as a mystery threshold miss. The bar is small
# BECAUSE the pause tail comes back EXACTLY frozen — the contrast the gate
# reads is "moves at all" against "spread is literally zero", not a ratio.
RUN_SPREAD_MIN   = 0.005           # 0.5%
PAUSE_SPREAD_MAX = 1.0e-6          # a frozen sun republishes the identical sky

CAM_EYE = vec3(0.0, 2.0, -12.0)
CAM_TGT = vec3(0.0, 6.0, 0.0)


def _spread(vals):
  """relative spread of a window's per-generation luminances (0 = frozen)."""
  if len(vals) < 2:
    return 0.0
  lo, hi = min(vals), max(vals)
  return (hi - lo) / hi if hi > 0.0 else 0.0


class PauseGateApp:

  def __init__(self):
    self.ezapp = lev2.OrkEzApp.createEx(
        self, [ecs.ecsInitCallback], name="pause_publish_gate",
        width=WIDTH, height=HEIGHT, offscreen=True, ssaa=0)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, -1)
    self.ezapp.topWidget.enableUiDraw()
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    self.sgv = lg.makeChild(fill=True, margin=0,
                            uiclass=lev2.ui.SceneGraphViewport,
                            args=["Viewport", vec4(0, 0, 0, 1)]).widget
    self.sgv.cameraName = CAM_NAME
    self.ctrl = None
    self.sysref = None
    self.sg = None
    self._frame = 0
    self._t0 = time.time()
    self._state = "wait_publish"
    self._state_t = time.time()
    self._last_gen = -1
    self._gens = {"run": [], "pause": [], "resume": []}
    self._G = {}
    self._L = {}
    self._fails = []
    self._done = False
    self._exit_code = 1

  ##############################################################################
  # boot
  ##############################################################################

  def onGpuInit(self, ctx):
    from ork.hypergraph.ecs.scene import Scene
    from ork.hypergraph.ecs.scene.assets import wire_scene_data
    from lev2utils.cameras import setupUiCameraX

    class PauseGateScene(Scene):
      def __init__(self):
        super().__init__()
        # the procsky family's sky, cut to its minimum: procedural source and
        # the celestial SUN (the sim-driven motion the pause has to stop). No
        # moon, no stars, no decks, no terrain — none of them is under test and
        # each costs a bake.
        self.sky(time_of_day  = TIME_OF_DAY,
                 time_scale   = TIME_SCALE,
                 latitude_deg = LATITUDE_DEG,
                 day_of_year  = DAY_OF_YEAR,
                 moon         = False,
                 stars        = False,
                 msaa         = 0,
                 ssaa         = 0)

    sd = ecs.SceneData()
    PauseGateScene().build(sd)
    wire_scene_data(sd, ezapp=self.ezapp)

    self.cameralut = lev2.CameraDataLut()
    self.camera, self.uicam = setupUiCameraX(cameralut=self.cameralut,
                                             camname=CAM_NAME)
    self.uicam.far_max = 200000.0   # the sky shell sits at 30km
    self.uicam.lookAt(CAM_EYE, CAM_TGT, vec3(0, 1, 0))
    self.uicam.updateMatrices()
    self.camera.copyFrom(self.uicam.cameradata)

    sgp = sd.generateSceneGraphParams()
    sg = lev2.scenegraph.Scene(sgp)
    for _lyr in ("std_forward", "std_transparent", "hud_overlay"):
      sg.createLayer(_lyr)
    self.sg = sg
    self.sgv.scenegraph = sg
    self.sgv.forkDB()

    # THE CADENCE THIS GATE NEEDS. Left to the shipped sun-motion trigger, a
    # paused (frozen) sun would produce no further cycles BY DESIGN, and the
    # pause window could say nothing about whether publishing rides the tick.
    # The declared interval is a wall clock, so it keeps asking during a pause —
    # which is what makes "did it publish while paused" answerable at all.
    atmo = sg.pbr_common.atmosphere
    if atmo is None:
      atmo = lev2.SkyAtmosphereData()
      sg.pbr_common.atmosphere = atmo
    atmo.ibl_crossfade_frames  = 0
    atmo.ibl_snapshot_interval = SNAPSHOT_INTERVAL
    sg.pbr_common.sky_source = "procedural"

    self.ctrl = ecs.Controller()
    self.ctrl.bindScene(sd)
    self.ctrl.createSimulation(scenegraph=sg)

  def onGpuUpdate(self, ctx):
    if self.ctrl:
      self.ctrl.gpuUpdate(ctx)

  def onUpdateInit(self):
    # DELIBERATELY EMPTY — startSimulation is deferred to onUpdate; see the
    # header's boot note (and test_star_splat_energy_gate.py, which filed it).
    pass

  def onUpdate(self, updinfo):
    if self.ctrl is None or self._done:
      return
    if self.sysref is None:
      if self._frame < SIM_START_FRAME:
        return
      self.ctrl.startSimulation()
      self.sysref = self.ctrl.findSystem("SceneGraphSystem")
      print("[pause-gate] sim started at frame %d" % self._frame, flush=True)
      return
    UIC = self.uicam.cameradata
    self.ctrl.systemNotify(self.sysref, tokens.UpdateCamera,
                           {tokens.eye: UIC.eye, tokens.tgt: UIC.target,
                            tokens.up: UIC.up, tokens.near: UIC.near,
                            tokens.far: UIC.far, tokens.fovy: UIC.fovy})
    self.ctrl.updateSimulation()
    self.sgv.setDirty()

  ##############################################################################
  # the windows
  ##############################################################################

  def _pbr(self):
    return self.sg.pbr_common

  def _fail(self, why):
    self._fails.append(why)
    self._emitVerdict()

  def _restate(self, s):
    self._state = s
    self._state_t = time.time()

  def _sample(self, bucket):
    """collect one reading PER PUBLISHED GENERATION; returns the generation
    count in this window."""
    pbr = self._pbr()
    gen = int(pbr.sky_ibl_generation)
    if gen != self._last_gen:
      self._last_gen = gen
      lum = float(pbr.sky_measured_luminance)
      fw = float(pbr.sky_ibl_fade_weight)
      self._gens[bucket].append((gen, lum, fw))
    return len(self._gens[bucket])

  def _windowFull(self, bucket, secs, mingens):
    return ((time.time() - self._state_t) >= secs
            and len(self._gens[bucket]) >= mingens)

  def onGpuPostFrame(self, ctx):
    self._frame += 1
    if self._done or self.sysref is None:
      return
    pbr = self._pbr()

    if self._state == "wait_publish":
      if bool(pbr.sky_ibl_ready) and (not bool(pbr.sky_ibl_inflight)):
        fw = float(pbr.sky_ibl_fade_weight)
        if fw < 0.9999:
          return
        self._G["L0_gen"] = int(pbr.sky_ibl_generation)
        self._L["L0"] = float(pbr.sky_measured_luminance)
        print("[pause-gate] first publish: G0=%d L0=%.9g fade=%.4f (frame %d)"
              % (self._G["L0_gen"], self._L["L0"], fw, self._frame), flush=True)
        self._last_gen = -1
        self._restate("run")
      elif (time.time() - self._state_t) > WAIT_SECONDS:
        self._fail("the first procedural publish never landed")
      return

    if self._state == "run":
      self._sample("run")
      run_moved = (_spread([l for (_g, l, _f) in self._gens["run"]]) >= RUN_SPREAD_MIN)
      if self._windowFull("run", WINDOW_SECS, MIN_GENS) and run_moved:
        self._G["run_end"] = int(pbr.sky_ibl_generation)
        # THE PAUSE, issued from the render thread — where a UI pause key lands,
        # and the thread Controller::_transportAtomicOp keeps draining the GPU
        # rendezvous for while it waits on the sim lock.
        t_pause = time.time()
        self.ctrl.pauseSimulation()
        print("[pause-gate] pauseSimulation() returned in %.3f s (frame %d, gen %d)"
              % (time.time() - t_pause, self._frame, self._G["run_end"]), flush=True)
        self._restate("pause")
      elif (time.time() - self._state_t) > WAIT_SECONDS:
        # NO SENSITIVITY, NO GATE: a pause window that looks frozen proves
        # nothing if the running sky was frozen too. Named, never absorbed.
        self._fail("the RUNNING sky never moved: %d publishes in %.0f s, spread "
                   "%.6g (< %.3g) — sun off the sky, or the sim not ticking"
                   % (len(self._gens["run"]), WAIT_SECONDS,
                      _spread([l for (_g, l, _f) in self._gens["run"]]),
                      RUN_SPREAD_MIN))
      return

    if self._state == "pause":
      self._sample("pause")
      if self._windowFull("pause", WINDOW_SECS, MIN_GENS + PAUSE_SETTLE_GENS):
        self._G["pause_end"] = int(pbr.sky_ibl_generation)
        t_res = time.time()
        self.ctrl.resumeSimulation()
        print("[pause-gate] resumeSimulation() returned in %.3f s (frame %d, gen %d)"
              % (time.time() - t_res, self._frame, self._G["pause_end"]), flush=True)
        self._restate("resume")
      elif (time.time() - self._state_t) > WAIT_SECONDS:
        # NOT a timeout dressed as a pass: too few publishes WHILE PAUSED is the
        # defect this gate exists to catch, named as such.
        self._G["pause_end"] = int(pbr.sky_ibl_generation)
        self._fail("PAUSED feed published only %d cycles in %.0f s "
                   "(generation %d -> %d) — the publish chain rides the sim tick"
                   % (len(self._gens["pause"]), WAIT_SECONDS,
                      self._G["run_end"], self._G["pause_end"]))
      return

    if self._state == "resume":
      self._sample("resume")
      if self._windowFull("resume", RESUME_SECS, RESUME_GENS):
        self._G["resume_end"] = int(pbr.sky_ibl_generation)
        self._L["L1"] = float(pbr.sky_measured_luminance)
        self._emitVerdict()
      elif (time.time() - self._state_t) > WAIT_SECONDS:
        self._G["resume_end"] = int(pbr.sky_ibl_generation)
        self._fail("resumed feed published only %d cycles in %.0f s"
                   % (len(self._gens["resume"]), WAIT_SECONDS))
      return

  ##############################################################################

  def _emitVerdict(self):
    if self._done:
      return

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        self._fails.append(label)

    for bucket in ("run", "pause", "resume"):
      print("=== %s window: %d published cycles ===" % (bucket, len(self._gens[bucket])),
            flush=True)
      for gen, lum, fw in self._gens[bucket]:
        print("  gen %6d  luminance %.9g  fade %.4f" % (gen, lum, fw), flush=True)

    L0 = self._L.get("L0", -1.0)
    check("first_publish_measured", L0 > 0.0, "L0=%.9g" % L0)

    run = [l for (_g, l, _f) in self._gens["run"]]
    pau = [l for (_g, l, _f) in self._gens["pause"]]
    res = [l for (_g, l, _f) in self._gens["resume"]]

    if run and pau:
      # SENSITIVITY: the run window has to MOVE, or a frozen pause window is
      # not evidence of anything.
      run_spread = _spread(run)
      check("run_window_moves", run_spread >= RUN_SPREAD_MIN,
            "spread=%.6g (>= %.3g)" % (run_spread, RUN_SPREAD_MIN))

      # THE CLAIM: publishing continued while PAUSED...
      g_pause_delta = self._G.get("pause_end", 0) - self._G.get("run_end", 0)
      check("published_while_paused", len(pau) >= MIN_GENS and g_pause_delta >= MIN_GENS,
            "%d cycles, generation %d -> %d"
            % (len(pau), self._G.get("run_end", -1), self._G.get("pause_end", -1)))

      # ...and every one of those publishes measured a REAL environment. A
      # negative reading is "unmeasured", a zero is the stale-black the scene
      # adaptation would grade to its floor; both are the failure this names.
      check("paused_luminance_valid", all(l > 0.0 for l in pau),
            "min=%.9g max=%.9g" % (min(pau), max(pau)))

      # THE PAUSE ENGAGED: the sun is frozen, so every publish AFTER the
      # in-flight one republishes the SAME sky and the spread collapses.
      # Without this, a gate that never actually paused would still pass
      # everything above.
      tail = pau[PAUSE_SETTLE_GENS:]
      pause_spread = _spread(tail)
      check("pause_froze_the_sky", len(tail) >= MIN_GENS and pause_spread <= PAUSE_SPREAD_MAX,
            "%d settled cycles, spread=%.6g (<= %.3g)"
            % (len(tail), pause_spread, PAUSE_SPREAD_MAX))
    else:
      check("windows_collected", False,
            "run=%d pause=%d" % (len(run), len(pau)))

    if res:
      L1 = self._L.get("L1", -1.0)
      check("resumed_frames_clean", all(l > 0.0 for l in res) and L1 > 0.0,
            "%d cycles, L1=%.9g" % (len(res), L1))
    else:
      check("resumed_frames_clean", False, "no publish after resume")

    ok = (len(self._fails) == 0)
    detail = ("L0=%.6g L1=%.6g G0=%d G_run_end=%d G_pause_end=%d G_resume_end=%d"
              % (self._L.get("L0", -1.0), self._L.get("L1", -1.0),
                 self._G.get("L0_gen", -1), self._G.get("run_end", -1),
                 self._G.get("pause_end", -1), self._G.get("resume_end", -1)))
    if self._fails:
      detail += " failed=" + ",".join(self._fails)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  # BOUND THE WEDGE. Every window above already fails itself on WAIT_SECONDS, but
  # those clocks are read from the render callback — a gate that hangs BELOW that
  # (the GIL-mode ECS sub-interpreter deadlock, see test_gil_ecs_regression.py)
  # never reaches them and used to hang forever with no evidence. This samples the
  # wedged process and exits WATCHDOG_RC. Ceiling = the four windows at their own
  # WAIT_SECONDS ceilings plus boot; a passing run disarms it in a fraction of it.
  wd = Watchdog(4 * WAIT_SECONDS + 60.0, label="pause_publish_gate").arm()
  app = PauseGateApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  wd.disarm()
  if not app._done:
    return verdict(False, "loop exited before the gate reached a verdict")
  return app._exit_code


if __name__ == "__main__":
  sys.exit(main())
