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
Toggle visibility with ~ key. Pause/resume with spacebar.

Usage:
    from ork.app.frame_profiler import FrameProfilerComponent, EVENT_NOTE_ON

    class MyApp(ComponentizedApplication):
      def __init__(self):
        super().__init__()
        # Enable all sections (default)
        self.profiler = self.addComponent("profiler", FrameProfilerComponent)
        # Or enable specific sections + per-channel event markers
        self.profiler = self.addComponent("profiler", FrameProfilerComponent,
                                          update=True, gpu=True, audio=True,
                                          events=["AudioThread"])

    # Add event markers from any thread (specify channel name):
    self.profiler.addEvent(EVENT_NOTE_ON, "AudioThread")
    self.profiler.addEvent(EVENT_NOTE_OFF, "AudioThread", color=vec4(1,0.5,0,1))
"""

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ApplicationComponent

tokens = CrcStringProxy()

# Event type constants
EVENT_NOTE_ON = 0
EVENT_NOTE_OFF = 1
EVENT_GENERIC = 2

################################################################################

class FrameProfilerComponent(ApplicationComponent):

  MAX_SAMPLES = 490       # ~8 seconds at 60fps
  EVENT_STRIP_HEIGHT = 16  # pixels reserved below channel for event markers

  # Series colors — defined in HSV for easy hue spacing
  # H=[0..1], S=[0..1], V=[0..1]
  hsv = vec3.fromHsv

  COLOR_GPU_UPDATE = hsv(0.33, 0.65, 0.75)             # green
  COLOR_UPDATE = hsv(0.50, 0.70, 0.90)                  # cyan
  COLOR_IDLE = hsv(0.0, 0.0, 0.20)                      # dark grey
  # Primary window bands (stacking order: bottom → top)
  COLOR_FRAME_SETUP = hsv(0.0, 0.0, 0.92)            # white
  COLOR_ACQUIRE_WAIT = hsv(0.0, 0.70, 0.90)             # red
  COLOR_DRAW = hsv(0.58, 0.55, 0.95)                    # light blue
  COLOR_SUBMIT = hsv(0.14, 0.75, 0.665)                  # yellow (darker)
  COLOR_PRESENT = hsv(0.78, 0.60, 0.85)                  # purple
  COLOR_FENCE_WAIT = hsv(0.0, 0.0, 0.50)                # grey
  # Secondary window bands — offset hues, no repeats
  COLOR_SEC_FRAME_SETUP = hsv(0.55, 0.75, 0.55)      # sage
  COLOR_SEC_ACQUIRE_WAIT = hsv(0.08, 0.70, 0.95)        # orange
  COLOR_SEC_DRAW = hsv(0.44, 0.55, 0.80)                # mint
  COLOR_SEC_SUBMIT = hsv(0.92, 0.50, 0.90)              # pink
  COLOR_SEC_PRESENT = hsv(0.17, 0.55, 0.70)             # khaki
  COLOR_SEC_FENCE_WAIT = hsv(0.21, 0.45, 0.75)          # olive
  # Audio bands
  COLOR_VOICES = hsv(0.60, 0.60, 0.90)                  # blue
  COLOR_EVENTS = hsv(0.12, 0.60, 0.90)                  # gold
  COLOR_EFFECTS = hsv(0.83, 0.55, 0.85)                 # magenta
  COLOR_MIXING = hsv(0.45, 0.55, 0.80)                  # teal
  # Event marker colors
  COLOR_NOTE_ON = vec4(0.2, 0.95, 0.3, 1.0)             # bright green
  COLOR_NOTE_OFF = vec4(0.95, 0.2, 0.2, 1.0)            # bright red
  COLOR_GENERIC = vec4(0.3, 0.6, 0.95, 1.0)             # bright blue

  # Default GPU filter: show everything except fwd:total, ui:top, frame:all
  DEFAULT_GPU_FILTER = ["*", "-fwd:total", "-ui:top", "-frame:all"]

  def __init__(self, update=True, gpu=True, audio=False, overlay=True, events=None, gpu_filter=None):
    super().__init__()
    self._enable_update = update
    self._enable_gpu = gpu
    self._enable_audio = audio
    self._overlay = overlay
    self._gpu_filter = gpu_filter if gpu_filter is not None else self.DEFAULT_GPU_FILTER
    self._event_channels = set(events) if events else set()
    self.graphview = None
    self.series_gpu_update = None
    self.series_update = None
    self.series_update_idle = None
    # Primary window 8-band breakdown
    self.series_frame_setup = None
    self.series_acquire_wait = None
    self.series_draw = None
    self.series_submit = None
    self.series_present = None
    self.series_fence_wait = None
    # Secondary window breakdown lists
    self.series_sec_frame_setup = []
    self.series_sec_acquire_wait = []
    self.series_sec_draw = []
    self.series_sec_submit = []
    self.series_sec_present = []
    self.series_sec_fence_wait = []
    # GPU timestamp series (keyed by perf block name)
    self._gpu_timing_series = {}
    # Audio breakdown
    self.series_voices = None
    self.series_events = None
    self.series_effects = None
    self.series_mixing = None
    # Per-channel event pending buffers (thread-safe via GIL)
    self._pending_events = {}  # channel_name -> [(type, color), ...]
    # Channel objects for event commit
    self._event_channel_objs = {}  # channel_name -> graphchannel_ptr

  ##############################################

  def _onGpuInit(self, ctx):
    if self.graphview is None:
      # No pre-created graphview — create as overlay
      lg_group = self.app.ezapp.topLayoutGroup
      self.overlay_group = lev2.ui.LayoutGroup.create("profiler_overlay")
      graphview_item = self.overlay_group.makeChild(
        uiclass=lev2.ui.GraphView,
        args=[],
        fill=True
      )
      self.graphview = graphview_item.widget
      lg_group.overlay_widget = self.overlay_group

    self.graphview.clear_color = vec4(0, 0, 0, 0.8)
    self.graphview.show_stats = False

    if self._enable_update:
      self._initUpdateChannel()

    if self._enable_gpu:
      self._initMainThreadChannel()
      self._initGpuTimingChannel()

    if self._enable_audio:
      self._initAudioChannel()

  ##############################################

  def _setupChannelEvents(self, name, channel):
    """Enable event markers on a channel if it's in the events list."""
    if name in self._event_channels:
      channel.bottom_margin = self.EVENT_STRIP_HEIGHT
      channel.max_event_samples = self.MAX_SAMPLES
      # Set white arrow images for event types (modulated by vertex color at render time)
      channel.setEventImage(EVENT_NOTE_ON, self._makeArrowImage(direction="down"))
      channel.setEventImage(EVENT_NOTE_OFF, self._makeArrowImage(direction="up"))
      channel.setEventImage(EVENT_GENERIC, self._makeArrowImage(direction="up"))
      self._pending_events[name] = []
      self._event_channel_objs[name] = channel

  ##############################################

  @staticmethod
  def _makeArrowImage(direction="up", size=16):
    """Create a white arrow image (RGBA8) for event marker textures.
    The white pixels get modulated by vertex color at render time."""
    import numpy as np
    S = size
    pixels = np.zeros((S, S, 4), dtype=np.uint8)
    for y in range(S):
      row = y if direction == "up" else (S - 1 - y)
      if row < (S * 10 // 16):
        half_w = (row * 7) // 9
      else:
        half_w = 2
      cx = S // 2
      for x in range(max(0, cx - half_w), min(S, cx + half_w + 1)):
        pixels[y, x] = [255, 255, 255, 255]
    buf = pixels.tobytes()
    return lev2.Image.createFromBuffer(S, S, tokens.RGBA8, buf)

  ##############################################

  def _initUpdateChannel(self):
    update_channel = self.graphview.channel("UpdateThread")
    update_channel.stacked = True
    update_channel.lane_bgcolor = vec4(0.05, 0.05, 0.1, 0.8)
    update_channel.lane_outline = True

    self.series_update = update_channel.addSeries("update", self.COLOR_UPDATE)
    self.series_update.setMaxSamples(self.MAX_SAMPLES)
    self.series_update.setFixedRange(0.0, 3.0)

    self.series_update_idle = update_channel.addSeries("idle", self.COLOR_IDLE)
    self.series_update_idle.setMaxSamples(self.MAX_SAMPLES)
    self.series_update_idle.setFixedRange(0.0, 3.0)

    self._setupChannelEvents("UpdateThread", update_channel)

  ##############################################

  def _initMainThreadChannel(self):
    budget_channel = self.graphview.channel("MainThread")
    budget_channel.stacked = True
    budget_channel.lane_bgcolor = vec4(0.05, 0.05, 0.1, 0.8)
    budget_channel.lane_outline = True

    self.series_gpu_update = budget_channel.addSeries("gpu_update", self.COLOR_GPU_UPDATE)
    self.series_gpu_update.setMaxSamples(self.MAX_SAMPLES)
    self.series_gpu_update.setFixedRange(0.0, 24.0)

    self.series_frame_setup = budget_channel.addSeries("frame_setup", self.COLOR_FRAME_SETUP)
    self.series_frame_setup.setMaxSamples(self.MAX_SAMPLES)
    self.series_frame_setup.setFixedRange(0.0, 24.0)

    self.series_acquire_wait = budget_channel.addSeries("acquire_wait", self.COLOR_ACQUIRE_WAIT)
    self.series_acquire_wait.setMaxSamples(self.MAX_SAMPLES)
    self.series_acquire_wait.setFixedRange(0.0, 24.0)

    self.series_draw = budget_channel.addSeries("draw", self.COLOR_DRAW)
    self.series_draw.setMaxSamples(self.MAX_SAMPLES)
    self.series_draw.setFixedRange(0.0, 24.0)

    self.series_submit = budget_channel.addSeries("submit", self.COLOR_SUBMIT)
    self.series_submit.setMaxSamples(self.MAX_SAMPLES)
    self.series_submit.setFixedRange(0.0, 24.0)

    self.series_present = budget_channel.addSeries("present", self.COLOR_PRESENT)
    self.series_present.setMaxSamples(self.MAX_SAMPLES)
    self.series_present.setFixedRange(0.0, 24.0)

    self.series_fence_wait = budget_channel.addSeries("fence_wait", self.COLOR_FENCE_WAIT)
    self.series_fence_wait.setMaxSamples(self.MAX_SAMPLES)
    self.series_fence_wait.setFixedRange(0.0, 24.0)

    self._budget_channel = budget_channel
    self._updateSecondaryChannels()
    self._reorderBudgetSeries()

    budget_channel.addHLine(8.3, vec3(1, 1, 1), "8.3ms")
    self._setupChannelEvents("MainThread", budget_channel)

  ##############################################

  def _initGpuTimingChannel(self):
    self._gpu_channel = self.graphview.channel("GPU")
    self._gpu_channel.stacked = True
    self._gpu_channel.lane_bgcolor = vec4(0.05, 0.1, 0.05, 0.8)
    self._gpu_channel.lane_outline = True
    self._gpu_channel.addHLine(8.3, vec3(1, 1, 1), "8.3ms")
    self._gpu_hue_index = 0
    # Placeholder so the channel is visible even before any GPU blocks fire
    s = self._gpu_channel.addSeries("(idle)", vec3(0.2, 0.2, 0.2))
    s.setMaxSamples(self.MAX_SAMPLES)
    s.setFixedRange(0.0, 10.0)
    self._gpu_timing_series["(idle)"] = s

  @staticmethod
  def _matchGpuFilter(name, filters):
    """Evaluate a composite filter list against a block name.

    Filter rules are evaluated in order:
      "*"          — include all
      "fwd:*"      — include names starting with "fwd:"
      "-fwd:total" — exclude exact match
      "-fwd:*"     — exclude names starting with "fwd:"
      "fwd:color"  — include exact match

    Last matching rule wins. If no rule matches, the name is excluded.
    """
    from fnmatch import fnmatch
    matched = False
    for rule in filters:
      if rule.startswith("-"):
        if fnmatch(name, rule[1:]):
          matched = False
      else:
        if fnmatch(name, rule):
          matched = True
    return matched

  # Preferred GPU series order (bottom to top in stacked chart)
  GPU_SERIES_ORDER = ["ui:top", "frame:all"]

  def _discoverGpuSeries(self, results):
    """Auto-create series for newly discovered GPU perf blocks."""
    added = False
    for name in results:
      if name in self._gpu_timing_series:
        continue
      if not self._matchGpuFilter(name, self._gpu_filter):
        continue
      hue = (self._gpu_hue_index * 0.618034) % 1.0  # golden ratio spacing
      self._gpu_hue_index += 1
      color = vec3.fromHsv(hue, 0.65, 0.8)
      s = self._gpu_channel.addSeries(name, color)
      s.setMaxSamples(self.MAX_SAMPLES)
      s.setFixedRange(0.0, 10.0)
      self._gpu_timing_series[name] = s
      added = True
    if added:
      self._reorderGpuSeries()

  def _reorderGpuSeries(self):
    """Reorder GPU series: known series first (bottom), then auto-discovered."""
    order = [n for n in self.GPU_SERIES_ORDER if n in self._gpu_timing_series]
    for name in self._gpu_timing_series:
      if name not in order:
        order.append(name)
    self._gpu_channel.setSeriesOrder(order)

  ##############################################

  def _initAudioChannel(self):
    audio_channel = self.graphview.channel("AudioThread")
    audio_channel.stacked = True
    audio_channel.lane_bgcolor = vec4(0.05, 0.05, 0.1, 0.8)
    audio_channel.lane_outline = True

    self.series_mixing = audio_channel.addSeries("mixing", self.COLOR_MIXING)
    self.series_mixing.setMaxSamples(self.MAX_SAMPLES)
    self.series_mixing.setFixedRange(0.0, 16.0)

    self.series_voices = audio_channel.addSeries("voices", self.COLOR_VOICES)
    self.series_voices.setMaxSamples(self.MAX_SAMPLES)
    self.series_voices.setFixedRange(0.0, 16.0)

    self.series_events = audio_channel.addSeries("events", self.COLOR_EVENTS)
    self.series_events.setMaxSamples(self.MAX_SAMPLES)
    self.series_events.setFixedRange(0.0, 16.0)

    self.series_effects = audio_channel.addSeries("effects", self.COLOR_EFFECTS)
    self.series_effects.setMaxSamples(self.MAX_SAMPLES)
    self.series_effects.setFixedRange(0.0, 16.0)

    audio_channel.addHLine(10.0, vec3(1, 1, 1), "10ms")
    self._setupChannelEvents("AudioThread", audio_channel)

  ##############################################

  def _updateSecondaryChannels(self):
    """Create/remove series to match the current secondary window count."""
    ezapp = self.app.ezapp
    sec_wins = ezapp.secondaryWindows
    num_sec = len(sec_wins)
    changed = False

    # Remove series for closed secondary windows
    while len(self.series_sec_draw) > num_sec:
      idx = len(self.series_sec_draw) - 1
      self._budget_channel.removeSeries(f"sec{idx}_frame_setup")
      self._budget_channel.removeSeries(f"sec{idx}_acquire_wait")
      self._budget_channel.removeSeries(f"sec{idx}_draw")
      self._budget_channel.removeSeries(f"sec{idx}_submit")
      self._budget_channel.removeSeries(f"sec{idx}_present")
      self._budget_channel.removeSeries(f"sec{idx}_fence_wait")
      self.series_sec_frame_setup.pop()
      self.series_sec_acquire_wait.pop()
      self.series_sec_draw.pop()
      self.series_sec_submit.pop()
      self.series_sec_present.pop()
      self.series_sec_fence_wait.pop()
      # Remove GPU timing series for this secondary window
      prefix = f"sec{idx}:"
      to_remove = [n for n in self._gpu_timing_series if n.startswith(prefix)]
      for name in to_remove:
        self._gpu_channel.removeSeries(name)
        del self._gpu_timing_series[name]
      changed = True

    # Add series for new secondary windows
    while len(self.series_sec_draw) < num_sec:
      idx = len(self.series_sec_draw)
      s_bo = self._budget_channel.addSeries(f"sec{idx}_frame_setup", self.COLOR_SEC_FRAME_SETUP)
      s_bo.setMaxSamples(self.MAX_SAMPLES)
      s_bo.setFixedRange(0.0, 20.0)
      self.series_sec_frame_setup.append(s_bo)
      s_aq = self._budget_channel.addSeries(f"sec{idx}_acquire_wait", self.COLOR_SEC_ACQUIRE_WAIT)
      s_aq.setMaxSamples(self.MAX_SAMPLES)
      s_aq.setFixedRange(0.0, 20.0)
      self.series_sec_acquire_wait.append(s_aq)
      s_dr = self._budget_channel.addSeries(f"sec{idx}_draw", self.COLOR_SEC_DRAW)
      s_dr.setMaxSamples(self.MAX_SAMPLES)
      s_dr.setFixedRange(0.0, 20.0)
      self.series_sec_draw.append(s_dr)
      s_su = self._budget_channel.addSeries(f"sec{idx}_submit", self.COLOR_SEC_SUBMIT)
      s_su.setMaxSamples(self.MAX_SAMPLES)
      s_su.setFixedRange(0.0, 20.0)
      self.series_sec_submit.append(s_su)
      s_pr = self._budget_channel.addSeries(f"sec{idx}_present", self.COLOR_SEC_PRESENT)
      s_pr.setMaxSamples(self.MAX_SAMPLES)
      s_pr.setFixedRange(0.0, 20.0)
      self.series_sec_present.append(s_pr)
      s_fw = self._budget_channel.addSeries(f"sec{idx}_fence_wait", self.COLOR_SEC_FENCE_WAIT)
      s_fw.setMaxSamples(self.MAX_SAMPLES)
      s_fw.setFixedRange(0.0, 20.0)
      self.series_sec_fence_wait.append(s_fw)
      changed = True

    if changed:
      self._reorderBudgetSeries()

  ##############################################

  def _reorderBudgetSeries(self):
    """Ensure series are stacked in correct order."""
    order = ["gpu_update", "frame_setup", "acquire_wait", "draw", "fence_wait", "present", "submit"]
    for i in range(len(self.series_sec_draw)):
      order.append(f"sec{i}_frame_setup")
      order.append(f"sec{i}_acquire_wait")
      order.append(f"sec{i}_draw")
      order.append(f"sec{i}_fence_wait")
      order.append(f"sec{i}_present")
      order.append(f"sec{i}_submit")
    self._budget_channel.setSeriesOrder(order)

  ##############################################

  def addEvent(self, event_type, channel, color=None):
    """Add an event marker at the current sample position.

    Thread-safe — events are buffered and committed on the GPU thread.

    Args:
      event_type: EVENT_NOTE_ON, EVENT_NOTE_OFF, or EVENT_GENERIC
      channel: channel name (e.g. "AudioThread", "UpdateThread", "GPU")
      color: optional vec4 tint (uses default color for event type if None)
    """
    pending = self._pending_events.get(channel)
    if pending is not None:
      if color is None:
        if event_type == EVENT_NOTE_ON:
          color = self.COLOR_NOTE_ON
        elif event_type == EVENT_NOTE_OFF:
          color = self.COLOR_NOTE_OFF
        else:
          color = self.COLOR_GENERIC
      pending.append((event_type, color))

  ##############################################

  def _onGpuUpdate(self, ctx):
    if not self.graphview or self.graphview.paused:
      return

    ezapp = self.app.ezapp

    if self._enable_update:
      update_ms = ezapp.perf_update_duration * 1000.0
      self.series_update.addSample(update_ms)
      self.series_update_idle.addSample(max(0.0, 2.5 - update_ms))

    if self._enable_gpu:
      self.series_gpu_update.addSample(ezapp.perf_gpu_update_duration * 1000.0)
      results = ctx.gpu_profiler_results

      mainwin = ezapp.mainwin
      if mainwin:
        enqueue = mainwin.perf_enqueue_duration * 1000.0
        beginf = mainwin.perf_beginFrame_duration * 1000.0
        endf = mainwin.perf_endFrame_duration * 1000.0
        acquire = mainwin.perf_acquire_duration * 1000.0
        fence = mainwin.perf_fence_wait_duration * 1000.0
        submit = mainwin.perf_submit_duration * 1000.0
        present = mainwin.perf_present_vk_duration * 1000.0

        self.series_frame_setup.addSample(max(0.0, beginf - acquire))
        self.series_acquire_wait.addSample(acquire)
        self.series_draw.addSample(max(0.0, enqueue - beginf - endf))
        self.series_submit.addSample(submit)
        self.series_present.addSample(present)
        self.series_fence_wait.addSample(fence)

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
          submit = sw.perf_submit_duration * 1000.0
          present = sw.perf_present_vk_duration * 1000.0

          self.series_sec_frame_setup[i].addSample(max(0.0, beginf - acquire))
          self.series_sec_acquire_wait[i].addSample(acquire)
          self.series_sec_draw[i].addSample(max(0.0, enqueue - beginf - endf))
          self.series_sec_submit[i].addSample(submit)
          self.series_sec_present[i].addSample(present)
          self.series_sec_fence_wait[i].addSample(fence)

      # GPU timestamp results from C++ instrumentation (dynamic discovery)
      # Merge secondary window GPU results with "secN:" prefix
      merged_results = dict(results) if results else {}
      for i, sw in enumerate(sec_wins):
        sec_ctx = sw.gfx_context
        if sec_ctx:
          sec_results = sec_ctx.gpu_profiler_results
          if sec_results:
            for name, val in sec_results.items():
              merged_results[f"sec{i}:{name}"] = val

      if merged_results:
        self._discoverGpuSeries(merged_results)
      for name, series in self._gpu_timing_series.items():
        if name in merged_results:
          series.currentValue = merged_results[name] * 1000.0  # seconds -> ms
        series.addSample(series.currentValue, False)

    if self._enable_audio:
      synth = ezapp.audio_synth
      if synth:
        self.series_voices.addSample(synth.perf_voices_duration * 1000.0)
        self.series_events.addSample(synth.perf_events_duration * 1000.0)
        self.series_effects.addSample(synth.perf_effects_duration * 1000.0)
        self.series_mixing.addSample(synth.perf_mixing_duration * 1000.0)

    # Commit buffered events to C++ channels, then advance frame
    for ch_name, ch_obj in self._event_channel_objs.items():
      pending = self._pending_events[ch_name]
      for event_type, color in pending:
        ch_obj.addEvent(event_type, color)
      pending.clear()
      ch_obj.commitEventFrame()

    self.graphview.setDirty()
