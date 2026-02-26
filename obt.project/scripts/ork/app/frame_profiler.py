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
of the primary window, showing line plots of frame timing metrics sourced
directly from the C++ ork::ProfilerChannel data on the rendering Context.
Toggle visibility with ~ key.

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

  def __init__(self):
    super().__init__()
    self.graphview = None

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

    self.graphview.clear_color = vec4(0, 0, 0, 0.8)
    self.graphview.setContext(ctx)

  ##############################################

  def _onGpuUpdate(self, ctx):
    if self.graphview:
      self.graphview.setDirty()
