#!/usr/bin/env ork.python
################################################################################
# GIL-MODE ECS REGRESSION gate — the ECS sub-interpreter must run GIL-OFF.
#
# THE DEADLOCK THIS PINS. ork::python::Context2 (the ECS sub-interpreter) was
# built for a free-threaded, GIL-OFF interpreter. Its bind/unbind takes a C++
# mutex (_subInterpMutex) and THEN attaches the sub-interpreter's thread state.
# Under PYTHON_GIL=1 that sub-GIL becomes a REAL lock, and the pair below
# closes an AB-BA cycle:
#
#   render thread : holds _subInterpMutex (context.cpp bindSubInterpreter),
#                   blocks taking the SUB interpreter's GIL.
#   update thread : holds the SUB GIL running a scene system script
#                   (sky_time_system.py), and the FIRST import of any C
#                   extension in a sub-interpreter forces CPython to attach to
#                   the MAIN interpreter's GIL (import_run_extension ->
#                   switch_to_main_interpreter) — which the render thread's
#                   main-thread-state holder is not going to release.
#
# The far side of that cycle is INSIDE CPython, so no lock reorder in orkid can
# break it. The invariant is re-established where it belongs: the ork.python
# launcher sets PYTHON_GIL=0 in the DON'T-CLOBBER form (an explicitly-set value
# still wins), and ork.scene.viewer.py does the same for the player it execs.
#
# THREE LEGS, and the second one is the teeth:
#
#   LEG A (the fix)   spawn the deadlock-shaped child THROUGH the ork.python
#                     wrapper with PYTHON_GIL REMOVED from its environment. The
#                     wrapper must inject GIL-off and the child must reach its
#                     own verdict and exit clean.
#   LEG B (the control) the identical child with PYTHON_GIL=1 set explicitly —
#                     the don't-clobber form leaves it alone, so the deadlock
#                     mechanism is still armed and the child must NOT get out
#                     alive, inside a bound derived from what LEG A just took.
#                     If LEG B ever exits clean, the mechanism changed
#                     underneath this gate and LEG A stopped proving anything —
#                     which is why the control is permanent.
#   LEG C (the second hole) the launcher only covers processes that GO THROUGH
#                     the launcher. Anything else — a bare venv python, an
#                     embedder, a pip-installed orkengine — reaches CPython with
#                     PYTHON_GIL simply ABSENT, and an extension module that has
#                     not DECLARED free-threading support makes CPython
#                     re-enable the GIL at import time ("has not declared that
#                     it can run safely without the GIL", RuntimeWarning) — the
#                     deadlock re-arms with nobody having set anything. So every
#                     orkengine extension declares it: _core/_lev2/_ecs pass
#                     py::mod_gil_not_used() to PYBIND11_MODULE, _ecssim (a
#                     hand-written multi-phase PyModuleDef) carries the
#                     {Py_mod_gil, Py_MOD_GIL_NOT_USED} slot. This leg runs the
#                     venv interpreter DIRECTLY (no wrapper, PYTHON_GIL scrubbed)
#                     under -W error::RuntimeWarning and requires
#                     sys._is_gil_enabled() to stay False across all four
#                     imports — the warning being fatal makes a regression loud
#                     instead of a silent 40x-slower, deadlock-armed process.
#
# THE CHILD is this same file re-invoked with --child: an offscreen OrkEzApp, a
# procedural sky (which is what attaches sky_time_system.py — the sub-interp
# system script whose first C-extension import is one half of the cycle), an
# ecs.Controller and startSimulation, run for a handful of frames. Deferring
# startSimulation into onUpdate is the shape test_pause_publish_gate.py
# documents (the ECS gpu-init rendezvous draining outside a frame null-derefs
# LightManager::gpuInit).
#
# SELF-CONTAINED: no environment variables to set, no assets, no goldens. The
# wrapper defaults to whichever ork.python is on PATH; --wrapper <abs path>
# points the legs at a freshly built one (that is how a lane proves ITS binary
# rather than staging's — an ork.python that predates the fix fails leg A's
# injection assertion, which is exactly what this gate is for). --pypkg <dir> is
# the same idea for LEG C: it prepends a package directory to that leg's
# PYTHONPATH so a lane can test the orkengine modules IT just built instead of
# the ones installed in staging.
#
# THE CYCLE, as /usr/bin/sample caught it in a wedged child (mac, aug 2026):
#   main/render thread: PythonSystem::_onGpuUpdate -> Context2::bindSubInterpreter
#                       (context.cpp:544) -> recursive_mutex::lock -> psynch_mutexwait
#   sim update thread : ... import_find_and_load -> _imp_create_builtin ->
#                       import_run_extension -> _PyThreadState_Attach -> take_gil
#
# THE IN-CHILD WATCHDOG CANNOT BE THE BOUND HERE, measured: this deadlock ends
# with the MAIN interpreter's GIL held by the wedged update thread (that is the
# cross-attach), so no Python thread in the child — the watchdog's included —
# gets scheduled again, and it never fires. It stays armed because it still
# samples any OTHER kind of wedge; the DRIVER's deadline plus a process-GROUP
# kill (start_new_session, so only pids this test spawned) is the real bound.
#
# WHERE THE WEDGE LANDS, measured on mac: the child renders its frames and then
# hangs in TEARDOWN (the sim sub-interpreter's own shutdown re-enters the same
# critical section), so the completion marker is printed only after a survived
# shutdown — see _run_child.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import argparse
import shutil
import signal
import subprocess
import time

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))
# THIS tree's helpers, not the staging install's — a lane worktree runs its own.
sys.path.insert(0, os.path.join(_ROOT, "ork.lev2", "examples", "python"))

from ork.testing import verdict
from ork.testing.watchdog import Watchdog

WIDTH, HEIGHT = 256, 192
CAM_NAME = "spawncam"              # the only camera name SceneGraphSystem publishes

SIM_START_FRAME = 10               # drawn frames before startSimulation
RUN_FRAMES      = 120              # drawn frames AFTER the sim starts, then done
CHILD_OK        = "GILCHILD_COMPLETED"   # printed only after a CLEAN teardown
CHILD_ENV_MARK  = "GILCHILD_ENV"         # the launcher's fingerprint (see _run_child)
LEGC_MARK       = "GILDECL"              # per-module GIL state, leg C
LEGC_OK         = "GILDECL_ALL_OFF"      # every import left the GIL off
LEGC_TIMEOUT    = 90.0                   # imports only; no context, no frames

# ROOM FOR A COLD START: first-run shader/pipeline builds dominate. The driver's
# own deadline is the real bound (see the watchdog note in the header); the
# in-child watchdog sits just under it as the sampler for wedges that are NOT
# this deadlock.
LEG_DRIVER_TIMEOUT   = 120.0
LEG_CHILD_DEADLINE   = 110.0
# THE CONTROL'S BOUND IS DERIVED, NOT GUESSED: a control killed before a healthy
# child would even have finished proves nothing, so LEG B waits a multiple of
# what LEG A actually took (floored, and capped by the driver bound).
LEG_B_SLACK_FACTOR   = 2.5
LEG_B_MIN_TIMEOUT    = 45.0

################################################################################
# the child: the deadlock-shaped app
################################################################################


def _run_child(deadline_s):
  # THE LAUNCHER'S FINGERPRINT, printed before anything else. With PYTHON_GIL
  # absent from the spawn environment a free-threaded interpreter would run
  # GIL-off ANYWAY, so "the child finished" alone cannot tell a fixed launcher
  # from an unfixed one. What only the fixed launcher produces is the injected
  # value itself — leg A asserts on this line.
  print("%s PYTHON_GIL=%s gil_enabled=%s"
        % (CHILD_ENV_MARK, os.environ.get("PYTHON_GIL"),
           sys._is_gil_enabled() if hasattr(sys, "_is_gil_enabled") else "n/a"),
        flush=True)

  from orkengine.core import vec3, vec4, CrcStringProxy
  from orkengine import lev2
  from orkengine import ecs
  from lev2utils.cameras import setupUiCameraX

  tokens = CrcStringProxy()

  class GilChildApp:

    def __init__(self):
      self.ezapp = lev2.OrkEzApp.createEx(
          self, [ecs.ecsInitCallback], name="gil_ecs_regression",
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
      self._frame = 0
      self._simframe = 0
      self._done = False

    def onGpuInit(self, ctx):
      from ork.hypergraph.ecs.scene import Scene
      from ork.hypergraph.ecs.scene.assets import wire_scene_data

      class GilChildScene(Scene):
        def __init__(self):
          super().__init__()
          # the sky's minimum: it is here for the SYSTEM SCRIPT it attaches
          # (sky_time_system.py, the sub-interpreter half of the cycle), not
          # for any pixel this test reads.
          self.sky(time_of_day = 10.0,
                   time_scale  = 60.0,
                   moon        = False,
                   stars       = False,
                   msaa        = 0,
                   ssaa        = 0)

      sd = ecs.SceneData()
      GilChildScene().build(sd)
      wire_scene_data(sd, ezapp=self.ezapp)

      self.cameralut = lev2.CameraDataLut()
      self.camera, self.uicam = setupUiCameraX(cameralut=self.cameralut,
                                               camname=CAM_NAME)
      self.uicam.far_max = 200000.0   # the sky shell sits at 30km
      self.uicam.lookAt(vec3(0, 2, -12), vec3(0, 6, 0), vec3(0, 1, 0))
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)

      sgp = sd.generateSceneGraphParams()
      sg = lev2.scenegraph.Scene(sgp)
      for _lyr in ("std_forward", "std_transparent", "hud_overlay"):
        sg.createLayer(_lyr)
      self.sg = sg
      self.sgv.scenegraph = sg
      self.sgv.forkDB()
      sg.pbr_common.sky_source = "procedural"

      self.ctrl = ecs.Controller()
      self.ctrl.bindScene(sd)
      self.ctrl.createSimulation(scenegraph=sg)

    def onGpuUpdate(self, ctx):
      # the RENDER-thread entry into the sub-interpreter — one half of the cycle.
      if self.ctrl:
        self.ctrl.gpuUpdate(ctx)

    def onUpdateInit(self):
      pass   # startSimulation is deferred to onUpdate (see header)

    def onUpdate(self, updinfo):
      if self.ctrl is None or self._done:
        return
      if self.sysref is None:
        if self._frame < SIM_START_FRAME:
          return
        self.ctrl.startSimulation()
        self.sysref = self.ctrl.findSystem("SceneGraphSystem")
        print("[gil-child] sim started at frame %d" % self._frame, flush=True)
        return
      UIC = self.uicam.cameradata
      self.ctrl.systemNotify(self.sysref, tokens.UpdateCamera,
                             {tokens.eye: UIC.eye, tokens.tgt: UIC.target,
                              tokens.up: UIC.up, tokens.near: UIC.near,
                              tokens.far: UIC.far, tokens.fovy: UIC.fovy})
      # the UPDATE-thread entry into the sub-interpreter — the other half.
      self.ctrl.updateSimulation()
      self.sgv.setDirty()

    def onGpuPostFrame(self, ctx):
      self._frame += 1
      if self._done or self.sysref is None:
        return
      self._simframe += 1
      if self._simframe >= RUN_FRAMES:
        self._done = True
        print("[gil-child] frames done: %d (sim %d)" % (self._frame, self._simframe),
              flush=True)
        self.ezapp.signalExit()

  wd = Watchdog(deadline_s, label="gil_ecs_child").arm()
  app = GilChildApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  wd.disarm()
  # AFTER TEARDOWN, deliberately: under PYTHON_GIL=1 the observed wedge lands in
  # the shutdown path (the sim sub-interpreter's own teardown binds the same
  # mutex), so a marker printed at the last frame would call a wedged run
  # "completed". This marker means the whole process got out alive.
  if app._done:
    print("%s frames=%d" % (CHILD_OK, app._frame), flush=True)
    return 0
  return 1


################################################################################
# the driver
################################################################################


def _spawn_leg(wrapper, child_env, child_deadline, driver_timeout):
  """Run one leg. Returns (rc_or_None, elapsed, output). rc None == the driver's
  own deadline fired and the child's process GROUP was killed (only the pids this
  test spawned: start_new_session puts the child in its own group)."""
  cmd = [wrapper, os.path.abspath(__file__), "--child",
         "--deadline", "%.1f" % child_deadline]
  env = dict(os.environ)
  # THIS tree's scripts on the child's PYTHONPATH: the scene attaches a system
  # script BY PATH from this worktree, and the sim SUB-INTERPRETER resolves that
  # script's own siblings off sys.path — which it inherits from the child's
  # startup config, i.e. from here. Without it the script dies on an import
  # before it can reach the code path under test.
  _scripts = os.path.join(_ROOT, "obt.project", "scripts")
  env["PYTHONPATH"] = (_scripts + os.pathsep + env["PYTHONPATH"]
                       if env.get("PYTHONPATH") else _scripts)
  env.update({k: v for k, v in child_env.items() if v is not None})
  for k, v in child_env.items():
    if v is None:
      env.pop(k, None)
  t0 = time.time()
  proc = subprocess.Popen(cmd, env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, start_new_session=True)
  try:
    out = proc.communicate(timeout=driver_timeout)[0].decode("utf-8", "replace")
    return proc.returncode, time.time() - t0, out
  except subprocess.TimeoutExpired:
    try:
      os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except Exception:
      proc.kill()
    out = b""
    try:
      out = proc.communicate(timeout=15)[0] or b""
    except Exception:
      pass
    return None, time.time() - t0, out.decode("utf-8", "replace")


################################################################################
# LEG C — the module-declaration leg (no wrapper in the picture at all)
################################################################################

# Runs in a BARE venv interpreter. Kept as source text rather than a --child
# mode because the point is a process that never touched orkid's launcher.
_LEGC_SRC = r"""
import sys
if not hasattr(sys, "_is_gil_enabled"):
  print("GILDECL_SKIP not_a_free_threaded_interpreter", flush=True); raise SystemExit(0)
print("GILDECL start gil_enabled=%s" % sys._is_gil_enabled(), flush=True)
_on = []
for _name in ("orkengine.core", "orkengine.lev2", "orkengine.ecs", "orkengine.ecssim"):
  __import__(_name)
  _g = sys._is_gil_enabled()
  print("GILDECL %s gil_enabled=%s" % (_name, _g), flush=True)
  if _g:
    _on.append(_name)
# asserted independently of -W error::RuntimeWarning: a python that suppresses
# or renames that warning must still not get a silent pass here.
if _on:
  print("GILDECL_FAIL gil_re_enabled_by=%s" % ",".join(_on), flush=True); raise SystemExit(3)
print("GILDECL_ALL_OFF", flush=True)
"""


def _spawn_leg_c(pypkg):
  """Bare venv interpreter, PYTHON_GIL scrubbed, RuntimeWarning fatal."""
  cmd = [sys.executable, "-W", "error::RuntimeWarning", "-c", _LEGC_SRC]
  env = dict(os.environ)
  env.pop("PYTHON_GIL", None)
  if pypkg:
    env["PYTHONPATH"] = (pypkg + os.pathsep + env["PYTHONPATH"]
                         if env.get("PYTHONPATH") else pypkg)
  t0 = time.time()
  proc = subprocess.Popen(cmd, env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, start_new_session=True)
  try:
    out = proc.communicate(timeout=LEGC_TIMEOUT)[0].decode("utf-8", "replace")
    return proc.returncode, time.time() - t0, out
  except subprocess.TimeoutExpired:
    try:
      os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except Exception:
      proc.kill()
    return None, time.time() - t0, ""


def _tail(text, n=12):
  lines = [l for l in text.splitlines() if l.strip()]
  return "\n".join("    | " + l for l in lines[-n:])


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--child", action="store_true", help=argparse.SUPPRESS)
  ap.add_argument("--deadline", type=float, default=LEG_CHILD_DEADLINE,
                  help=argparse.SUPPRESS)
  ap.add_argument("--wrapper", default=None,
                  help="ork.python launcher to spawn the legs with "
                       "(default: the one on PATH)")
  ap.add_argument("--pypkg", default=None,
                  help="package dir prepended to LEG C's PYTHONPATH, so a lane "
                       "can test the orkengine modules it just built "
                       "(default: whatever is installed)")
  args = ap.parse_args()

  if args.child:
    return _run_child(args.deadline)

  wrapper = args.wrapper or shutil.which("ork.python")
  if not wrapper or not os.path.isfile(wrapper):
    return verdict(False, "no ork.python launcher found (--wrapper <path>)")
  wrapper = os.path.abspath(wrapper)
  print("[gil-gate] launcher: %s" % wrapper, flush=True)

  fails = []

  # LEG A — PYTHON_GIL absent: the launcher must inject GIL-off and the child
  # must run AND tear down clean.
  rc_a, t_a, out_a = _spawn_leg(wrapper, {"PYTHON_GIL": None},
                                LEG_CHILD_DEADLINE, LEG_DRIVER_TIMEOUT)
  a_injected = ("%s PYTHON_GIL=0 gil_enabled=False" % CHILD_ENV_MARK) in out_a
  a_ok = (rc_a == 0) and (CHILD_OK in out_a) and a_injected
  print("=== LEG A (PYTHON_GIL unset -> launcher injects 0): rc=%s %.1fs "
        "injected=%s %s"
        % (rc_a, t_a, a_injected, "PASS" if a_ok else "FAIL"), flush=True)
  print(_tail(out_a), flush=True)
  if not a_injected:
    # the launcher under test did NOT set it — an unfixed ork.python. The child
    # may well still finish (a free-threaded build defaults to GIL-off), which
    # is exactly why this is asserted separately from completion.
    fails.append("legA_launcher_did_not_inject_PYTHON_GIL=0")
  if rc_a != 0 or CHILD_OK not in out_a:
    fails.append("legA_child_did_not_complete(rc=%s,%.1fs)" % (rc_a, t_a))

  # LEG B — PYTHON_GIL=1 explicit: the don't-clobber form leaves it, so the
  # mechanism is still armed and the child must NOT get out alive inside a bound
  # that a HEALTHY child (leg A, just measured) clears with room to spare.
  b_timeout = min(LEG_DRIVER_TIMEOUT, max(LEG_B_MIN_TIMEOUT,
                                          LEG_B_SLACK_FACTOR * t_a)) if a_ok \
      else LEG_DRIVER_TIMEOUT
  rc_b, t_b, out_b = _spawn_leg(wrapper, {"PYTHON_GIL": "1"},
                                b_timeout + 5.0, b_timeout)
  b_wedged = (CHILD_OK not in out_b) and (rc_b != 0)
  print("=== LEG B (PYTHON_GIL=1 explicit, control; bound %.1fs = %.1fx leg A): "
        "rc=%s %.1fs %s"
        % (b_timeout, b_timeout / t_a if t_a > 0 else 0.0,
           "killed-by-deadline" if rc_b is None else rc_b, t_b,
           "PASS" if b_wedged else "FAIL"), flush=True)
  print(_tail(out_b), flush=True)
  if not b_wedged:
    fails.append("legB_control_completed_under_GIL1(rc=%s,%.1fs) — the deadlock "
                 "mechanism is gone and LEG A no longer proves the fix" % (rc_b, t_b))

  # LEG C — no wrapper, PYTHON_GIL absent: the modules themselves must declare
  # free-threading support or CPython re-enables the GIL behind everyone's back.
  pypkg = os.path.abspath(args.pypkg) if args.pypkg else None
  rc_c, t_c, out_c = _spawn_leg_c(pypkg)
  c_skipped = "GILDECL_SKIP" in out_c
  c_ok = (rc_c == 0) and (c_skipped or (LEGC_OK in out_c))
  print("=== LEG C (bare venv python, PYTHON_GIL absent, RuntimeWarning fatal%s): "
        "rc=%s %.1fs %s"
        % (" pypkg=%s" % pypkg if pypkg else "", rc_c, t_c,
           "SKIP(non-free-threaded)" if c_skipped else
           ("PASS" if c_ok else "FAIL")), flush=True)
  print(_tail(out_c), flush=True)
  if not c_ok:
    fails.append("legC_module_did_not_declare_free_threading(rc=%s,%.1fs)"
                 % (rc_c, t_c))

  ok = (len(fails) == 0)
  detail = ("legA_rc=%s legA_secs=%.1f legB_rc=%s legB_secs=%.1f "
            "legC_rc=%s legC_secs=%.1f"
            % (rc_a, t_a, "killed" if rc_b is None else rc_b, t_b, rc_c, t_c))
  if fails:
    detail += " failed=" + ",".join(fails)
  return verdict(ok, detail)


if __name__ == "__main__":
  sys.exit(main())
