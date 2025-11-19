#!/usr/bin/env ork.python

################################################################################
# GraphView Test - Multiple series with math-synthesized data
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
#
# This test demonstrates the enhanced GraphView widget with:
#   - Multiple series (plots) in the same graph
#   - Ring buffer data storage with auto-range
#   - Math-synthesized waveforms (sine, cosine, square, sawtooth)
#   - Real-time data generation
#
# Usage:
#   # Create a GraphView
#   graphview = lev2.ui.GraphView()
#
#   # Get or create a channel
#   channel = graphview.channel("Performance")
#
#   # Add multiple series to the channel
#   sine_series = channel.addSeries("sine", vec3(1.0, 0.3, 0.3))
#   cosine_series = channel.addSeries("cosine", vec3(0.3, 1.0, 0.3))
#
#   # Configure series
#   sine_series.setMaxSamples(100)
#   sine_series.auto_range = True
#
#   # Add data points
#   for i in range(100):
#       sine_series.addSample(math.sin(i * 0.1))
#       cosine_series.addSample(math.cos(i * 0.1))
#
################################################################################

import math
from orkengine.core import vec3, vec4, logger
from orkengine import lev2
from ork.app import application, loggerui
from _graphview_waveforms import WaveformSet

################################################################################

class GraphViewTest(application.ComponentizedApplication):

  def __init__(self):
    super().__init__()

    # Animation state
    self.time = 0.0
    self.time_speed = 0.05

    # Waveform set with definitions: name -> (color, initial_freq)
    self.waveforms = WaveformSet({
      'sine': (vec3(1.0, 0.3, 0.3), 0.1),      # Red
      'cosine': (vec3(0.3, 1.0, 0.3), 0.1),    # Green
      'square': (vec3(0.3, 0.3, 1.0), 0.1),    # Blue
      'sawtooth': (vec3(1.0, 1.0, 0.3), 0.1)   # Yellow
    })

    # FM synthesis parameters for perfItem test
    self.fm_carrier_freq = 0.1
    self.fm_modulator_freq = 0.2
    self.fm_modulation_index = 16.0
    self.fm_phase = 0.0

    ############################################
    # Setup logger UI component
    ############################################

    self.logger = self.addComponent("logger", loggerui.LoggerUIComponent,
                                    filter_regex=[".*"],
                                    background_color=vec4(1.0, 0.0, 0.0, 0.25))

    ############################################
    # Configure EzApp creation args
    ############################################

    self.ezapp_args = {
      'left': 100,
      'top': 100,
      'width': 1200,
      'height': 900,
      'enable_freerun_ups': True,
      'enable_freerun_fps': True
    }

    ############################################
    # Create EzApp and initialize
    ############################################

    self.createEzApp()

  ##############################################

  def _onAppLink(self):
    """Called after all components initialized - configure channels here"""
    # Configure GVIEW channel for perfItem testing
    self.gview_channel = self.logger.configureChannel(
        "GVIEW",
        vec3(0.3, 1.0, 0.8),
        enable_channel=True
    )

    # Set sampling rate to 400 samples/sec (0.0025 second interval)
    self.gview_channel.perf_interval = 0.0001

    # Register pull-based perfItems (lambdas) - sampled at perf_interval rate
    # These will be automatically sampled at 100 Hz
    self.gview_channel.perfItem("fm_wave", lambda: self._getFmWave())
    self.gview_channel.perfItem("carrier", lambda: self._getCarrier())
    self.gview_channel.perfItem("modulator", lambda: self._getModulator())

    print(f"GVIEW channel configured: {self.gview_channel}")
    print(f"GVIEW sampling rate: {1.0/self.gview_channel.perf_interval} Hz")

  ##############################################

  def _onUiInit(self):
    """Initialize main UI layout and widgets"""
    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 4
    lg_group.clearColorStd = vec4(0.5, 0.4, 0.15, 1)
    lg_group.clearColorGuide = vec4(0.7, 0.6, 0.15, 1)

    ############################################
    # Create horizontal split
    # Top: GraphView, Bottom: Frequency Sliders
    ############################################

    # Create horizontal guide at 75% down
    hguide = lg_group.layout.fixedHorizontalGuide(-128)

    # Create GraphView on top
    graphview_item = lg_group.makeChild(
      uiclass=lev2.ui.GraphView,
      args=[]
    )
    graphview_item.layout.top.anchorTo(lg_group.layout.top)
    graphview_item.layout.bottom.anchorTo(hguide)
    graphview_item.layout.left.anchorTo(lg_group.layout.left)
    graphview_item.layout.right.anchorTo(lg_group.layout.right)
    self.graphview = graphview_item.widget
    self.graphview.clear_color = vec4(0, 0, 0, 1)

    # Create VerticalPack on bottom for frequency sliders
    vpack_item = lg_group.makeChild(
      uiclass=lev2.ui.VerticalPack,
      args=["frequency_controls"]
    )
    vpack_item.layout.top.anchorTo(hguide)
    vpack_item.layout.bottom.anchorTo(lg_group.layout.bottom)
    vpack_item.layout.left.anchorTo(lg_group.layout.left)
    vpack_item.layout.right.anchorTo(lg_group.layout.right)
    vpack = vpack_item.widget
    vpack.margin = 2
    vpack.item_height = 24
    vpack.fill = False

    # Create frequency control sliders
    self.waveforms.createSliders(vpack, min_freq=0.01, max_freq=1.0)

    ############################################
    # Create channel and add multiple series
    ############################################

    channel = self.graphview.channel("Waveforms")
    channel.color = vec3(1, 1, 1)

    # Create series for all waveforms
    self.waveforms.createSeries(channel, max_samples=1000, auto_range=True, window_size=1000)

  ##############################################
  # Pull-based perfItem helpers (called by lambdas at perf_interval rate)
  ##############################################

  def _getFmWave(self):
    """FM synthesis: carrier modulated by sine wave"""
    modulator = math.sin(self.fm_phase * self.fm_modulator_freq) * self.fm_modulation_index
    return math.sin(self.fm_phase * self.fm_carrier_freq + modulator)

  def _getCarrier(self):
    """Pure carrier wave"""
    return math.sin(self.fm_phase * self.fm_carrier_freq)

  def _getModulator(self):
    """Normalized modulator signal"""
    modulator = math.sin(self.fm_phase * self.fm_modulator_freq) * self.fm_modulation_index
    return modulator / self.fm_modulation_index

  ##############################################

  def _onUpdate(self, updinfo):
    # Log messages every 120 frames (~2 seconds at 60fps)
    frame_count = int(self.time / self.time_speed)
    if frame_count % 120 == 0:
      self.gview_channel.log(f"Frame {frame_count}: FM synthesis running")
      self.gview_channel.status("synthesis", f"Carrier: {self.fm_carrier_freq:.3f} Hz, Mod: {self.fm_modulator_freq:.3f} Hz")
      self.gview_channel.status("modulation", f"Index: {self.fm_modulation_index:.2f}")

    # Update waveforms
    self.waveforms.update(self.time_speed)

    self.time += self.time_speed

    # Update FM phase (used by pull-based perfItem lambdas)
    self.fm_phase += self.time_speed

    # Note: perfItems are now pull-based (lambdas) - sampled automatically at 100 Hz
    # No need to call perfItem() here anymore!

    # Mark graphview as needing repaint
    self.graphview.setDirty()

###############################################################################

GraphViewTest().ezapp.mainThreadLoop()
