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
from orkengine.core import vec3, vec4
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
    lg_group.clearColorGuide = vec4(0.1, 0.1, 0.15, 1)

    ############################################
    # Create GraphView as direct child of layout group
    ############################################

    graphview_item = lg_group.makeChild(
      uiclass=lev2.ui.GraphView,
      args=[]
    )

    # Apply full-window bounds
    graphview_item.layout.fill(lg_group.layout)
    self.graphview = graphview_item.widget

    # Set black background
    self.graphview.clear_color = vec4(0, 0, 0, 1)

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
      series.setMaxSamples(200)
      series.auto_range = True

    # Animation state
    self.time = 0.0
    self.time_speed = 0.05

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
    # Generate new samples for each waveform
    self.sine_series.addSample(math.sin(self.time))
    self.cosine_series.addSample(math.cos(self.time))

    # Square wave: alternates between -1 and 1
    square_val = 1.0 if math.sin(self.time) >= 0 else -1.0
    self.square_series.addSample(square_val)

    # Sawtooth wave: linear ramp from -1 to 1
    sawtooth_val = (self.time % (2 * math.pi)) / math.pi - 1.0
    self.sawtooth_series.addSample(sawtooth_val)

    self.time += self.time_speed

    # Mark graphview as needing repaint
    self.graphview.setDirty()

  ##############################################

  def onUiEvent(self,uievent):
    return lev2.ui.HandlerResult()

###############################################################################

GraphViewTest().ezapp.mainThreadLoop()
