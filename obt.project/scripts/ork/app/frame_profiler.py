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
  COLOR_BEGIN_OVERHEAD = vec3(0.2, 0.6, 0.6)    # dim cyan
  COLOR_ACQUIRE_WAIT = vec3(1.0, 0.3, 0.3)      # red
  COLOR_DRAW = vec3(0.0, 0.502, 0.753)          # blue
  COLOR_SUBMIT_OVERHEAD = vec3(0.5, 0.2, 0.6)   # dim purple
  COLOR_FENCE_WAIT = vec3(1.0, 0.5, 0.0)        # orange
  COLOR_SECONDARY_RENDER = vec3(0.375, 0.1875, 0.375)  # purple

  def __init__(self):
    super().__init__()
    self.graphview = None
    self.series_frame_time = None
    self.series_gpu_update = None
    self.series_update = None
    self.series_update_idle = None
    # Primary window 5-band breakdown
    self.series_begin_overhead = None
    self.series_acquire_wait = None
    self.series_draw = None
    self.series_submit_overhead = None
    self.series_fence_wait = None
    # Secondary window breakdown lists
    self.series_sec_begin_overhead = []
    self.series_sec_acquire_wait = []
    self.series_sec_draw = []
    self.series_sec_submit_overhead = []
    self.series_sec_fence_wait = []

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

    # Channel 3: "FrameBudget" — stacked area chart (6-band breakdown)
    budget_channel = self.graphview.channel("FrameBudget")
    budget_channel.stacked = True
    budget_channel.lane_bgcolor = vec4(0.05, 0.05, 0.1, 0.8)
    budget_channel.lane_outline = True

    self.series_gpu_update = budget_channel.addSeries("gpu_update", self.COLOR_GPU_UPDATE)
    self.series_gpu_update.setMaxSamples(self.MAX_SAMPLES)
    self.series_gpu_update.setFixedRange(0.0, 24.0)

    self.series_begin_overhead = budget_channel.addSeries("begin_overhead", self.COLOR_BEGIN_OVERHEAD)
    self.series_begin_overhead.setMaxSamples(self.MAX_SAMPLES)
    self.series_begin_overhead.setFixedRange(0.0, 24.0)

    self.series_acquire_wait = budget_channel.addSeries("acquire_wait", self.COLOR_ACQUIRE_WAIT)
    self.series_acquire_wait.setMaxSamples(self.MAX_SAMPLES)
    self.series_acquire_wait.setFixedRange(0.0, 24.0)

    self.series_draw = budget_channel.addSeries("draw", self.COLOR_DRAW)
    self.series_draw.setMaxSamples(self.MAX_SAMPLES)
    self.series_draw.setFixedRange(0.0, 24.0)

    self.series_submit_overhead = budget_channel.addSeries("submit_overhead", self.COLOR_SUBMIT_OVERHEAD)
    self.series_submit_overhead.setMaxSamples(self.MAX_SAMPLES)
    self.series_submit_overhead.setFixedRange(0.0, 24.0)

    self.series_fence_wait = budget_channel.addSeries("fence_wait", self.COLOR_FENCE_WAIT)
    self.series_fence_wait.setMaxSamples(self.MAX_SAMPLES)
    self.series_fence_wait.setFixedRange(0.0, 24.0)

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
    while len(self.series_sec_draw) < len(sec_wins):
      idx = len(self.series_sec_draw)
      s_bo = self._budget_channel.addSeries(f"sec{idx}_begin_overhead", self.COLOR_BEGIN_OVERHEAD)
      s_bo.setMaxSamples(self.MAX_SAMPLES)
      s_bo.setFixedRange(0.0, 20.0)
      self.series_sec_begin_overhead.append(s_bo)
      s_aq = self._budget_channel.addSeries(f"sec{idx}_acquire_wait", self.COLOR_ACQUIRE_WAIT)
      s_aq.setMaxSamples(self.MAX_SAMPLES)
      s_aq.setFixedRange(0.0, 20.0)
      self.series_sec_acquire_wait.append(s_aq)
      s_dr = self._budget_channel.addSeries(f"sec{idx}_draw", self.COLOR_SECONDARY_RENDER)
      s_dr.setMaxSamples(self.MAX_SAMPLES)
      s_dr.setFixedRange(0.0, 20.0)
      self.series_sec_draw.append(s_dr)
      s_so = self._budget_channel.addSeries(f"sec{idx}_submit_overhead", self.COLOR_SUBMIT_OVERHEAD)
      s_so.setMaxSamples(self.MAX_SAMPLES)
      s_so.setFixedRange(0.0, 20.0)
      self.series_sec_submit_overhead.append(s_so)
      s_fw = self._budget_channel.addSeries(f"sec{idx}_fence_wait", self.COLOR_FENCE_WAIT)
      s_fw.setMaxSamples(self.MAX_SAMPLES)
      s_fw.setFixedRange(0.0, 20.0)
      self.series_sec_fence_wait.append(s_fw)
      added = True
    if added:
      self._reorderBudgetSeries()

  ##############################################

  def _reorderBudgetSeries(self):
    """Ensure series are stacked in correct order."""
    order = ["gpu_update", "begin_overhead", "acquire_wait", "draw", "submit_overhead", "fence_wait"]
    for i in range(len(self.series_sec_draw)):
      order.append(f"sec{i}_begin_overhead")
      order.append(f"sec{i}_acquire_wait")
      order.append(f"sec{i}_draw")
      order.append(f"sec{i}_submit_overhead")
      order.append(f"sec{i}_fence_wait")
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

    # Primary window render (5-band breakdown)
    mainwin = ezapp.mainwin
    if mainwin:
      enqueue = mainwin.perf_enqueue_duration * 1000.0
      beginf = mainwin.perf_beginFrame_duration * 1000.0
      endf = mainwin.perf_endFrame_duration * 1000.0
      acquire = mainwin.perf_acquire_duration * 1000.0
      fence = mainwin.perf_fence_wait_duration * 1000.0

      self.series_begin_overhead.addSample(max(0.0, beginf - acquire))
      self.series_acquire_wait.addSample(acquire)
      self.series_draw.addSample(max(0.0, enqueue - beginf - endf))
      self.series_submit_overhead.addSample(max(0.0, endf - fence))
      self.series_fence_wait.addSample(fence)

    # Secondary windows (5-band breakdown)
    self._updateSecondaryChannels()
    sec_wins = ezapp.secondaryWindows
    for i in range(len(self.series_sec_draw)):
      if i < len(sec_wins):
        sw = sec_wins[i]
        enqueue = sw.perf_enqueue_duration * 1000.0
        beginf = sw.perf_beginFrame_duration * 1000.0
        endf = sw.perf_endFrame_duration * 1000.0
        acquire = sw.perf_acquire_duration * 1000.0
        fence = sw.perf_fence_wait_duration * 1000.0

        self.series_sec_begin_overhead[i].addSample(max(0.0, beginf - acquire))
        self.series_sec_acquire_wait[i].addSample(acquire)
        self.series_sec_draw[i].addSample(max(0.0, enqueue - beginf - endf))
        self.series_sec_submit_overhead[i].addSample(max(0.0, endf - fence))
        self.series_sec_fence_wait[i].addSample(fence)

    self.graphview.setDirty()
