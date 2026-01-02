#!/usr/bin/env ork.python
################################################################################
# PrimCanvas Clock Widget Test
# Tests AnalogClock widget using standard UI instantiation
################################################################################

import signal
from orkengine.core import vec4
from orkengine import lev2
from ork.ui.analog_clock import AnalogClock

################################################################################

class ClockTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=True, name = "Clock")
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg = self.ezapp.topLayoutGroup
    lg.clearColorStd = vec4(0.1, 0.1, 0.12, 1)

    # Create clock using standard widget factory
    cl = lg.makeChild(uiclass=AnalogClock, args=["clock"])

    # Anchor to fill parent
    for edge in ['top', 'left', 'bottom', 'right']:
      getattr(cl.layout, edge).anchorTo(getattr(lg.layout, edge))

    signal.signal(signal.SIGINT, lambda *_: self.ezapp.signalExit())

  def onUiEvent(self, ev):
    return lev2.ui.HandlerResult()

################################################################################

ClockTest().ezapp.mainThreadLoop()
