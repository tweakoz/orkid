#!/usr/bin/env ork.python
################################################################################
# D1 teardown-exit gate (JUL24).
#
# D1 was: an offscreen ComponentizedApplication SIGSEGV'd during teardown AFTER
# the capture was written and the verdict flushed (rc=139). gdb pinned it on the
# radiance-map XIR decode — a concurrent-queue op that routinely outlives the
# frame that requested it — reaching gloadercontext AFTER stopLoaderThread()
# released it (radiancemaps_asset.cpp submitLoadingPhase on a null context).
#
# That crash is INVISIBLE to every other gate: the verdict-before-teardown
# protocol deliberately prints pass/fail first, and read_verdict classifies
# PASS-with-rc!=0 as PASS_WITH_TEARDOWN_BUG (tracked, not a failure). So nothing
# in the battery FAILS if the teardown crash comes back. This gate is the
# tripwire — it runs a real offscreen capture app as a CHILD process and scores
# the child's EXIT, which is the one signal the tolerant classifier drops.
#
# PASS requires, for every child run:
#   * rc == 0 (no SIGSEGV/abort on the way out), and
#   * the child's post-mainThreadLoop "CAPTURE=... sha256=..." line is present —
#     D1's other signature was that this line never printed, because the crash
#     landed between the last frame and the report.
#
# canary_probe.py is the subject: the cheapest committed scene that reproduced
# D1 100% of the time before the fix (probe capture + skybox XIR load).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import subprocess
import sys
import tempfile

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from ork.testing import ensure_parent_dir, verdict

HERE = os.path.dirname(os.path.abspath(__file__))
SUBJECT = os.path.join(HERE, "canary_probe.py")
RUNS = 2          # two runs: teardown races are timing-sensitive, one is thin
TIMEOUT = 180.0   # child boots a GPU scene; a hang is a failure, not a wait


def _run_child(out_png):
  ensure_parent_dir(out_png)
  try:
    proc = subprocess.run([sys.executable, SUBJECT, out_png],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          timeout=TIMEOUT)
  except subprocess.TimeoutExpired:
    return None, ""
  return proc.returncode, proc.stdout.decode("utf-8", "replace")


def main():
  if not os.path.isfile(SUBJECT):
    return verdict(False, "subject scene missing: %s" % SUBJECT)

  outdir = tempfile.mkdtemp(prefix="teardown_rc_gate_")
  results = []
  ok = True
  for i in range(RUNS):
    out_png = os.path.join(outdir, "probe_%d.png" % i)
    rc, log = _run_child(out_png)
    captured = "CAPTURE=" in log
    wrote = os.path.isfile(out_png) and os.path.getsize(out_png) > 0
    run_ok = (rc == 0) and captured and wrote
    ok = ok and run_ok
    results.append("run%d rc=%s capture_line=%d file=%d" %
                   (i, "TIMEOUT" if rc is None else rc, int(captured), int(wrote)))
    if not run_ok:
      # the child's own tail is the only evidence of WHERE it died — keep it.
      sys.stdout.write(log[-4000:] if log else "(no child output)\n")
      sys.stdout.flush()

  return verdict(ok, "teardown exit gate | %s | %s" % (" | ".join(results), SUBJECT))


if __name__ == "__main__":
  sys.exit(main())
