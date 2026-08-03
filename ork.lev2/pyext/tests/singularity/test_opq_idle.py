#!/usr/bin/env ork.python
################################################################################
# test_opq_idle — canary for the concurrentQueue idle-pool cost (H4).
#
# WHAT IT PROVES (needs NO audio hardware, NO gpu, NO display — that is the point):
#   A booted, SETTLED, otherwise-idle engine must not keep a large fixed worker
#   pool spinning. This measures, from /proc, the two things the H4 slice changes:
#
#     (a) RESIDENT POOL SIZE — how many "concurrentQueue" worker threads survive
#         20s of idle. Asserted <= the MIN default the pool is sized to (mirrored
#         from ork.core/src/kernel/opq.cpp::concurrentQueue). Before H4 a 96-core
#         box parked ncores/2 = 48 workers here forever.
#     (b) IDLE CPU% — the CPU those workers burn while there is nothing to do.
#         REPORTED, never asserted: it is machine- and load-dependent, and a
#         threshold here would be a flake generator. The number is the point.
#
#   Per-THREAD cpu accounting (not whole-process) is what isolates the pool: the
#   ezapp main loop paces itself at 1kHz and its python on_iter dominates process
#   CPU regardless of the pool. Process CPU% is printed alongside for context.
#
# SHAPE (mirrors test_audio_device_fallback.py): the driver (no --role) is pure
# stdlib and spawns the engine-booting sampler as its OWN ork.python subprocess,
# forcing an unresolvable output device id so the run degrades to the NULL audio
# device and needs no sound hardware. It then classifies the child's numbers and
# emits the machine verdict line.
#
# NOTE on ork.testing: the harness's headless_app()/capture_app() build a GPU
# context and import ecs — neither may happen here (an audio-only, no-gpu process
# is what keeps the CPU measurement attributable). Only the verdict protocol is
# reused; the settle step below is the drain half of ork.testing.app.settle().
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

# a 4-char alnum string (a valid "short id" shape) that no real device hashes to,
# so audio degrades to the NULL device (see test_audio_device_fallback).
UNRESOLVABLE_OUTPUT_ID = "ZZZZ"

# OpqThread::run names every worker after its queue; linux truncates comm to 15
# chars and "concurrentQueue" is exactly 15, so it survives intact.
OPQ_THREAD_COMM = "concurrentQueue"

IDLE_SAMPLE_SECONDS = 20.0
SETTLE_SECONDS = 3.0


################################################################################
# /proc sampling — shared by child roles, pure stdlib.
################################################################################

def _cpu_ticks(stat_path):
  """utime+stime (clock ticks) from a /proc stat file. The comm field is
  parenthesized and may itself contain spaces, so fields are taken relative to
  the LAST ')': utime is field 14, stime field 15, i.e. rest[11] / rest[12]."""
  with open(stat_path, "r") as f:
    st = f.read()
  rest = st[st.rindex(")") + 2:].split()
  return int(rest[11]) + int(rest[12])


def _sample_pool():
  """(worker_count, worker_cpu_ticks) for the concurrentQueue pool right now."""
  count = 0
  ticks = 0
  for tid in os.listdir("/proc/self/task"):
    try:
      with open("/proc/self/task/%s/comm" % tid, "r") as f:
        if f.read().strip() != OPQ_THREAD_COMM:
          continue
      ticks += _cpu_ticks("/proc/self/task/%s/stat" % tid)
      count += 1
    except (IOError, OSError, ValueError):
      continue          # a worker can retire mid-scan; it simply is not counted
  return count, ticks


################################################################################
# CHILD — one engine boot: audio-only headless, settle, then sample the idle pool.
################################################################################

def _role_idle():
  import time
  import orkengine.core                # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp

  core = orkengine.core

  class App(object):
    def __init__(self):
      self.phase = 0
      self.iters = 0
      self.settled = False
      self.t_start = 0.0
      self.samples = []               # (elapsed, worker_count)
      self.pool0 = self.pool1 = (0, 0)
      self.proc0 = self.proc1 = 0
      self.next_sample = 0.0
      self.ezapp = OrkEzApp.create(
          self,
          name="OpqIdleCanary",
          use_subsystems=['opq', 'core', 'audioO'],
          freerun=True,
      )

    def onRunLoopIteration(self):
      self.iters += 1
      now = time.time()

      if self.phase == 0:
        # audio comes up during subsystem bring-up; the (degraded NULL) device
        # object existing is our "boot complete" edge.
        if self.ezapp.audio_device is None and self.iters < 4000:
          return
        # settle: drain the loader queue so no boot-time work is still in flight
        # when the idle window opens (the drain half of ork.testing.app.settle;
        # asyncWorkPending is not yet pybound).
        cq = core.opq_concurrentQueue()
        cq.drain(SETTLE_SECONDS)
        deadline = now + SETTLE_SECONDS
        while time.time() < deadline and cq.num_pending_operations != 0:
          time.sleep(0.02)
        self.settled = (cq.num_pending_operations == 0)
        self.t_start = time.time()
        self.next_sample = self.t_start + 1.0
        self.pool0 = _sample_pool()
        self.proc0 = _cpu_ticks("/proc/self/stat")
        self.phase = 1
        return

      # phase 1: idle. on_iter is free; we only touch /proc once a second.
      elapsed = now - self.t_start
      if now >= self.next_sample:
        self.next_sample = now + 1.0
        self.samples.append((round(elapsed, 2), _sample_pool()[0]))
      if elapsed >= IDLE_SAMPLE_SECONDS:
        self.pool1 = _sample_pool()
        self.proc1 = _cpu_ticks("/proc/self/stat")
        self.window = elapsed
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)

  hz = float(os.sysconf("SC_CLK_TCK"))
  window = getattr(app, "window", 0.0)
  counts = [c for _, c in app.samples]
  pool_pct = proc_pct = -1.0
  if window > 0.0:
    pool_pct = 100.0 * (app.pool1[1] - app.pool0[1]) / hz / window
    proc_pct = 100.0 * (app.proc1 - app.proc0) / hz / window

  # one token per line — the driver's _grep matches line-start keys.
  print("CHILD_SETTLED=%s" % app.settled, flush=True)
  print("CHILD_WINDOW_S=%.2f" % window, flush=True)
  print("CHILD_POOL_BEGIN=%d" % app.pool0[0], flush=True)
  print("CHILD_POOL_END=%d" % app.pool1[0], flush=True)
  print("CHILD_POOL_MAX=%d" % (max(counts) if counts else -1), flush=True)
  print("CHILD_POOL_CPU_PCT=%.3f" % pool_pct, flush=True)
  print("CHILD_PROC_CPU_PCT=%.3f" % proc_pct, flush=True)
  print("CHILD_NCORES=%d" % (os.cpu_count() or 0), flush=True)
  print("CHILD_SAMPLES=%s" % ",".join("%d" % c for c in counts), flush=True)
  sys.exit(0 if (window > 0.0 and app.pool1[0] > 0) else 3)


################################################################################
# DRIVER — pure stdlib; spawns the child and classifies.
################################################################################

def _expected_min_threads():
  """The resident pool size the C++ default policy lands on for THIS machine.
  Mirrors ork::opq::concurrentQueue (ork.core/src/kernel/opq.cpp): a clamped
  quarter of the core count, with the same OBT_NUM_CORES / env-knob overrides."""
  override = os.environ.get("ORKID_CONCURRENTQ_MIN_THREADS")
  if override:
    return int(override)
  ncores = int(os.environ.get("OBT_NUM_CORES") or (os.cpu_count() or 4))
  return max(4, min(24, ncores // 4))


def _spawn_idle(timeout=180):
  import subprocess
  env = dict(os.environ)
  env["ORKID_AUDIO_IOCLASS"] = "PORTAUDIO"            # force the PA resolution path
  env["ORKID_AUDIO_OUTPUT_DEVICE"] = UNRESOLVABLE_OUTPUT_ID
  env.pop("ORKID_DRM_MODE", None)                     # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF, "--role", "idle"]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _grep(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return line.split("=", 1)[1].split()[0]
  return None


def _main_driver():
  import subprocess
  from ork.testing import verdict

  try:
    rc, out = _spawn_idle()
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "child timed out (possible wedge on idle sampling)"))

  print(out)

  def _int(key, dflt=-1):
    v = _grep(out, key)
    return int(v) if v is not None else dflt

  def _flt(key, dflt=-1.0):
    v = _grep(out, key)
    return float(v) if v is not None else dflt

  pool_begin = _int("CHILD_POOL_BEGIN")
  pool_end = _int("CHILD_POOL_END")
  pool_max = _int("CHILD_POOL_MAX")
  pool_pct = _flt("CHILD_POOL_CPU_PCT")
  proc_pct = _flt("CHILD_PROC_CPU_PCT")
  window = _flt("CHILD_WINDOW_S")
  ncores = _int("CHILD_NCORES")
  settled = (_grep(out, "CHILD_SETTLED") == "True")

  expected_min = _expected_min_threads()

  no_crash = (rc == 0)
  measured = (window >= IDLE_SAMPLE_SECONDS * 0.9 and pool_begin > 0 and pool_end > 0)
  # the assertion: an idle engine parks no more than the MIN the pool is sized to.
  pool_ok = (0 < pool_end <= expected_min)

  passed = no_crash and measured and settled and pool_ok
  detail = ("rc=%d ncores=%d expected_min=%d pool_begin=%d pool_end=%d pool_max=%d "
            "idle_cpu_pool=%.2f%% idle_cpu_proc=%.2f%% window=%.1fs settled=%s pool_ok=%s"
            % (rc, ncores, expected_min, pool_begin, pool_end, pool_max,
               pool_pct, proc_pct, window, settled, pool_ok))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "idle":
    _role_idle()
  else:
    _main_driver()
