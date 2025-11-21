#!/usr/bin/env ork.python
################################################################################
# GraphView Waveforms Helper
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
################################################################################

"""
WaveformSet - Manages multiple waveform series for GraphView

Handles waveform generation, frequency control UI, and series updates.
Private helper module for graphview.py test.
"""

import math
from orkengine.core import vec3
from orkengine import lev2

################################################################################

class WaveformSet:
  """
  Manages a set of waveforms with automatic UI generation and updates.

  Usage:
      waveforms = WaveformSet({
          'sine': (vec3(1.0, 0.3, 0.3), 0.1),
          'cosine': (vec3(0.3, 1.0, 0.3), 0.1)
      })

      # Create UI controls
      waveforms.createSliders(vpack_widget)

      # Create series in GraphView channel
      waveforms.createSeries(graphview_channel, max_samples=1000)

      # Update each frame
      waveforms.update(time_speed)
  """

  def __init__(self, waveform_defs):
    """
    Args:
        waveform_defs: Dict mapping name -> (color, initial_freq)
    """
    self.waveform_defs = waveform_defs

    # State
    self.phases = {name: 0.0 for name in waveform_defs}
    self.freq_target = {name: freq for name, (_, freq) in waveform_defs.items()}
    self.freq_current = {name: freq for name, (_, freq) in waveform_defs.items()}
    self.freq_smoothing = 0.05

    # Series dict (populated by createSeries)
    self.series = {}

    # Waveform generator functions
    self.generators = {
      'sine': lambda p: math.sin(p),
      'cosine': lambda p: math.cos(p),
      'square': lambda p: 1.0 if math.sin(p) >= 0 else -1.0,
      'sawtooth': lambda p: (p % (2 * math.pi)) / math.pi - 1.0,
      'triangle': lambda p: 2.0 * abs(((p / math.pi) % 2.0) - 1.0) - 1.0
    }

  def createSliders(self, vpack, min_freq=0.01, max_freq=1.0):
    """
    Create frequency control sliders in a VerticalPack widget.

    Args:
        vpack: VerticalPack widget to add sliders to
        min_freq: Minimum frequency
        max_freq: Maximum frequency
    """
    for name, (color, initial_freq) in self.waveform_defs.items():
      slider = vpack.makeChild(
        uiclass=lev2.ui.FloatSlider,
        args=[f"{name.title()} Freq", color, min_freq, max_freq, initial_freq]
      )
      slider.update_on_drag = True
      slider.onValueChanged = lambda w, n=name: self.freq_target.__setitem__(n, w.value)

  def createSeries(self, channel, max_samples=1000, window_size=None):
    """
    Create GraphView series for all waveforms.

    Args:
        channel: GraphView channel to add series to
        max_samples: Ring buffer size
        auto_range: Enable auto-ranging
        window_size: Moving window size (None = show all)
    """
    if window_size is None:
      window_size = max_samples

    for name, (color, _) in self.waveform_defs.items():
      series = self.series[name] = channel.addSeries(name, color)
      series.setMaxSamples(max_samples)
      series.window_size = window_size

  def update(self, time_speed):
    """
    Update phase accumulators and generate samples.

    Args:
        time_speed: Time delta multiplier
    """
    # Interpolate frequencies and update phases
    for name in self.waveform_defs:
      self.freq_current[name] += (self.freq_target[name] - self.freq_current[name]) * self.freq_smoothing
      self.phases[name] += self.freq_current[name] * time_speed

    # Generate samples
    for name in self.waveform_defs:
      if name in self.generators:
        value = self.generators[name](self.phases[name])
        self.series[name].addSample(value)

################################################################################
