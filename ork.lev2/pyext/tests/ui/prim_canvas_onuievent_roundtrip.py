#!/usr/bin/env ork.python
################################################################################
# Bug-2 headless test: PrimCanvas.onUiEvent must ROUND-TRIP.
#
# The getter used to hardcode py::none(); a setter-stored callable never came
# back, and (under 3.14t free-threaded) calling that None corrupted qsbr thread
# state (_Py_qsbr_attach(qsbr=0x0) segfault). The getter now returns the cached
# handler, so get-then-call works. No GPU/window needed.
################################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys

from orkengine import core   # core before lev2
from orkengine import lev2


def main():
  canvas = lev2.ui.PrimCanvas.wfactory(["roundtrip_canvas"])

  # unset getter returns None
  assert canvas.onUiEvent is None, f"expected None initially, got {canvas.onUiEvent!r}"

  calls = []

  def handler(ev):
    calls.append(ev)
    return lev2.ui.HandlerResult()

  canvas.onUiEvent = handler
  got = canvas.onUiEvent
  assert got is handler, f"onUiEvent did not round-trip: got {got!r}, expected {handler!r}"

  # the round-tripped object must be callable without corrupting thread state
  got(None)
  assert len(calls) == 1, f"round-tripped handler was not callable ({len(calls)} calls)"

  # reassignment replaces cleanly
  def handler2(ev):
    return lev2.ui.HandlerResult()

  canvas.onUiEvent = handler2
  assert canvas.onUiEvent is handler2, "reassignment did not round-trip"

  # clearing with None restores the None getter
  canvas.onUiEvent = None
  assert canvas.onUiEvent is None, f"clear-to-None failed: {canvas.onUiEvent!r}"

  print("=== bug2 onUiEvent round-trip gate PASSED ===", flush=True)
  sys.exit(0)


main()
