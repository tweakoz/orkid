#!/usr/bin/env ork.python
################################################################################
# test_audio_first_window — the COLD-START ratchet, on a real device.
#
# WHAT IT PROVES (needs a HOST AUDIO DEVICE; no gpu, no display):
#   That nothing expensive is left for the audio thread to do the FIRST time it
#   is called. The synth learns its buffer geometry from the frame count it is
#   handed, and GROWING that geometry walks 512 pooled layers x 32 stage slots
#   x 2 sync tracks plus every bus. Because _numFrames starts at zero, that
#   growth used to happen inside the very first device callback — 80.1M user
#   instructions, ~18.5ms measured, against a 5.33ms budget. It underran, once,
#   at the top of every run. The fix primes resize() off the audio thread
#   before the stream starts, so the callback's resize is a no-op.
#
#   The failure is invisible to every steady-state gate: it is over before the
#   second callback, it leaves no fault-counter trace (mlockall had already
#   populated the pages), and a single xrun at t=0 is easy to write off as
#   "device warmup". So it gets its own fence, on the FIRST diagnostic window.
#
#   Asserts, over the FIRST [PA_DIAG] 5-second window of a bounded MUTED run:
#     (a) a real host callback ran (audioDiagCounters callbacks > 0) — without
#         this, (b)-(d) could be satisfied by a device that never started;
#     (b) cpu_at_max < FENCE_CPU_US — THE RATCHET. The defect is 80.1M retired
#         instructions, so it shows up as CPU TIME, and CPU time is the one axis
#         a preempted thread cannot inflate (see FENCE below);
#     (c) window_max_compute < the callback's own budget (wall). Coarse by
#         design: in the ungranted lane this axis carries scheduler stalls that
#         have nothing to do with the synth;
#     (d) underflows_total == 0 as of that first window. The pre-fix run
#         underran here deterministically.
#
#   NO VOICES ARE KEYED ON. That is deliberate: resize() runs at the top of
#   synth::compute unconditionally, so an idle synth reproduces the defect at
#   full magnitude, while keyOn's own createInstance outlier (a separate,
#   separately-gated mechanism — see test_audio_keyon_cost.py) would land in
#   this same window and make the fence flaky. This gate measures cold start
#   and nothing else.
#
#   The STREAM and NULL devices never execute a host-device path at all (and
#   keep on-demand resize by design), so this gate cannot run headless without
#   hardware: it SKIPs loudly where no host device opens.
#
# BOTH HOST BACKENDS
#   linux routes PortAudio, darwin routes CoreAudio (audiodevice.cpp
#   AudioDevFactory: COREAUDIO is the default device type wherever
#   ENABLE_CORE_AUDIO is defined). Both emit the same [PA_DIAG] grammar, so the
#   regexes below are platform-independent; the darwin line carries a trailing
#   backend<coreaudio> tag. What differs behind the grammar:
#     - the synth runs on the HAL IOProc thread under PortAudio, and on the
#       CoreAudioThread producer feeding the output queue under CoreAudio, so a
#       window reports whichever thread actually computes the buffer;
#     - faults_* read 0 on darwin (no per-thread rusage), and cpu_at_max comes
#       from CLOCK_THREAD_CPUTIME_ID, which darwin quantizes to 1us;
#     - underflows_total counts output-queue starvation on darwin. The HAL
#       starts before the producer thread exists, so those unavoidable
#       start-up callbacks are reported separately as preroll_starves and are
#       NOT underflows.
#   The cold-start defect is the same on both: the first compute grows the
#   geometry, and the fix primes resize() before the computing thread starts.
#
# FENCE — why the ratchet is on CPU TIME, not wall
#   MEASURED first-window peak, ALSA reference host (WING hw:3,0, 256 frames @48k, 5333us
#   budget). The fence has to be green in BOTH scheduling lanes (like
#   test_audio_numa_legs), and the lanes differ:
#     pre-fix,  fresh-login FIFO ..... 18698 / 18338 us wall (underflow at t=0;
#                                      CPU-bound, so cpu_at_max tracks it)
#     post-fix, fresh-login FIFO ......... 344.7 us wall (loaded: tour voices)
#     post-fix, in-session SCHED_OTHER, 6 runs, idle as this gate runs it:
#             wall  35.5  37.5  43.5  60.9  101.6  899.1 us
#         cpu_at_max 34.1  36.1  42.6  60.2  101.3   35.3 us
#   Read the last column: 899us of WALL against 35us of CPU. With RLIMIT_RTPRIO
#   =0 the audio thread is SCHED_OTHER and simply gets preempted; the wall axis
#   carries that stall, the CPU axis does not. A wall-only fence near 1ms would
#   flake in the ungranted lane, and loosening it far enough to be safe would
#   blunt the ratchet. So the ratchet rides cpu_at_max (10x over the worst
#   sample, ~18x under the defect's 18.5ms) and the wall axis is kept only as a
#   coarse deadline check against the callback's own budget.
#   A FAIL with a LARGE wall and a SMALL cpu_at_peak is scheduler noise, not a
#   regression — the detail line prints both so that is readable at a glance.
#   Tighten only with fresh samples from both lanes.
#
# RUNNING IT
#   ungranted lane (SCHED_OTHER): run it directly from any obt shell. It PASSes
#     there — the RT-ELEVATION DENIED error in the log is expected, not a fail.
#   granted lane (SCHED_FIFO): RLIMIT_RTPRIO comes from pam_limits
#     (/etc/security/limits.d/audio.conf, @audio) and is fixed at LOGIN, so it
#     cannot be acquired inside an already-running session — start the run from
#     a fresh login (a tty/desktop terminal, or `ssh <host>` where sshd runs;
#     the reference host has no sshd). Confirm with `ulimit -r` printing 95, not 0.
#   The host device is EXCLUSIVE — do not run this alongside a showcase tour or
#   test_audio_numa_legs; the driver names any conflicting process it finds.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import re
import sys
import time
import argparse
import subprocess

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

FENCE_CPU_US = 1000.0  # THE RATCHET: first-window peak CPU time (see FENCE above)
BOOT_S    = 15.0     # cap on the wait for the first host callback
WINDOW_S  = 8.0      # hold after the first callback; the diag window is 5s of
                     #  frames, so this clears it with margin for a slow start
SPAWN_TO  = 45.0     # hard timeout on the child (wall budget for the gate)

_WIN_RE = re.compile(
    r"\[PA_DIAG\] window_max_compute<([0-9.eE+-]+)us> cpu_at_max<([0-9.eE+-]+)us> "
    r"faults_at_max<(\d+)> faults_window<(\d+)> budget<([0-9.eE+-]+)us> "
    r"underflows_total<(\d+)>")
_HDR_RE = re.compile(r"\[PA_DIAG\] framesPerBuffer<(\d+)> budget<([0-9.eE+-]+)us> SR<([0-9.eE+-]+)>")

# the backends' "I resolved and started a device" lines (CoreAudio, PortAudio):
#  they separate "this seat has no audio hardware" from "the host stack is
#  wedged" when no callback ever arrives.
_DEVICE_OPENED_MARKS = ("FOUND OUTPUT DEVICE", "using device<")


################################################################################
# CHILD — one engine boot on the REAL device, muted, idle, bounded.
################################################################################

def _role_child():
  import orkengine.core                 # core FIRST (import-order law)
  from orkengine import lev2
  from orkengine.lev2 import OrkEzApp

  class App(object):
    def __init__(self):
      self.state = 0
      self.t_mark = None
      self.callbacks = 0
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioFirstWindowTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          freerun=True,                                # -> real host device
      )

    def iterate(self):
      syn = self.ezapp.audio_synth
      if syn is None:
        return
      now = time.monotonic()
      ############################################
      if self.state == 0:   # MUTE. no program, no voices — cold start only.
        syn.masterGain = 0.0
        self.t_mark = now
        self.state = 1
        return
      ############################################
      time.sleep(0.004)
      self.callbacks = int(lev2.audioDiagCounters().get("callbacks", 0))
      ############################################
      if self.state == 1:   # wait for the host callback to actually be running
        if self.callbacks > 0:
          self.t_mark = now
          self.state = 2
        elif (now - self.t_mark) > BOOT_S:
          self.ezapp.signalExit()   # no device ever ran -> the driver SKIPs
        return
      ############################################
      if self.state == 2:   # hold until the first diag window has printed
        if (now - self.t_mark) >= WINDOW_S:
          self.ezapp.signalExit()
        return

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.iterate)

  print("CHILD_CALLBACKS=%d" % app.callbacks, flush=True)
  sys.exit(0)


################################################################################
# DRIVER
################################################################################

def _device_holders():
  # name anything already holding the exclusive host device, so a SKIP for "no
  #  callback" is never mistaken for "no hardware here".
  try:
    p = subprocess.run(["pgrep", "-af", "showcase|tour|test_audio_numa_legs"],
                       capture_output=True, text=True, timeout=10)
  except (OSError, subprocess.SubprocessError):
    return ""
  lines = [l.split(None, 1)[0] for l in (p.stdout or "").splitlines()
           if SELF not in l]
  return ",".join(lines)


def _spawn():
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env.pop("ORKID_AUDIO_IOCLASS", None)          # the REAL device is the point
  env["ORKID_PA_DIAG"] = "1"                    # emit the headroom windows
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  p = subprocess.run(["ork.python", SELF, "--role", "child"],
                     capture_output=True, text=True, timeout=SPAWN_TO, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _main_driver():
  from ork.testing import verdict

  try:
    rc, out = _spawn()
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "first-window run timed out (possible wedge)"))
  print(out)

  callbacks = 0
  for line in out.splitlines():
    if line.startswith("CHILD_CALLBACKS="):
      callbacks = int(line.split("=", 1)[1].strip())

  if rc == 0 and callbacks == 0:
    # no host device opened here (headless seat, or the device is held). The
    #  consumer-pumped paths never run this callback at all, so there is
    #  nothing to measure. Loud skip, never a green tick.
    if any(m in out for m in _DEVICE_OPENED_MARKS):
      # a device WAS resolved and started, and the host still never called
      #  back: the audio stack is wedged, not absent. On darwin that is
      #  coreaudiod (seen as "HALS_PlugIn: the object is not valid" in
      #  `log show --predicate 'process == "coreaudiod"'`, and as
      #  `afplay <any>` failing -66681 for EVERY device incl. virtual ones);
      #  it clears with `sudo killall coreaudiod`. Distinguished from "no
      #  hardware" so the next operator does not go looking for a device.
      print("TESTVERDICT=SKIP detail=host-device-opened-but-never-called-back "
            "(host audio stack not delivering IO; on darwin restart coreaudiod) "
            "holders=<%s>" % _device_holders(), flush=True)
      sys.exit(0)
    print("TESTVERDICT=SKIP detail=no-host-audio-callback (real device required) holders=<%s>"
          % _device_holders(), flush=True)
    sys.exit(0)

  hdr = _HDR_RE.search(out)
  win = _WIN_RE.search(out)   # FIRST window line — the cold-start one
  if win is None:
    sys.exit(verdict(False,
                     "rc=%d callbacks=%d — no [PA_DIAG] window line (ORKID_PA_DIAG "
                     "not honored, or the run ended before the first 5s window)"
                     % (rc, callbacks)))

  peak_us     = float(win.group(1))
  cpu_at_max  = float(win.group(2))
  faults      = int(win.group(3))
  budget_us   = float(win.group(5))
  underflows  = int(win.group(6))
  fpb         = int(hdr.group(1)) if hdr else -1
  srate       = float(hdr.group(3)) if hdr else -1.0

  # the wall fence is the callback's OWN deadline, read out of the log, so it
  #  scales with framesPerBuffer/SR instead of hardcoding this device's 5333us.
  wall_fence = budget_us if budget_us > 0.0 else 5333.0

  cpu_ok   = (cpu_at_max < FENCE_CPU_US)
  wall_ok  = (peak_us < wall_fence)
  under_ok = (underflows == 0)
  cb_ok    = (callbacks > 0)
  passed   = (rc == 0 and cpu_ok and wall_ok and under_ok and cb_ok)

  detail = ("rc=%d callbacks=%d fpb=%d SR=%g | first-window cpu_at_peak=%.1fus "
            "(fence %.0fus) wall_peak=%.1fus (budget %.0fus) faults_at_peak=%d "
            "underflows=%d | cpu=%s wall=%s underflows=%s callback=%s"
            % (rc, callbacks, fpb, srate,
               cpu_at_max, FENCE_CPU_US, peak_us, wall_fence, faults, underflows,
               cpu_ok, wall_ok, under_ok, cb_ok))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "child":
    _role_child()
  else:
    _main_driver()
