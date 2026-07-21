################################################################################
# ork.testing.watchdog — a deadline watchdog that SAMPLES before it kills.
#
# WHY: a wedged test (a deadlocked join, a spinning materialize, a stuck exit
# handler — see the "hangs = stuck exit handler" class) that a CI just SIGKILLs
# leaves no evidence of WHERE it hung. This arms a deadline; on expiry it first
# captures a stack sample of the wedged process, writes it to a named file, and
# ONLY THEN hard-exits with a distinct return code — so the sample outlives the
# kill and the rc is unambiguously "watchdog fired" (not the test's own 0/1).
#
# Sample sources, richest-first, best-effort union:
#   * faulthandler.dump_traceback — the PORTABLE core (all Python thread stacks);
#     always available, always written first so there is ALWAYS a sample.
#   * `sample` (macOS) — native full-process stacks (C++ frames too); appended
#     when present. On linux the faulthandler dump is the portable substitute.
#
# The armed thread is a daemon waiting on an Event with a timeout: disarm() sets
# the Event so a normal finish cancels the kill cleanly.
################################################################################

import faulthandler
import os
import subprocess
import sys
import threading

# distinct, documented rc: not 0/1 (test verdict), not 130 (SIGINT), not 137
# (SIGKILL) — a reader seeing this rc knows the DEADLINE fired.
WATCHDOG_RC = 111


class Watchdog(object):
  """A one-shot deadline. arm() starts the countdown; disarm() cancels it. On
  expiry: sample -> write file -> os._exit(rc). Re-armable is out of scope (a test
  arms once around its body)."""

  def __init__(self, deadline_s, sample_path=None, rc=WATCHDOG_RC, label="watchdog"):
    self._deadline_s = float(deadline_s)
    self._sample_path = sample_path or os.path.join(
        os.environ.get("TMPDIR", "/tmp"), "ork_testing_watchdog_%d.txt" % os.getpid())
    self._rc = int(rc)
    self._label = label
    self._cancel = threading.Event()
    self._thread = None

  @property
  def sample_path(self):
    return self._sample_path

  def arm(self):
    if self._thread is not None:
      return self
    self._thread = threading.Thread(target=self._run, name=self._label, daemon=True)
    self._thread.start()
    return self

  def disarm(self):
    self._cancel.set()

  def _run(self):
    fired = not self._cancel.wait(self._deadline_s)
    if not fired:
      return   # disarmed before the deadline — normal finish
    self._sample_and_die()

  def _sample_and_die(self):
    pid = os.getpid()
    try:
      with open(self._sample_path, "w") as f:
        f.write("=== ork.testing watchdog FIRED: %s deadline=%.1fs pid=%d ===\n"
                % (self._label, self._deadline_s, pid))
        f.flush()
        # PORTABLE core: every Python thread's stack (this is the guaranteed sample).
        faulthandler.dump_traceback(file=f, all_threads=True)
        f.flush()
        # NATIVE augmentation (macOS): full-process stacks incl. C++ frames.
        if sys.platform == "darwin":
          try:
            out = subprocess.run(["/usr/bin/sample", str(pid), "1", "-mayDie"],
                                 capture_output=True, timeout=20)
            f.write("\n=== /usr/bin/sample %d ===\n" % pid)
            f.write(out.stdout.decode("utf-8", "replace"))
            if out.stderr:
              f.write("\n[sample stderr]\n" + out.stderr.decode("utf-8", "replace"))
          except Exception as e:   # sample unavailable / timed out — dump was enough
            f.write("\n[sample failed: %r]\n" % (e,))
        f.flush()
    except Exception:
      # never let the watchdog itself wedge — a sample write failure still must kill.
      pass
    sys.stdout.write("WATCHDOG_FIRED label=%s rc=%d sample=%s\n"
                     % (self._label, self._rc, self._sample_path))
    sys.stdout.flush()
    os._exit(self._rc)   # hard exit: teardown is exactly what we do not trust here


class armed(object):
  """Context manager: `with armed(30, path): body()` samples+kills if body runs past
  the deadline; a clean exit disarms it."""

  def __init__(self, deadline_s, sample_path=None, rc=WATCHDOG_RC, label="watchdog"):
    self._wd = Watchdog(deadline_s, sample_path=sample_path, rc=rc, label=label)

  def __enter__(self):
    return self._wd.arm()

  def __exit__(self, *exc):
    self._wd.disarm()
    return False
