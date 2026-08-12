#!/usr/bin/env ork.python
################################################################################
# Player perf-HUD LAYOUT-CONSTANCY gate.
#
# The HUD's page templates (ork.ecs/examples/c++/player/perfhud_pages.h) are the
# thing under test: fixed named slots, one page per topic, a silent sink keeping
# its last value plus an age suffix instead of vanishing. The assertions live in
# C++ (ork.ecs/tests/perfhud_pages.cpp) because the HUD is a player-side header
# with no python binding — this script is the committed runner that turns them
# into a machine verdict:
#
#   * a page's line count is identical whether every sink published or none did,
#   * the rendered line count matches the count the desktop bottom-anchor uses,
#   * a silent slot renders its last value + "@<n>f", a never-published one "--",
#   * row order never moves across alternating full/starved publish patterns,
#   * the ECS row set freezes at the first non-empty systems snapshot.
#
# Falsified: re-introducing the old "drop the row when its sink was silent"
# behavior fails the age + order tests (and the rendered-count check).
#
# No engine boot here (the unit-test exe does its own), so this stays a seconds-
# scale gate; ork.testing.verdict is pure stdlib.
################################################################################

import os
import re
import subprocess
import sys

os.environ["PYTHONUNBUFFERED"] = "1"

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from ork.testing import verdict, Watchdog

EXE = "ork.test.ecs.exe"
PATTERN = "PerfHud!"  # the harness glob: every PerfHud* test
EXPECTED = [
    "PerfHudPageLineCountsConstant",
    "PerfHudSlotAgeSuffix",
    "PerfHudSlotOrderStable",
    "PerfHudEcsRowsFrozenAtBind",
]


def main():
  wd = Watchdog(120.0, label="perfhud_pages_gate").arm()
  proc = subprocess.run([EXE, PATTERN], capture_output=True, text=True)
  wd.disarm()
  # the harness colorizes every field — strip SGR before parsing, or the escape
  # digits get read as the failure count.
  out = re.sub(r"\x1b\[[0-9;]*m", "", proc.stdout + proc.stderr)
  passed = set(re.findall(r"Test:\s+(\w+)\s+Failures:\s+0\s+Status:\s+PASSED", out))
  ran = [name for name in EXPECTED if name in passed]
  m = re.search(r"Total Failures:\s+(\d+)", out)
  failures = int(m.group(1)) if m else None
  ok = (proc.returncode == 0) and (failures == 0) and (len(ran) == len(EXPECTED))
  code = verdict(
      ok,
      "perfhud pages: %d/%d tests passed, failures=%s rc=%d"
      % (len(ran), len(EXPECTED), failures, proc.returncode))
  if not ok:
    sys.stdout.write(out)
  sys.exit(code)


if __name__ == "__main__":
  main()
