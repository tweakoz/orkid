#!/usr/bin/env ork.python
################################################################################
# test_audio_rtsched_diag — device xrun telemetry + headless scheduling neutrality.
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   The realtime-scheduling work on the host-device path is INVISIBLE to the
#   headless STREAM path, and the telemetry it publishes is reachable from
#   python on every platform.
#
#   Asserts:
#     (a) lev2.audioDiagCounters() exists and returns the four telemetry keys
#         (underflows / callbacks / frames_per_buffer / sample_rate) — the
#         binding a --diag surface (ork.synth.showcase.py) reads;
#     (b) under the SYNC_NONREALTIME StrAudioDevice every counter reads ZERO
#         after a full pump schedule: only a host-api callback publishes them,
#         so a nonzero value here means a real device was opened headless;
#     (c) NO thread had its scheduling policy touched — elevateAudioThread() is
#         called from the PortAudio callback only, so the child emits no
#         "RT-ELEVAT" line. This is the guard that keeps the FIFO request off
#         the STREAM/offline paths (byte-determinism there must not depend on
#         scheduling);
#     (d) the child boots and exits clean (rc 0).
#
#   The achieved-policy line itself (SCHED_FIFO vs the EPERM fallback) is only
#   observable with a real device attached and is verified by running
#   ork.synth.showcase.py --diag on hardware — it cannot be gated headless.
#
# SHAPE (mirrors test_audio_keyon_rtalloc.py): the driver (no --role) is pure
# stdlib and spawns the engine boot as its OWN ork.python subprocess with
# ORKID_DRM_MODE stripped and ORKID_AUDIO_IOCLASS pinned to STREAM, so the test
# can never touch the physical display or a host audio device.
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

DT     = 1.0 / 100.0   # 480 frames @48k
PUMPS  = 32            # bounded schedule: no test runs unbounded
KEYS   = ("underflows", "callbacks", "frames_per_buffer", "sample_rate")


################################################################################
# CHILD
################################################################################

def _role_child():
  import orkengine.core                 # core FIRST (import-order law)
  from orkengine import lev2
  from orkengine.lev2 import OrkEzApp

  class App(object):
    def __init__(self):
      self.i = 0
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioRtSchedDiagTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          audio_stream_sync=True,                      # -> StrAudioDevice SYNC
          freerun=True,
      )

    def onRunLoopIteration(self):
      dev = self.ezapp.audio_device
      if dev is None:
        return
      dev.advanceTime(DT)
      self.i += 1
      if self.i >= PUMPS:
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)

  ctrs = lev2.audioDiagCounters()
  missing = [k for k in KEYS if k not in ctrs]
  print("CHILD_KEYS_MISSING=%s" % ",".join(missing), flush=True)
  print("CHILD_CTRS=%s" % ",".join("%s:%g" % (k, float(ctrs.get(k, -1)))
                                   for k in KEYS), flush=True)
  print("CHILD_PUMPS=%d" % app.i, flush=True)
  sys.exit(0)


################################################################################
# DRIVER
################################################################################

def _spawn(timeout=600):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env["ORKID_AUDIO_IOCLASS"] = "STREAM"         # never open a host audio device
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF, "--role", "child"]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _grep(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return line.split("=", 1)[1].strip()
  return None


def _main_driver():
  import subprocess
  from ork.testing import verdict

  try:
    rc, out = _spawn()
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "child run timed out (possible wedge)"))

  print(out)

  missing = _grep(out, "CHILD_KEYS_MISSING")
  ctrs_raw = _grep(out, "CHILD_CTRS")
  pumps = _grep(out, "CHILD_PUMPS")

  ran_ok = (rc == 0 and ctrs_raw is not None and pumps == str(PUMPS))
  keys_ok = (missing == "")

  values = {}
  if ctrs_raw:
    for tok in ctrs_raw.split(","):
      k, _, v = tok.partition(":")
      try:
        values[k] = float(v)
      except ValueError:
        values[k] = -1.0
  zero_ok = ran_ok and keys_ok and all(values.get(k, -1.0) == 0.0 for k in KEYS)
  nosched_ok = ("RT-ELEVAT" not in out)

  passed = (ran_ok and keys_ok and zero_ok and nosched_ok)
  detail = ("rc=%d pumps=%s ran=%s keys=%s(missing='%s') zero=%s(%s) "
            "no_sched_change=%s"
            % (rc, pumps, ran_ok, keys_ok, missing, zero_ok, ctrs_raw, nosched_ok))
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
