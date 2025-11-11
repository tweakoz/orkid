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

import signal
import math
from orkengine.core import vec3, vec4, logger
from orkengine import lev2

################################################################################

class GraphViewTest(object):

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self,
                                      left=100,
                                      top=100,
                                      width=1200,
                                      height=900)

    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 4
    lg_group.clearColorStd = vec4(0.5, 0.4, 0.15, 1)
    lg_group.clearColorGuide = vec4(0.7, 0.6, 0.15, 1)

    ############################################
    # Setup logger UI backend
    ############################################

    # Create UI backend for logger
    self.logger_backend = lev2.ui.LoggerUIBackend.create()
    logger().setBackend(self.logger_backend)
    print("LoggerUIBackend created and set")

    # Create LoggerGroup widget
    self.logger_group = lev2.ui.LoggerGroup.create("logger_ui", ["A*"])
    print("LoggerGroup created")
    print(self.logger_group)
    # Register logger group with backend
    self.logger_group.registerOnBackend(self.logger_backend)
    print("LoggerGroup registered with backend")

    # Add logger group as overlay
    lg_group.overlay_widget = self.logger_group

    ############################################
    # Create horizontal split
    # Top: GraphView, Bottom: TextBox
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

    # Frequency control variables (target and current for smooth interpolation)
    self.sine_freq_target = 0.1
    self.cosine_freq_target = 0.1
    self.square_freq_target = 0.1
    self.sawtooth_freq_target = 0.1

    self.sine_freq_current = 0.1
    self.cosine_freq_current = 0.1
    self.square_freq_current = 0.1
    self.sawtooth_freq_current = 0.1

    self.freq_smoothing = 0.05  # Smoothing factor (0 = no smoothing, 1 = instant)

    # Create sliders for each waveform frequency
    sine_slider = vpack.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Sine Freq", vec3(1.0, 0.3, 0.3), 0.01, 1.0, 0.1]
    )
    sine_slider.update_on_drag = True
    sine_slider.onValueChanged = lambda w: setattr(self, 'sine_freq_target', w.value)

    cosine_slider = vpack.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Cosine Freq", vec3(0.3, 1.0, 0.3), 0.01, 1.0, 0.1]
    )
    cosine_slider.update_on_drag = True
    cosine_slider.onValueChanged = lambda w: setattr(self, 'cosine_freq_target', w.value)

    square_slider = vpack.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Square Freq", vec3(0.3, 0.3, 1.0), 0.01, 1.0, 0.1]
    )
    square_slider.update_on_drag = True
    square_slider.onValueChanged = lambda w: setattr(self, 'square_freq_target', w.value)

    sawtooth_slider = vpack.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Sawtooth Freq", vec3(1.0, 1.0, 0.3), 0.01, 1.0, 0.1]
    )
    sawtooth_slider.update_on_drag = True
    sawtooth_slider.onValueChanged = lambda w: setattr(self, 'sawtooth_freq_target', w.value)

    ############################################
    # Create channel and add multiple series
    ############################################

    channel = self.graphview.channel("Waveforms")
    channel.color = vec3(1, 1, 1)

    # Create 4 different waveform series
    self.sine_series = channel.addSeries("sine", vec3(1.0, 0.3, 0.3))       # Red
    self.cosine_series = channel.addSeries("cosine", vec3(0.3, 1.0, 0.3))   # Green
    self.square_series = channel.addSeries("square", vec3(0.3, 0.3, 1.0))   # Blue
    self.sawtooth_series = channel.addSeries("sawtooth", vec3(1.0, 1.0, 0.3))  # Yellow

    # Configure max samples (ring buffer size)
    for series in [self.sine_series, self.cosine_series, self.square_series, self.sawtooth_series]:
      series.setMaxSamples(1000)  # Large buffer for extensive history
      series.auto_range = True
      series.window_size = 1000  # Moving window: show only most recent 1000 samples

    # Animation state
    self.time = 0.0
    self.time_speed = 0.05

    # Phase accumulators for smooth frequency changes
    self.sine_phase = 0.0
    self.cosine_phase = 0.0
    self.square_phase = 0.0
    self.sawtooth_phase = 0.0

    ############################################
    # Signal handling
    ############################################

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):
    pass

  ##############################################

  def onUpdate(self,updinfo):
    # Smoothly interpolate current frequencies toward target frequencies
    self.sine_freq_current += (self.sine_freq_target - self.sine_freq_current) * self.freq_smoothing
    self.cosine_freq_current += (self.cosine_freq_target - self.cosine_freq_current) * self.freq_smoothing
    self.square_freq_current += (self.square_freq_target - self.square_freq_current) * self.freq_smoothing
    self.sawtooth_freq_current += (self.sawtooth_freq_target - self.sawtooth_freq_current) * self.freq_smoothing

    # Increment phase accumulators by frequency (prevents phase jumps when frequency changes)
    self.sine_phase += self.sine_freq_current * self.time_speed
    self.cosine_phase += self.cosine_freq_current * self.time_speed
    self.square_phase += self.square_freq_current * self.time_speed
    self.sawtooth_phase += self.sawtooth_freq_current * self.time_speed

    # Generate new samples for each waveform using phase accumulators
    self.sine_series.addSample(math.sin(self.sine_phase))
    self.cosine_series.addSample(math.cos(self.cosine_phase))

    # Square wave: alternates between -1 and 1
    square_val = 1.0 if math.sin(self.square_phase) >= 0 else -1.0
    self.square_series.addSample(square_val)

    # Sawtooth wave: linear ramp from -1 to 1
    sawtooth_val = (self.sawtooth_phase % (2 * math.pi)) / math.pi - 1.0
    self.sawtooth_series.addSample(sawtooth_val)

    self.time += self.time_speed

    # Mark graphview as needing repaint
    self.graphview.setDirty()

  ##############################################

  def onUiEvent(self,uievent):
    return lev2.ui.HandlerResult()

###############################################################################

GraphViewTest().ezapp.mainThreadLoop()
