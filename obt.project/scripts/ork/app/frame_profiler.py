#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
FrameProfilerComponent - Real-time frame timing overlay

An ApplicationComponent that creates a GraphView overlay covering most
of the primary window, showing scrolling line plots of frame timing metrics.
Toggle visibility with ~ key.

Usage:
    from ork.app.frame_profiler import FrameProfilerComponent

    class MyApp(ComponentizedApplication):
      def __init__(self):
        super().__init__()
        self.profiler = self.addComponent("profiler", FrameProfilerComponent)
        # ... rest of app setup
"""

from orkengine.core import vec3, vec4
from orkengine import lev2
from ork.app.application import ApplicationComponent

################################################################################

class FrameProfilerComponent(ApplicationComponent):

  MAX_SAMPLES = 250  # ~4 seconds at 60fps

  # Series colors (SteamVR-inspired)
  COLOR_FRAME_TIME = vec3(1.0, 1.0, 1.0)       # white
  COLOR_GPU_UPDATE = vec3(0.3, 1.0, 0.3)       # green
  COLOR_UPDATE = vec3(0.3, 1.0, 1.0)           # cyan
  COLOR_IDLE = vec3(0.2, 0.2, 0.2)              # dark grey
  COLOR_ENQUEUE = vec3(0.0, 0.502, 0.753)         # blue
  COLOR_PRESENT = vec3(1.0, 1.0, 0.0)             # yellow
  COLOR_SECONDARY_RENDER = vec3(0.375, 0.1875, 0.375)  # purple
  COLOR_SECONDARY_PRESENT = vec3(1.0, 0.5, 0.0)       # orange

  def __init__(self):
    super().__init__()
    self.graphview = None
    self.series_frame_time = None
    self.series_gpu_update = None
    self.series_update = None
    self.series_enqueue = None
    self.series_present = None
    self.series_update_idle = None
    self.series_sec_enqueue = []
    self.series_sec_present = []

  ##############################################

  def _onGpuInit(self, ctx):
    lg_group = self.app.ezapp.topLayoutGroup

    # Create overlay container with GraphView inside
    self.overlay_group = lev2.ui.LayoutGroup.create("profiler_overlay")
    graphview_item = self.overlay_group.makeChild(
      uiclass=lev2.ui.GraphView,
      args=[],
      fill=True
    )

    self.graphview = graphview_item.widget
    self.graphview.clear_color = vec4(0, 0, 0, 0.8)
    self.graphview.show_stats = False

    # Set as overlay on topLayoutGroup (~ key toggles visibility)
    lg_group.overlay_widget = self.overlay_group

    # Channel 1: "Timing" — line plots for frame_time and update
    timing_channel = self.graphview.channel("Timing")
    timing_channel.color = vec3(1, 1, 1)

    self.series_frame_time = timing_channel.addSeries("frame_time", self.COLOR_FRAME_TIME)
    self.series_frame_time.setMaxSamples(self.MAX_SAMPLES)
    self.series_frame_time.setFixedRange(0.0, 33.0)

    # Channel 2: "UpdateBudget" — stacked area (update vs idle within 2.5ms budget)
    update_channel = self.graphview.channel("UpdateBudget")
    update_channel.stacked = True
    update_channel.lane_bgcolor = vec4(0.05, 0.05, 0.1, 0.8)
    update_channel.lane_outline = True

    self.series_update = update_channel.addSeries("update", self.COLOR_UPDATE)
    self.series_update.setMaxSamples(self.MAX_SAMPLES)
    self.series_update.setFixedRange(0.0, 2.5)

    self.series_update_idle = update_channel.addSeries("idle", self.COLOR_IDLE)
    self.series_update_idle.setMaxSamples(self.MAX_SAMPLES)
    self.series_update_idle.setFixedRange(0.0, 2.5)

    # Channel 3: "FrameBudget" — stacked area chart
    budget_channel = self.graphview.channel("FrameBudget")
    budget_channel.stacked = True
    budget_channel.lane_bgcolor = vec4(0.05, 0.05, 0.1, 0.8)
    budget_channel.lane_outline = True

    self.series_gpu_update = budget_channel.addSeries("gpu_update", self.COLOR_GPU_UPDATE)
    self.series_gpu_update.setMaxSamples(self.MAX_SAMPLES)
    self.series_gpu_update.setFixedRange(0.0, 24.0)

    self.series_enqueue = budget_channel.addSeries("pri_enqueue", self.COLOR_ENQUEUE)
    self.series_enqueue.setMaxSamples(self.MAX_SAMPLES)
    self.series_enqueue.setFixedRange(0.0, 24.0)

    self.series_present = budget_channel.addSeries("pri_present", self.COLOR_PRESENT)
    self.series_present.setMaxSamples(self.MAX_SAMPLES)
    self.series_present.setFixedRange(0.0, 24.0)

    # Secondary render series added dynamically to same channel
    self._budget_channel = budget_channel
    self._updateSecondaryChannels()
    self._reorderBudgetSeries()

  ##############################################

  def _updateSecondaryChannels(self):
    """Create series for any secondary windows that don't have one yet."""
    ezapp = self.app.ezapp
    sec_wins = ezapp.secondaryWindows
    added = False
    while len(self.series_sec_enqueue) < len(sec_wins):
      idx = len(self.series_sec_enqueue)
      s_enq = self._budget_channel.addSeries(f"sec{idx}_enqueue", self.COLOR_SECONDARY_RENDER)
      s_enq.setMaxSamples(self.MAX_SAMPLES)
      s_enq.setFixedRange(0.0, 20.0)
      self.series_sec_enqueue.append(s_enq)
      s_prs = self._budget_channel.addSeries(f"sec{idx}_present", self.COLOR_SECONDARY_PRESENT)
      s_prs.setMaxSamples(self.MAX_SAMPLES)
      s_prs.setFixedRange(0.0, 20.0)
      self.series_sec_present.append(s_prs)
      added = True
    if added:
      self._reorderBudgetSeries()

  ##############################################

  def _reorderBudgetSeries(self):
    """Ensure present is always on top (last in series order)."""
    order = ["gpu_update", "pri_enqueue", "pri_present"]
    for i in range(len(self.series_sec_enqueue)):
      order.append(f"sec{i}_enqueue")
      order.append(f"sec{i}_present")
    self._budget_channel.setSeriesOrder(order)

  ##############################################

  def _onGpuUpdate(self, ctx):
    if not self.graphview:
      return

    ezapp = self.app.ezapp

    # Global metrics (convert to milliseconds)
    self.series_frame_time.addSample(ezapp.perf_frame_duration * 1000.0)
    self.series_gpu_update.addSample(ezapp.perf_gpu_update_duration * 1000.0)
    update_ms = ezapp.perf_update_duration * 1000.0
    self.series_update.addSample(update_ms)
    self.series_update_idle.addSample(max(0.0, 2.5 - update_ms))

    # Primary window render (split into enqueue + present)
    mainwin = ezapp.mainwin
    if mainwin:
      self.series_enqueue.addSample(mainwin.perf_enqueue_duration * 1000.0)
      self.series_present.addSample(mainwin.perf_present_duration * 1000.0)

    # Secondary windows (split into enqueue + present)
    self._updateSecondaryChannels()
    sec_wins = ezapp.secondaryWindows
    for i in range(len(self.series_sec_enqueue)):
      if i < len(sec_wins):
        self.series_sec_enqueue[i].addSample(sec_wins[i].perf_enqueue_duration * 1000.0)
        self.series_sec_present[i].addSample(sec_wins[i].perf_present_duration * 1000.0)

    self.graphview.setDirty()
