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
from orkengine import lev2
from ork.app.application import ApplicationComponent

################################################################################

class FrameProfilerComponent(ApplicationComponent):

  DEFAULT_CHANNELS = [
    "render_context",
    "gpu",
    "ez_app_main_thread",
    "ez_app_update_thread",
  ]

  def __init__(self, gpu_filter=None, channels=None, **kwargs):
    super().__init__()
    self.graphview = None
    self._gpu_filter = gpu_filter
    self._channels = channels if channels is not None else self.DEFAULT_CHANNELS

  ##############################################

  def _onGpuInit(self, ctx):
    if self.graphview is None:
      lg_group = self.app.ezapp.topLayoutGroup
      self.overlay_group = lev2.ui.LayoutGroup.create("profiler_overlay")
      graphview_item = self.overlay_group.makeChild(
        uiclass=lev2.ui.ProfilerView,
        args=[],
        fill=True
      )
      self.graphview = graphview_item.widget
      lg_group.overlay_widget = self.overlay_group

    for ch in self._channels:
      self.graphview.addChannel(ch)
    self.graphview.clear_color = vec4(0, 0, 0, 0.8)

  ##############################################

  def _onGpuUpdate(self, ctx):
    if self.graphview:
      self.graphview.setDirty()
