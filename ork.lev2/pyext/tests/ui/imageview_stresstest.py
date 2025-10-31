#!/usr/bin/env ork.python

################################################################################
# ImageView/ImageProvider Stress Test
# 4x4 grid (16 ImageViews) with mixed movie players and matplotlib plots
# Designed to stress test async texture uploads and reproduce ping-pong buffer bugs
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, signal, random, threading, time
import numpy as np
from obt import path
from orkengine.core import vec2, vec3, vec4, mtx4, quat, VarMap, CrcStringProxy
from orkengine import lev2
import matplotlib.pyplot as plt
from matplotlib.backends.backend_agg import FigureCanvasAgg

tokens = CrcStringProxy()

################################################################################

class ImageViewStressTest(object):

  def __init__(self):
    super().__init__()
    self.abstime = 0.0

    self.ezapp = lev2.OrkEzApp.create(self,
                                      fullscreen=False,
                                      enable_audio=False,
                                      enable_audio_output=False,
                                      enable_audio_synth=False)

    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorGuide = vec4(1, 1, 0, 1)  # Bright yellow
    self.lg_group = lg_group
    lg_group.margin = 4

    ############################################
    # Create 4x4 grid of ImageViews
    ############################################

    self.griditems = lg_group.makeGrid(
      width=4,
      height=4,
      margin=4,
      uiclass=lev2.ui.Box,
      args=["placeholder", vec4(0.1, 0.1, 0.1, 1)],
    )

    ############################################
    # Replace each grid cell with an ImageView
    ############################################

    self.imageviews = []
    for i in range(16):
      imv = lg_group.makeChild(uiclass=lev2.ui.ImageView, args=[f"imgview_{i}"])
      self.lg_group.replaceChild(self.griditems[i].layout, imv)
      imv_widget = imv.widget
      imv_widget.maintain_aspect_ratio = True
      imv_widget.generate_mipmaps = False
      self.imageviews.append(imv_widget)

    ############################################
    # Movie state tracking
    ############################################

    self.movies = []
    self.movie_start_times = []
    self.movie_started = []

    ############################################
    # Matplotlib state
    ############################################

    self.mpl_figures = []
    self.mpl_canvases = []
    self.mpl_axes = []
    self.mpl_latest_images = [None] * 8
    self.mpl_threads = []
    self.mpl_running = True

    ############################################

    plt.style.use('dark_background')

    ############################################

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.mpl_running = False
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def createMatplotlibPlot(self, index, fm_params):
    """Create a matplotlib figure for a specific FM synthesis plot"""
    fig = plt.figure(figsize=(3, 3), dpi=100)
    canvas = FigureCanvasAgg(fig)
    ax = fig.add_subplot(111)

    self.mpl_figures.append(fig)
    self.mpl_canvases.append(canvas)
    self.mpl_axes.append(ax)

    return fig, canvas, ax, fm_params

  ##############################################

  def imageProviderMatPlotLib(self, index):
    """Provide images from matplotlib - called from lambda"""
    return self.mpl_latest_images[index]

  ##############################################

  def renderMatplotlibPlot(self, index, fig, canvas, ax, fm_params):
    """Render matplotlib plot with FM synthesis - runs in thread"""
    while self.mpl_running:
      try:
        t = self.abstime

        # FM Synthesis equation
        x = np.linspace(0, 4 * np.pi, 200)
        y = np.sin(fm_params['carrier_freq'] * x + t * fm_params['carrier_phase'])

        # Add modulators
        for mod in fm_params['modulators']:
          mod_signal = mod['amplitude'] * np.sin(
            mod['freq'] * x + t * mod['phase'] + mod.get('offset', 0)
          )
          y += mod_signal

        # Update plot
        ax.clear()
        ax.plot(x, y, color=fm_params['color'], linewidth=2)
        ax.set_ylim(fm_params['ylim'])
        ax.set_title(fm_params['title'], fontsize=10)
        ax.set_xlabel('x', fontsize=8)
        ax.set_ylabel('FM', fontsize=8)
        ax.grid(True, alpha=0.3)

        # Render to canvas
        canvas.draw()

        # Convert to ork Image
        rgba_buf = np.asarray(canvas.buffer_rgba())
        rgb = rgba_buf[:, :, :3].astype(np.float32)
        magnitude = np.sqrt(np.sum(rgb**2, axis=2))
        magnitude = np.clip(magnitude / (255 * np.sqrt(3)), 0, 1)
        rgba_buf[:, :, 3] = (magnitude * 255).astype(np.uint8)

        w = rgba_buf.shape[1]
        h = rgba_buf.shape[0]
        image = lev2.Image.createFromBuffer(w, h, tokens.RGBA8, rgba_buf)
        self.mpl_latest_images[index] = image

        time.sleep(1.0 / 60.0)  # 60 fps

      except Exception as e:
        print(f"Matplotlib thread {index} error: {e}")
        time.sleep(0.1)

  ##############################################

  def onGpuInit(self, ctx):

    print("=" * 80)
    print("ImageView Stress Test - 4x4 Grid")
    print("8 Movie Players + 8 Matplotlib FM Plots")
    print("=" * 80)

    ############################################
    # Setup 8 Movie Players with Delayed Starts
    # Layout: Movies in slots 0,1,4,5,8,9,12,13
    ############################################

    movie_configs = [
      # (slot_index, movie_file, start_delay)
      (0, "bunny.mp4", 0.0),
      (1, "wipeout.mp4", 1.5),
      (4, "bunny.mp4", 3.0),
      (5, "wipeout.mp4", 4.5),
      (8, "bunny.mp4", 6.0),
      (9, "wipeout.mp4", 7.5),
      (12, "bunny.mp4", 9.0),
      (13, "wipeout.mp4", 10.5),
    ]

    for slot_idx, movie_file, start_delay in movie_configs:
      movie = lev2.MoviePlaybackContext()
      movie_path = str(path.stage() / "assetcache" / "movies" / movie_file)
      movie.init(movie_path)
      provider = movie.createImageProvider()
      self.imageviews[slot_idx].image = provider

      self.movies.append(movie)
      self.movie_start_times.append(start_delay)
      self.movie_started.append(False)

      print(f"[Movie {len(self.movies)-1}] Slot {slot_idx:2d}: {movie_file:15s} (start @ {start_delay:.1f}s)")

    ############################################
    # Setup 8 Matplotlib FM Synthesis Plots
    # Layout: Plots in slots 2,3,6,7,10,11,14,15
    ############################################

    fm_configs = [
      # Slot 2: Simple 2-modulator FM
      {
        'title': 'FM: 2-Mod Low',
        'carrier_freq': 1.0,
        'carrier_phase': 1.0,
        'modulators': [
          {'freq': 2.0, 'amplitude': 0.5, 'phase': 0.5, 'offset': 0},
          {'freq': 3.5, 'amplitude': 0.3, 'phase': 1.0, 'offset': 0},
        ],
        'ylim': (-2, 2),
        'color': 'cyan',
      },
      # Slot 3: Complex 3-modulator FM
      {
        'title': 'FM: 3-Mod Med',
        'carrier_freq': 1.5,
        'carrier_phase': 0.8,
        'modulators': [
          {'freq': 2.5, 'amplitude': 0.4, 'phase': 0.3, 'offset': 0},
          {'freq': 4.0, 'amplitude': 0.35, 'phase': 0.6, 'offset': np.pi/4},
          {'freq': 5.5, 'amplitude': 0.25, 'phase': 1.2, 'offset': np.pi/2},
        ],
        'ylim': (-2.5, 2.5),
        'color': 'yellow',
      },
      # Slot 6: High frequency single modulator
      {
        'title': 'FM: 1-Mod High',
        'carrier_freq': 2.0,
        'carrier_phase': 1.5,
        'modulators': [
          {'freq': 8.0, 'amplitude': 0.8, 'phase': 2.0, 'offset': 0},
        ],
        'ylim': (-3, 3),
        'color': 'magenta',
      },
      # Slot 7: 4-modulator chaos
      {
        'title': 'FM: 4-Mod Chaos',
        'carrier_freq': 1.2,
        'carrier_phase': 0.5,
        'modulators': [
          {'freq': 1.5, 'amplitude': 0.3, 'phase': 0.2, 'offset': 0},
          {'freq': 3.3, 'amplitude': 0.25, 'phase': 0.7, 'offset': np.pi/3},
          {'freq': 5.7, 'amplitude': 0.2, 'phase': 1.1, 'offset': np.pi/2},
          {'freq': 7.1, 'amplitude': 0.15, 'phase': 1.5, 'offset': np.pi},
        ],
        'ylim': (-2, 2),
        'color': 'orange',
      },
      # Slot 10: Phase-shifted 2-mod
      {
        'title': 'FM: 2-Mod Phase',
        'carrier_freq': 1.3,
        'carrier_phase': 2.0,
        'modulators': [
          {'freq': 2.6, 'amplitude': 0.6, 'phase': 0.4, 'offset': np.pi/6},
          {'freq': 4.2, 'amplitude': 0.4, 'phase': 1.3, 'offset': 2*np.pi/3},
        ],
        'ylim': (-2.5, 2.5),
        'color': 'green',
      },
      # Slot 11: Amplitude-modulated 3-mod
      {
        'title': 'FM: 3-Mod AmpMod',
        'carrier_freq': 1.8,
        'carrier_phase': 0.6,
        'modulators': [
          {'freq': 3.0, 'amplitude': 0.7, 'phase': 0.9, 'offset': 0},
          {'freq': 4.5, 'amplitude': 0.5, 'phase': 1.4, 'offset': np.pi/4},
          {'freq': 6.0, 'amplitude': 0.3, 'phase': 1.8, 'offset': np.pi/2},
        ],
        'ylim': (-3, 3),
        'color': 'red',
      },
      # Slot 14: Asymmetric 2-mod
      {
        'title': 'FM: 2-Mod Asym',
        'carrier_freq': 0.8,
        'carrier_phase': 1.2,
        'modulators': [
          {'freq': 3.7, 'amplitude': 0.9, 'phase': 0.3, 'offset': 0},
          {'freq': 1.3, 'amplitude': 0.4, 'phase': 2.1, 'offset': np.pi/5},
        ],
        'ylim': (-2.5, 2.5),
        'color': 'lime',
      },
      # Slot 15: Complex 5-modulator
      {
        'title': 'FM: 5-Mod Complex',
        'carrier_freq': 1.0,
        'carrier_phase': 0.0,
        'modulators': [
          {'freq': 2.0, 'amplitude': 0.25, 'phase': 0.4, 'offset': 0},
          {'freq': 3.0, 'amplitude': 0.2, 'phase': 0.8, 'offset': np.pi/5},
          {'freq': 4.0, 'amplitude': 0.15, 'phase': 1.2, 'offset': 2*np.pi/5},
          {'freq': 5.0, 'amplitude': 0.12, 'phase': 1.6, 'offset': 3*np.pi/5},
          {'freq': 6.0, 'amplitude': 0.1, 'phase': 2.0, 'offset': 4*np.pi/5},
        ],
        'ylim': (-2, 2),
        'color': 'white',
      },
    ]

    plot_slots = [2, 3, 6, 7, 10, 11, 14, 15]

    for plot_idx, (slot_idx, fm_config) in enumerate(zip(plot_slots, fm_configs)):
      # Create matplotlib plot
      fig, canvas, ax, params = self.createMatplotlibPlot(plot_idx, fm_config)

      # Start rendering thread
      thread = threading.Thread(
        target=self.renderMatplotlibPlot,
        args=(plot_idx, fig, canvas, ax, params)
      )
      thread.daemon = True
      thread.start()
      self.mpl_threads.append(thread)

      # Create image provider
      provider = lev2.ImageProvider.createFromLambda(
        lambda idx=plot_idx: self.imageProviderMatPlotLib(idx)
      )
      self.imageviews[slot_idx].setImageProvider(provider)

      print(f"[Plot  {plot_idx}] Slot {slot_idx:2d}: {fm_config['title']}")

    print("=" * 80)
    print(f"Total: {len(self.movies)} movies + {len(self.mpl_figures)} plots = {len(self.movies) + len(self.mpl_figures)} image sources")
    print("=" * 80)

  ##############################################

  def onUpdate(self, updinfo):
    self.abstime = updinfo.absolutetime

    # Handle delayed movie starts
    for i, (movie, start_time, started) in enumerate(zip(
        self.movies, self.movie_start_times, self.movie_started)):
      if not started and self.abstime >= start_time:
        movie.play()
        self.movie_started[i] = True
        print(f"[T={self.abstime:.2f}s] Started movie {i} (delay={start_time:.1f}s)")

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

ImageViewStressTest().ezapp.mainThreadLoop()
