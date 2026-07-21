################################################################################
# ork.testing.verdict — the VERDICT-BEFORE-TEARDOWN protocol.
#
# WHY (bug class #57): teardown can SIGSEGV AFTER the frames have rendered and the
# pass/fail is already known — the pixels were correct, only the shutdown crashed.
# A gate that keys off process rc alone would score that PASS run as a failure. The
# protocol fixes this by decoupling the verdict from the exit:
#
#   1. The test computes pass/fail from what it OBSERVED (frames, extents, pixels).
#   2. It calls verdict(passed, detail) — which prints ONE machine-readable line,
#      FLUSHED immediately, BEFORE any teardown begins.
#   3. Only then does teardown run. If teardown survives, the process exit code
#      reflects the verdict (0 pass / 1 fail). If teardown SIGSEGVs, the verdict
#      line is ALREADY on stdout, so a reader can tell "the test passed, the engine
#      crashed on the way out" apart from "the test failed".
#
# read_verdict() is the reader side for gate-runners: it pairs the emitted verdict
# with the process rc and classifies:
#
#   verdict PASS + rc == 0   -> "PASS"
#   verdict PASS + rc != 0   -> "PASS_WITH_TEARDOWN_BUG"   (tracked, NOT a failure)
#   verdict FAIL             -> "FAIL"
#   no verdict + rc == 0     -> "NO_VERDICT"                (test never spoke)
#   no verdict + rc != 0     -> "CRASH_NO_VERDICT"          (died before verdict)
#
# Pure stdlib on purpose: a gate-runner classifies logs WITHOUT booting the engine.
################################################################################

import os
import re
import sys

VERDICT_PREFIX = "TESTVERDICT"

# classification tokens (also the return values of read_verdict when rc is supplied)
PASS = "PASS"
FAIL = "FAIL"
PASS_WITH_TEARDOWN_BUG = "PASS_WITH_TEARDOWN_BUG"
NO_VERDICT = "NO_VERDICT"
CRASH_NO_VERDICT = "CRASH_NO_VERDICT"

_VERDICT_RE = re.compile(r"^%s=(PASS|FAIL)\b" % re.escape(VERDICT_PREFIX), re.MULTILINE)


def verdict(passed, detail=""):
  """Emit the verdict line (flushed) BEFORE teardown, and return the intended exit
  code (0 pass / 1 fail). The caller runs teardown AFTER this returns, then exits
  with this code — so a clean run's rc reflects the verdict while a teardown crash
  leaves the already-printed verdict for read_verdict to reconcile (#57)."""
  tag = PASS if passed else FAIL
  line = "%s=%s detail=%s" % (VERDICT_PREFIX, tag, detail)
  # stdout, explicit flush: the line MUST survive a teardown SIGSEGV that follows.
  sys.stdout.write(line + "\n")
  sys.stdout.flush()
  return 0 if passed else 1


def read_verdict(log_text_or_path, rc=None):
  """Return the verdict a run emitted. `log_text_or_path` is either raw log text or
  a path to a log file. With rc=None, returns the raw token "PASS"/"FAIL"/None. With
  rc supplied, returns the full classification (PASS / FAIL / PASS_WITH_TEARDOWN_BUG /
  NO_VERDICT / CRASH_NO_VERDICT) per the module-top table."""
  text = log_text_or_path
  if isinstance(text, str) and os.path.sep in text and os.path.isfile(text):
    with open(text, "r") as f:
      text = f.read()
  elif isinstance(text, str) and os.path.isfile(text):
    with open(text, "r") as f:
      text = f.read()

  matches = _VERDICT_RE.findall(text or "")
  raw = matches[-1] if matches else None   # LAST verdict wins (a run may retry)

  if rc is None:
    return raw

  if raw == PASS:
    return PASS if int(rc) == 0 else PASS_WITH_TEARDOWN_BUG
  if raw == FAIL:
    return FAIL
  return NO_VERDICT if int(rc) == 0 else CRASH_NO_VERDICT
