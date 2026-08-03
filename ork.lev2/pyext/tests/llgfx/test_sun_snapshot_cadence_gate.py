#!/usr/bin/env ork.python
################################################################################
# Gate — the DECLARED sun-cascade snapshot interval IS the cadence, and the flip
# crossfade always runs to its end.
#
# The field defect (owner report, 2026-07-30): a scene declaring a 60 second
# shadow cadence still refit roughly once a second, because the fit ALSO had
# drift triggers — light-angle delta and a viewer-translation deadband — that a
# 480x sky clock trips continuously. The declared number was advisory. The
# triggers are gone; the interval is the whole cadence, and only a STRUCTURAL
# change (caster flip, rig edit) may pre-empt it.
#
# The same window had three crossfade defects, all of which this gate reads off
# the same trace: the fade was sized in FRAMES (24 ms unthrottled, 200 ms at
# 60 Hz — the blend duty was a function of frame rate), a cadence start landing
# inside a fade SNAPPED the weight to 0, and the flip frame published
# remaining/(total+1) = 12/13 instead of 1.0, i.e. a built-in step at every
# publish.
#
# None of that is visible in a still: it is a question about WHEN work happens,
# so the observable is the engine's own ORKID_SUNSNAP_TRACE stream, captured off
# fd 1 (the trace is printf, not python logging) and parsed at verdict time.
# Three phases in one process, each with a SWEEPING sun and a TRANSLATING
# camera — the two motions that used to force a refit — at a THROTTLED ~60 Hz
# pace, because an unthrottled loop is exactly the blind spot that let a
# frame-counted fade look correct:
#   1. HOLD    (interval 60 s): ONE snapshot family, the first-ever structural
#      one, and no cadence start at all while the sun sweeps ~90 degrees.
#   2. CADENCE (interval 2 s): starts land 2.0 s apart on the wall clock, every
#      publish opens at prev-weight 1.0, and every fade decays monotonically to
#      0 without a snap (no drop of more than a third of the window in a frame).
#   3. FLIP    (interval 60 s): handing the cascade to a second caster refits
#      IMMEDIATELY — a fit made for the sun's direction is wrong for the moon's,
#      and the interval must never suppress that.
#
# LIFECYCLE: hand-rolled rather than ork.testing's capture_app, and deliberately
# so — this gate captures no image, runs several time-paced phases across one
# process, and needs the whole run's stdout. capture_app is one settle-capture-
# exit shot; headless_app has no forward compositor to run the cascade prologue
# in. The sibling sun gates (amortize / jitter / band-resolution) use this same
# ComponentizedApplication lifecycle, including the verdict-before-teardown
# protocol (known defect D1: this scene class can SIGSEGV during mainThreadLoop
# teardown AFTER the verdict is flushed).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys, time, re, tempfile

# prepend THIS checkout's scripts dir so ork.testing / ork.app resolve from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine.core import vec3, vec4, CrcStringProxy  # core FIRST
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()

FRAME_PACE   = 1.0 / 60.0  # THROTTLED: the paced-loop case the old gates never covered
HOLD_SECS    = 3.0
CADENCE_SECS = 7.0
FLIP_SECS    = 1.0

HOLD_INTERVAL    = 60.0   # the field declaration: one snapshot a minute
CADENCE_INTERVAL = 2.0    # short enough to measure several periods in one run
CROSSFADE_FRAMES = 12
CROSSFADE_SECS   = 0.5   # > 12 frames at 60Hz: the CLOCK has to be what ends it
BANDS_PER_FRAME  = 1
CASCADES         = 4

SUN_SWEEP_DEG_PER_SEC = 30.0   # ~90 degrees across the hold phase
CAM_DRIFT_M_PER_SEC   = 3.0    # far past any deadband the old gate used

CADENCE_TOL   = 0.20  # seconds; a 60 Hz frame is 0.0167, the fade floor is 0.2
FLIP_W_MIN    = 0.98  # the publish frame's prev weight must be a full 1.0
FADE_STEP_MAX = 0.34  # a larger single-frame drop is a SNAP, not a fade

_RE_MARK    = re.compile(r"\[gate\] PHASE (\w+) (open|close)")
_RE_START   = re.compile(r"\[sunsnap\] frame<(\d+)> t<([0-9.]+)> START .*structural<(\d)> caster<(\d)> rig<(\d)>")
_RE_PUBLISH = re.compile(r"\[sunsnap\] frame<(\d+)> t<([0-9.]+)> PUBLISH .*fade_armed<(\d)> w<([0-9.]+)>")
_RE_FADE    = re.compile(r"\[sunsnap\] frame<(\d+)> t<([0-9.]+)> FADE w<([0-9.]+)>")


def _parse(path):
  """Split the trace into phases by the gate's OWN marker lines. Both streams go
  to fd 1 and both are flushed per line, so file ORDER is the phase attribution
  — no mapping between the engine's trace clock and the gate's wall clock, which
  is the mapping that would otherwise smear every phase boundary."""
  phases = {}
  cur = None
  with open(path, "r", errors="replace") as f:
    for line in f:
      m = _RE_MARK.search(line)
      if m:
        name, which = m.group(1), m.group(2)
        cur = phases.setdefault(name, {"starts": [], "publishes": [], "fades": []}) \
              if which == "open" else None
        continue
      if cur is None:
        continue
      m = _RE_START.search(line)
      if m:
        cur["starts"].append({"frame": int(m.group(1)), "t": float(m.group(2)),
                              "structural": int(m.group(3)), "caster": int(m.group(4)),
                              "rig": int(m.group(5))})
        continue
      m = _RE_PUBLISH.search(line)
      if m:
        cur["publishes"].append({"frame": int(m.group(1)), "t": float(m.group(2)),
                                 "armed": int(m.group(3)), "w": float(m.group(4))})
        continue
      m = _RE_FADE.search(line)
      if m:
        cur["fades"].append({"frame": int(m.group(1)), "t": float(m.group(2)),
                             "w": float(m.group(3))})
  return phases


class GateApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._done = False
    self._phase = 0
    self._phase_t0 = None
    self._marks = {}       # phase name -> (trace t at entry, trace t at exit)
    self._t_wall0 = None
    self._want_verdict = False
    self._sweep = 0.0
    self._drift = 0.0
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

    def _mklight(body, priority, caster):
      L = lev2.DynamicDirectionalLight()
      L.data.color = vec3(1, 1, 1)
      L.data.intensity = 4.0
      L.data.shadowBias = 2e-4
      L.data.shadowMapSize = 2048
      L.data.shadowCascadeCount = CASCADES
      L.data.shadowMaxDistance = 250.0
      L.data.pcfDither = 1.0
      L.data.sky_body = body
      L.data.priority = priority
      L.data.shadowSnapshotInterval      = HOLD_INTERVAL
      L.data.shadowSnapshotBandsPerFrame = BANDS_PER_FRAME
      L.data.shadowCrossfadeFrames       = CROSSFADE_FRAMES
      L.data.shadowCrossfadeSecs         = CROSSFADE_SECS
      L.shadowCaster = caster
      return L

    # two casters, one active: the night-time handoff, which is the one refresh
    # the interval is NOT allowed to suppress.
    self.sun = _mklight(1, 10.0, True)
    self.moon = _mklight(2, 5.0, False)
    self.sun.lookAt(vec3(30, 50, 20), vec3(0, 0, 0), vec3(0, 1, 0))
    self.moon.lookAt(vec3(-30, 50, -20), vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun_node = SGC.layer_fwd.createLightNode("sun", self.sun)
    self.moon_node = SGC.layer_fwd.createLightNode("moon", self.moon)

    lmgr = SGC.scenegraph.lightingmanager
    lmgr.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    """Scene animation + the phase machine, on the UPDATE thread and AFTER the
    scenegraph component's own onUpdate — the camera and the light nodes are
    that thread's to write. Driving them from onGpuPostFrame instead races
    updateScene and segfaults."""
    if self._done or not self._built:
      return
    now = time.monotonic()
    if self._phase_t0 is None:
      self._phase_t0 = now
      self._mark("hold", "open")
      return
    self._sweep_scene(FRAME_PACE)
    elapsed = now - self._phase_t0
    if self._phase == 0 and elapsed >= HOLD_SECS:
      self._mark("hold", "close")
      self.sun.data.shadowSnapshotInterval  = CADENCE_INTERVAL
      self.moon.data.shadowSnapshotInterval = CADENCE_INTERVAL
      self._phase, self._phase_t0 = 1, now
      self._mark("cadence", "open")
    elif self._phase == 1 and elapsed >= CADENCE_SECS:
      self._mark("cadence", "close")
      self.sun.data.shadowSnapshotInterval  = HOLD_INTERVAL
      self.moon.data.shadowSnapshotInterval = HOLD_INTERVAL
      self._phase, self._phase_t0 = 2, now
      self._mark("flip", "open")
      # the night handoff: the sun stops casting, the moon takes the cascade
      self.sun.shadowCaster = False
      self.moon.shadowCaster = True
    elif self._phase == 2 and elapsed >= FLIP_SECS:
      self._mark("flip", "close")
      self._want_verdict = True

  def _sweep_scene(self, dt):
    """A sun sweeping the sky and a camera walking away from the fit anchor —
    the exact pair of motions the retired drift triggers watched for."""
    import math
    self._sweep += SUN_SWEEP_DEG_PER_SEC * dt
    self._drift += CAM_DRIFT_M_PER_SEC * dt
    a = math.radians(self._sweep)
    pos = vec3(50.0 * math.cos(a), 50.0, 50.0 * math.sin(a))
    self.sun.lookAt(pos, vec3(0, 0, 0), vec3(0, 1, 0))
    self.moon.lookAt(vec3(-pos.x, pos.y, -pos.z), vec3(0, 0, 0), vec3(0, 1, 0))
    SGC = self.SGC
    SGC.uicam.lookAt(vec3(5 + self._drift, 8, 10 + self._drift),
                     vec3(self._drift, 2, self._drift),
                     vec3(0, 1, 0))
    SGC.uicam.updateMatrices()
    SGC.camera.copyFrom(SGC.uicam.cameradata)

  def _verdict(self, trace_path):
    """Restore fd 1 FIRST (the verdict line is the harness's only channel and it
    would otherwise land in the trace file), then read the whole run back."""
    from ork.testing import verdict
    self._restore_stdout()
    phases = _parse(trace_path)
    results = []
    ok = True

    # ---- phase 1: the declared hold ----
    ph = phases.get("hold", {"starts": [], "publishes": [], "fades": []})
    starts = ph["starts"]
    cadence_starts = [s for s in starts if not s["structural"]]
    hold_ok = (len(starts) <= 1) and not cadence_starts
    ok = ok and hold_ok
    results.append("hold(interval=%gs, sun swept %.0fdeg, cam moved %.0fm): starts=%d "
                   "structural=%s cadence-forced=%d %s"
                   % (HOLD_INTERVAL, SUN_SWEEP_DEG_PER_SEC * HOLD_SECS,
                      CAM_DRIFT_M_PER_SEC * HOLD_SECS, len(starts),
                      [s["structural"] for s in starts], len(cadence_starts),
                      "PASS" if hold_ok else "FAIL"))

    # ---- phase 2: the declared cadence ----
    ph = phases.get("cadence", {"starts": [], "publishes": [], "fades": []})
    starts = ph["starts"]
    deltas = [round(b["t"] - a["t"], 3) for a, b in zip(starts, starts[1:])]
    cad_ok = (len(deltas) >= 2) and all(abs(d - CADENCE_INTERVAL) <= CADENCE_TOL for d in deltas)
    ok = ok and cad_ok
    results.append("cadence(declared=%gs): starts=%d start-to-start=%s tol=%g %s"
                   % (CADENCE_INTERVAL, len(starts), deltas, CADENCE_TOL,
                      "PASS" if cad_ok else "FAIL"))

    # the flip frame opens at FULL prev weight — the old form published
    # remaining/(total+1), a built-in step at every publish.
    pubs = [p for p in ph["publishes"] if p["armed"]]
    worst_w = min([p["w"] for p in pubs], default=0.0)
    flip_ok = bool(pubs) and worst_w >= FLIP_W_MIN
    ok = ok and flip_ok
    results.append("flip-frame prev weight: n=%d min=%.4f (>= %.2f) %s"
                   % (len(pubs), worst_w, FLIP_W_MIN, "PASS" if flip_ok else "FAIL"))

    # every fade decays monotonically, lands ON 0, and lasts LONGER than its
    # frame count: a cadence start retiring a live fade is a single large step,
    # and a frame-counted window would end in exactly CROSSFADE_FRAMES ticks.
    windows = []
    cur = []
    for f in ph["fades"]:
      cur.append(f)
      if f["w"] <= 0.0:
        windows.append(cur)
        cur = []
    worst_step = 0.0
    monotone = True
    for w in windows:
      for a, b in zip(w, w[1:]):
        step = a["w"] - b["w"]
        if step < -1e-4:
          monotone = False
        worst_step = max(worst_step, step)
    shortest = min([len(w) for w in windows], default=0)
    fade_ok = (len(windows) >= 2) and monotone \
              and (worst_step <= FADE_STEP_MAX) and (shortest > CROSSFADE_FRAMES)
    ok = ok and fade_ok
    results.append("fades: completed=%d ticks/window>=%d (frames knob %d, secs %g) "
                   "monotone=%d worst step=%.3f (<= %.2f) %s"
                   % (len(windows), shortest, CROSSFADE_FRAMES, CROSSFADE_SECS,
                      int(monotone), worst_step, FADE_STEP_MAX,
                      "PASS" if fade_ok else "FAIL"))

    # ---- phase 3: the caster flip ----
    ph = phases.get("flip", {"starts": [], "publishes": [], "fades": []})
    starts = ph["starts"]
    flip_starts = [s for s in starts if s["caster"]]
    caster_ok = len(flip_starts) >= 1
    ok = ok and caster_ok
    results.append("caster flip (sun->moon at interval=%gs): starts=%d caster-forced=%d %s"
                   % (HOLD_INTERVAL, len(starts), len(flip_starts),
                      "PASS" if caster_ok else "FAIL"))

    self._verdict_code = verdict(ok, "sun snapshot cadence gate | " + " | ".join(results))

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
    # the first frames build the scene and take the first-ever (structural)
    # snapshot; the phases open once that settled state is the baseline.
    if not self._built:
      if self._frame >= 8:
        self._built = True
      return
    time.sleep(FRAME_PACE)  # THROTTLE — an unthrottled loop is the blind spot
    if self._want_verdict:
      self._verdict(self._trace_path)
      self._done = True
      self.ezapp.signalExit()


def main():
  from ork.testing import Watchdog, verdict
  os.environ["ORKID_SUNSNAP_TRACE"] = "1"

  trace_path = os.path.join(tempfile.mkdtemp(prefix="sunsnap_trace_"), "sunsnap.log")
  # the trace is C printf on fd 1, so the DESCRIPTOR is what has to be captured —
  # redirecting sys.stdout would leave every engine-side line going to the console
  # and nothing to parse.
  sys.stdout.flush()
  saved_fd = os.dup(1)
  tf = open(trace_path, "w")
  os.dup2(tf.fileno(), 1)

  wd = Watchdog(240.0, label="sun_snapshot_cadence_gate").arm()
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
