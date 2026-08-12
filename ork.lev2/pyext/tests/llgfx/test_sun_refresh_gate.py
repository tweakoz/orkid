#!/usr/bin/env ork.python
################################################################################
# Gate — the sun-cascade REFRESH GATE only refits when a PREMISE MOVED.
#
# The field defect (owner measurement, 2026-08-05): with a frozen sun and a
# parked viewer the cascade still refit four times a second — a full band fit,
# shadow cull and four depth passes, 1.9 ms of it on a PAUSED frame — because
# the gate carried a wall-clock ceiling armed by default. The ceiling is the
# CASTERS' clock (wind in a canopy moves no premise the gate can see), and the
# ruling is that the shimmer it buys is not worth the frame: the held snapshot
# is reused for as long as sun, viewer and caster set all stand still.
#
# This is a question about WHEN work happens, so the observable is the engine's
# own ORKID_SUNSNAP_TRACE START lines off fd 1, parsed at verdict time. Four
# phases in one process, each with a FROZEN sun over a scene that never streams
# anything in — the only variable is the camera and the three refresh knobs:
#
#   1. FROZEN  (defaults): nothing moves -> ZERO starts. The whole ruling.
#   2. MOVE    (defaults): the camera jumps 1 m, twice the 0.5 m threshold ->
#      exactly one start, then silence again.
#   3. CEILING (max_secs declared): a scene that WANTS the wall clock back gets
#      it — starts land at the declared period with the premises still frozen.
#   4. LEGACY  (all three at 0): the disarmed gate, which is the pre-gate path —
#      a refit every frame. That case is keyed on ALL THREE being zero, which is
#      why a zero ceiling alone reads as "no ceiling" and not as "no gate".
#
# The gate does not engage until the compositor has run k_gate_warm_frames (60)
# — a drawable exists several frames before it DRAWS — so a warm-up phase runs
# first and is not measured.
#
# LIFECYCLE: hand-rolled rather than ork.testing's capture_app, and deliberately
# so — this gate captures no image, runs several time-paced phases across one
# process, and needs the whole run's stdout. It mirrors the sibling sun gates'
# ComponentizedApplication lifecycle, verdict-before-teardown protocol included.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time, re, tempfile

# prepend THIS checkout's scripts dir so ork.testing / ork.app resolve from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine.core import vec3, CrcStringProxy  # core FIRST
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

FRAME_PACE  = 1.0 / 60.0  # THROTTLED: the paced-loop case, not a free-running one
WARM_SECS   = 2.5         # > the 60-frame warm-up at this pace
FROZEN_SECS = 2.0
MOVE_SECS   = 1.0
CEIL_SECS   = 1.6
LEGACY_SECS = 0.6

CEILING_PERIOD = 0.3      # the declared wall clock for phase 3
CEILING_TOL    = 0.10     # seconds

CAM_JUMP_M = 1.0          # 2x the 0.5 m default viewer-travel threshold
CASCADES   = 4

_RE_MARK  = re.compile(r"\[gate\] PHASE (\w+) (open|close)")
_RE_START = re.compile(r"\[sunsnap\] frame<(\d+)> t<([0-9.]+)> START ")
_RE_FRAME = re.compile(r"\[gate\] FRAME")


def _parse(path):
  """Split the trace into phases by the gate's OWN marker lines. Both streams go
  to fd 1 and both are flushed per line, so file ORDER is the phase attribution
  — no mapping between the engine's trace clock and the gate's wall clock."""
  phases = {}
  cur = None
  with open(path, "r", errors="replace") as f:
    for line in f:
      m = _RE_MARK.search(line)
      if m:
        name, which = m.group(1), m.group(2)
        cur = phases.setdefault(name, {"starts": [], "frames": 0}) if which == "open" else None
        continue
      if cur is None:
        continue
      if _RE_FRAME.search(line):
        cur["frames"] += 1
        continue
      m = _RE_START.search(line)
      if m:
        cur["starts"].append({"frame": int(m.group(1)), "t": float(m.group(2))})
  return phases


class GateApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._done = False
    self._phase = 0
    self._phase_t0 = None
    self._want_verdict = False
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

    L = lev2.DynamicDirectionalLight()
    L.data.color = vec3(1, 1, 1)
    L.data.intensity = 4.0
    L.data.shadowBias = 0.05  # metres
    L.data.shadowMapSize = 2048
    L.data.shadowCascadeCount = CASCADES
    L.data.shadowMaxDistance = 250.0
    L.data.pcfDither = 1.0
    L.data.sky_body = 1
    L.data.priority = 10.0
    # THE RIG UNDER TEST is the engine's own defaults: no declared interval (so
    # the refresh gate is what decides), angle 0.1 deg, distance 0.5 m, ceiling
    # OFF. Nothing here restates them — a gate that re-declared the defaults
    # would pass whatever they became.
    L.shadowCaster = True
    self.sun = L
    self.sun.lookAt(vec3(30, 50, 20), vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun_node = SGC.layer_fwd.createLightNode("sun", self.sun)
    self._park(0.0)

    lmgr = SGC.scenegraph.lightingmanager
    lmgr.gpuInit(ctx)

  def _park(self, offset_m):
    """Plant the camera at a fixed pose. Called on the UPDATE thread only — the
    camera is that thread's to write, and driving it from onGpuPostFrame races
    updateScene."""
    SGC = self.SGC
    SGC.uicam.lookAt(vec3(5 + offset_m, 8, 10),
                     vec3(offset_m, 2, 0),
                     vec3(0, 1, 0))
    SGC.uicam.updateMatrices()
    SGC.camera.copyFrom(SGC.uicam.cameradata)

  def _onUpdate(self, updinfo):
    if self._done or not self._built:
      return
    now = time.monotonic()
    if self._phase_t0 is None:
      self._phase_t0 = now
      self._mark("warm", "open")
      return
    elapsed = now - self._phase_t0
    if self._phase == 0 and elapsed >= WARM_SECS:
      self._mark("warm", "close")
      self._phase, self._phase_t0 = 1, now
      self._mark("frozen", "open")
    elif self._phase == 1 and elapsed >= FROZEN_SECS:
      self._mark("frozen", "close")
      self._phase, self._phase_t0 = 2, now
      self._mark("move", "open")
      self._park(CAM_JUMP_M)   # ONE jump, twice the threshold, then still again
    elif self._phase == 2 and elapsed >= MOVE_SECS:
      self._mark("move", "close")
      self.sun.data.shadowRefreshMaxSecs = CEILING_PERIOD
      self._phase, self._phase_t0 = 3, now
      self._mark("ceiling", "open")
    elif self._phase == 3 and elapsed >= CEIL_SECS:
      self._mark("ceiling", "close")
      self.sun.data.shadowRefreshMaxSecs   = 0.0
      self.sun.data.shadowRefreshAngleDeg  = 0.0
      self.sun.data.shadowRefreshDistance  = 0.0
      self._phase, self._phase_t0 = 4, now
      self._mark("legacy", "open")
    elif self._phase == 4 and elapsed >= LEGACY_SECS:
      self._mark("legacy", "close")
      self._want_verdict = True

  def _verdict(self, trace_path):
    """Restore fd 1 FIRST (the verdict line is the harness's only channel and it
    would otherwise land in the trace file), then read the whole run back."""
    from ork.testing import verdict
    self._restore_stdout()
    phases = _parse(trace_path)
    results = []
    ok = True

    def _ph(name):
      return phases.get(name, {"starts": [], "frames": 0})

    # ---- 1: frozen premises, engine defaults -> NO WORK AT ALL ----
    ph = _ph("frozen")
    n = len(ph["starts"])
    sub = (n == 0)
    ok = ok and sub
    results.append("frozen(%.1fs, %d frames, sun+camera+casters still): starts=%d (want 0) %s"
                   % (FROZEN_SECS, ph["frames"], n, "PASS" if sub else "FAIL"))

    # ---- 2: one 1 m camera jump -> exactly one refresh ----
    ph = _ph("move")
    n = len(ph["starts"])
    sub = (n == 1)
    ok = ok and sub
    results.append("move(one %.1fm jump, threshold 0.5m): starts=%d (want 1) %s"
                   % (CAM_JUMP_M, n, "PASS" if sub else "FAIL"))

    # ---- 3: a DECLARED ceiling still runs the wall clock ----
    ph = _ph("ceiling")
    starts = ph["starts"]
    deltas = [round(b["t"] - a["t"], 3) for a, b in zip(starts, starts[1:])]
    sub = (len(deltas) >= 2) and all(abs(d - CEILING_PERIOD) <= CEILING_TOL for d in deltas)
    ok = ok and sub
    results.append("ceiling(declared %gs, premises still): starts=%d start-to-start=%s tol=%g %s"
                   % (CEILING_PERIOD, len(starts), deltas, CEILING_TOL, "PASS" if sub else "FAIL"))

    # ---- 4: all three at zero = the disarmed gate = every frame ----
    ph = _ph("legacy")
    n, f = len(ph["starts"]), ph["frames"]
    sub = (f > 0) and (n >= int(0.8 * f))
    ok = ok and sub
    results.append("legacy(all three thresholds 0): starts=%d of %d frames (want >= 80%%) %s"
                   % (n, f, "PASS" if sub else "FAIL"))

    self._verdict_code = verdict(ok, "sun refresh gate | " + " | ".join(results))

  def _restore_stdout(self):
    """Idempotent: put fd 1 back on the console. Called from the verdict (so the
    verdict line reaches the harness) and again from main's finally (so a run
    that never reached the verdict still prints)."""
    fd = getattr(self, "_saved_fd", None)
    if fd is None:
      return
    self._saved_fd = None
    sys.stdout.flush()
    tf = getattr(self, "_trace_file", None)
    if tf is not None:
      tf.flush()
    os.dup2(fd, 1)
    os.close(fd)

  def _mark(self, name, which):
    """A phase boundary written straight to fd 1, so it interleaves with the
    engine's own flushed trace lines and file ORDER is the phase attribution."""
    os.write(1, ("[gate] PHASE %s %s\n" % (name, which)).encode())

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if self._frame >= 8:
        self._built = True
      return
    # the composited frame is the unit the gate counts against — one line per
    # frame, in the same stream, so "starts per frame" is read off file order.
    os.write(1, b"[gate] FRAME\n")
    time.sleep(FRAME_PACE)  # THROTTLE — an unthrottled loop is the blind spot
    if self._want_verdict:
      self._verdict(self._trace_path)
      self._done = True
      self.ezapp.signalExit()


def main():
  from ork.testing import Watchdog, verdict
  os.environ["ORKID_SUNSNAP_TRACE"] = "1"

  trace_path = os.path.join(tempfile.mkdtemp(prefix="sunrefresh_trace_"), "sunsnap.log")
  # the trace is C printf on fd 1, so the DESCRIPTOR is what has to be captured —
  # redirecting sys.stdout would leave every engine-side line going to the console
  # and nothing to parse.
  sys.stdout.flush()
  saved_fd = os.dup(1)
  tf = open(trace_path, "w")
  os.dup2(tf.fileno(), 1)

  wd = Watchdog(240.0, label="sun_refresh_gate").arm()
  app = None
  try:
    app = GateApp()
    app._trace_path = trace_path
    app._saved_fd = saved_fd
    app._trace_file = tf
    app.ezapp.mainThreadLoop()
  finally:
    if app is not None:
      app._restore_stdout()
    else:
      os.dup2(saved_fd, 1)
      os.close(saved_fd)
  wd.disarm()

  code = getattr(app, "_verdict_code", None) if app else None
  if code is None:
    code = verdict(False, "loop exited before the phases completed (trace at %s)" % trace_path)
  app.ezapp.shutdown()
  sys.exit(code)


if __name__ == "__main__":
  main()
