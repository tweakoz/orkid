#!/usr/bin/env python3
###############################################################################
# E.6/2.12 gate — material rebind-propagation oracle. The C++ selftest builds
# a material + cached FxPipeline with fabricated param handles and asserts the
# stamp/overlay contract: binds made BEFORE pipeline creation appear on first
# sync, REBINDS made after creation propagate (the pre-2.12 bug), a clean
# stamp skips the overlay (the O(1) hot path), and any new bind re-arms it.
# No GPU work — the full bindParam -> beginBlock -> uniform path is covered by
# the hypermesh paramsink demo + render gates.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  print("running fxpipeline rebind oracle ...", flush=True)
  fails = lev2.fxpipeline_rebind_selftest()

  ezapp.mainThreadEnd()
  passed = (fails == 0)
  print(f"=== fxpipeline rebind gate {'PASSED' if passed else 'FAILED'} ({fails} failures) ===", flush=True)
  ecs.headless_exit()
  sys.exit(0 if passed else 1)


if __name__ == "__main__":
  main()
