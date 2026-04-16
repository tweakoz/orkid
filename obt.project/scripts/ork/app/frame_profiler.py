#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################


"""
FrameProfilerComponent - Real-time frame timing overlay

An ApplicationComponent that creates a ProfilerView overlay covering most
of the primary window, showing scrolling line plots of frame timing metrics.
Toggle visibility with ~ key. Pause/resume with spacebar.

Usage:
    from ork.app.frame_profiler import FrameProfilerComponent

    class MyApp(ComponentizedApplication):
      def __init__(self):
        super().__init__()
        self.profiler = self.addComponent("profiler", FrameProfilerComponent)
"""

from orkengine.core import vec4
from ork.app.application import ApplicationComponent, ComponentizedApplication

################################################################################

class FrameProfilerComponent(ApplicationComponent):

  def __init__(self, channels=ComponentizedApplication.DEFAULT_PROFILER_CHANNELS, **kwargs):
    super().__init__()
    self.profileview = None
    self._channels = channels

  ##############################################

  def _onGpuInit(self, ctx):
    if self.profileview is None:
      return
    for ch in self._channels:
      self.profileview.addChannel(ch)
    self.profileview.clear_color = vec4(0, 0, 0, 0.8)

  ##############################################

  def addEvent(self, channel_name, series_name):
    from orkengine.lev2 import ui
    ui.profiler_add_event(channel_name, series_name)

  ##############################################

  def _onGpuUpdate(self, ctx):
    if self.profileview:
      self.profileview.setDirty()
