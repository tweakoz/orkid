#!/usr/bin/env ork.python
################################################################################
# test_audio_device_fallback — canary for audio device-enumeration hardening (H1).
#
# WHAT IT PROVES (needs NO audio hardware — that is the point):
#   An EzApp audio program whose requested OUTPUT device short id cannot be
#   resolved must NOT assert / SIGSEGV. It degrades to the NULL audio device with
#   a loud error and exits cleanly. This guards the OrkAssert(false) that used to
#   fire on an unresolvable id (PipeWire holding a device once caused a startup
#   assert), and keeps the AudioDeviceException -> NULL-device path wired.
#
# SHAPE (mirrors ork_testing_gate.py): the driver (no --role) is pure stdlib and
# spawns the engine-booting check as its OWN ork.python subprocess, forcing the
# PORTAUDIO ioclass and an unresolvable 4-char output id via the environment. It
# then asserts on the child's rc (no crash) AND on the loud diagnostic in the
# child's output, and emits the machine verdict line.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import subprocess

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

# a 4-char alnum string (a valid "short id" shape) that no real device hashes to.
UNRESOLVABLE_OUTPUT_ID = "ZZZZ"

# substrings the C++ degrade path MUST log (audiodevice_pa.cpp / ezapp.cpp).
EXPECT_UNRESOLVED = "unresolvable output audio device short id"
EXPECT_DEGRADE = "degrading to NULL audio device"


################################################################################
# CHILD — one engine boot: force PORTAUDIO + an unresolvable output id, run the
# ezapp bounded, and exit 0 iff we degraded to a live NULL device without crashing.
################################################################################

def _role_fallback():
  import orkengine.core             # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp

  class App(object):
    def __init__(self):
      self.iters = 0
      self.device_seen = False
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioDeviceFallbackTest",
          use_subsystems=['opq', 'core', 'audioO'],
          freerun=True,
      )

    def onRunLoopIteration(self):
      self.iters += 1
      # audio init happens during subsystem bring-up; once the (degraded) device
      # object exists we have observed the fallback and can exit cleanly.
      dev = self.ezapp.audio_device
      if dev is not None:
        self.device_seen = True
      if self.device_seen or self.iters >= 400:
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  # verdict computed by the driver; the child just reports what it observed.
  print("CHILD_DEVICE_SEEN=%s iters=%d" % (app.device_seen, app.iters), flush=True)
  sys.exit(0 if app.device_seen else 2)


################################################################################
# DRIVER — pure stdlib; spawns the child and classifies.
################################################################################

def _spawn_fallback(timeout=90):
  env = dict(os.environ)
  env["ORKID_AUDIO_IOCLASS"] = "PORTAUDIO"           # force the PA resolution path
  env["ORKID_AUDIO_OUTPUT_DEVICE"] = UNRESOLVABLE_OUTPUT_ID
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"                 # C++ error logs must be captured
  cmd = ["ork.python", SELF, "--role", "fallback"]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _main_driver():
  from ork.testing import verdict

  try:
    rc, out = _spawn_fallback()
  except subprocess.TimeoutExpired as e:
    combined = (e.stdout or "") + (e.stderr or "")
    print(combined)
    sys.exit(verdict(False, "child timed out (possible wedge/deadlock on fallback)"))

  print(out)

  no_crash = (rc == 0)
  saw_unresolved = EXPECT_UNRESOLVED in out
  saw_degrade = EXPECT_DEGRADE in out
  saw_device = "CHILD_DEVICE_SEEN=True" in out

  passed = no_crash and saw_unresolved and saw_degrade and saw_device
  detail = ("rc=%d no_crash=%s unresolved=%s degrade=%s null_device=%s"
            % (rc, no_crash, saw_unresolved, saw_degrade, saw_device))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "fallback":
    _role_fallback()
  else:
    _main_driver()
