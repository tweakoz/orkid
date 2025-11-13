#!/usr/bin/env ork.python

################################################################################
# ImageView/ImageProvider Stress Test
# Variable NxN grid (default 4x4 = 16 ImageViews) with mixed movie players and matplotlib plots
# Designed to stress test async texture uploads and reproduce ping-pong buffer bugs
# Usage: ./imageview_stresstest.py [-g N]  (default: -g 4 for 4x4 grid)
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, signal, random, threading, time, argparse
import numpy as np
from obt import path
from orkengine.core import vec2, vec3, vec4, mtx4, quat, VarMap, CrcStringProxy
from orkengine import lev2
from ork.app import application, loggerui
import matplotlib.pyplot as plt
from matplotlib.backends.backend_agg import FigureCanvasAgg

tokens = CrcStringProxy()

################################################################################

class ImageViewStressTest(application.ComponentizedApplication):

  def __init__(self, griddim=4):
    super().__init__()

    # Grid configuration
    self.griddim = griddim
    self.total_cells = griddim * griddim

    # Movie state tracking
    self.movies = []
    self.movie_start_times = []
    self.movie_started = []

    # Matplotlib state
    self.num_plots = self.total_cells // 2
    self.mpl_figures = []
    self.mpl_canvases = []
    self.mpl_axes = []
    self.mpl_latest_images = [None] * self.num_plots
    self.mpl_threads = []
    self.mpl_running = True

    # Matplotlib setup
    plt.style.use('dark_background')

    ############################################
    # Setup logger UI component
    ############################################

    self.addComponent("logger", loggerui.LoggerUIComponent,
                      overlay=True,
                      filter_regex=[".*"],
                      background_color=vec4(0.2, 0.2, 0.2, 0.8))

    ############################################
    # Configure EzApp creation args
    ############################################

    self.ezapp_args = {
      'width': 1600,
      'height': 900,
      'fullscreen': False,
      'enable_audio': False,
      'enable_audio_output': False,
      'enable_audio_synth': False,
      'enable_freerun_ups': True,
      'enable_freerun_fps': True
    }

    ############################################
    # Create EzApp and initialize
    ############################################

    self.createEzApp()

  ##############################################

  def _onUiInit(self):
    """Initialize UI layout and widgets"""
    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorGuide = vec4(1, 1, 0, 1)  # Bright yellow
    lg_group.margin = 2

    ############################################
    # Create NxN grid of ImageViews
    ############################################

    self.griditems = lg_group.makeGrid(
      width=self.griddim,
      height=self.griddim,
      margin=2,
      uiclass=lev2.ui.Box,
      args=["placeholder", vec4(0.1, 0.1, 0.1, 1)],
    )

    ############################################
    # Replace each grid cell with an ImageView
    ############################################

    self.imageviews = []
    for i in range(self.total_cells):
      imv = lg_group.makeChild(uiclass=lev2.ui.ImageView, args=[f"imgview_{i}"])
      lg_group.replaceChild(self.griditems[i].layout, imv)
      imv_widget = imv.widget
      imv_widget.maintain_aspect_ratio = True
      imv_widget.generate_mipmaps = False
      self.imageviews.append(imv_widget)

  ##############################################

  def createMatplotlibPlot(self, index, fm_params):
    """Create a matplotlib figure for a specific FM synthesis plot"""
    # Start with small default size - will be resized to widget dimensions each frame
    fig = plt.figure(figsize=(3, 3), dpi=100, facecolor='#101010')
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

  def renderMatplotlibPlot(self, index, fig, canvas, ax, fm_params, widget):
    """Render matplotlib plot with FM synthesis - runs in thread"""
    x = np.linspace(0, 4 * np.pi, 200)
    while self.mpl_running:
      try:
        t = self.absolutetime

        # Get widget dimensions and resize figure to match
        w = widget.width
        h = widget.height
        if w > 0 and h > 0:
          fig.set_size_inches(w / 100.0, h / 100.0, forward=True)

        # FM Synthesis equation
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

        time.sleep(1.0 / 10.0)  # 60 fps

      except Exception as e:
        print(f"Matplotlib thread {index} error: {e}")
        time.sleep(0.1)

  ##############################################

  def _onAppLink(self):
    """Configure logger channels after component initialization"""
    logger_comp = self.findComponentByName("logger")

    # Configure STRESS channel for stress test events
    self.stress_channel = logger_comp.configureChannel(
        "STRESS",
        vec3(1.0, 0.5, 0.0),  # Orange
        enable_channel=True
    )

    # Configure MOVIE channel for movie playback events
    self.movie_channel = logger_comp.configureChannel(
        "MOVIE",
        vec3(0.3, 0.8, 1.0),  # Cyan
        enable_channel=True
    )

    # Configure PLOT channel for matplotlib events
    self.plot_channel = logger_comp.configureChannel(
        "PLOT",
        vec3(1.0, 0.3, 0.8),  # Magenta
        enable_channel=True
    )

  ##############################################

  def _onGpuInit(self, ctx):
    """Initialize GPU resources - movies and matplotlib plots"""
    self.stress_channel.log("=" * 80)
    self.stress_channel.log(f"ImageView Stress Test - {self.griddim}x{self.griddim} Grid")
    self.stress_channel.log(f"{self.total_cells // 2} Movie Players + {self.total_cells // 2} Matplotlib FM Plots")
    self.stress_channel.log("=" * 80)

    ############################################
    # Generate movie and plot slots in checkerboard pattern
    # Movies get even checkerboard positions, plots get odd positions
    ############################################

    movie_slots = []
    plot_slots = []

    for row in range(self.griddim):
      for col in range(self.griddim):
        slot_idx = row * self.griddim + col
        # Checkerboard: (row + col) % 2 determines pattern
        if (row + col) % 2 == 0:
          movie_slots.append(slot_idx)
        else:
          plot_slots.append(slot_idx)

    ############################################
    # Setup Movie Players with Delayed Starts
    ############################################

    movies_to_use = ["bunny.mp4", "wipeout.mp4","fr-098.mp4","charge.mp4", "starstruck.mp4"]
    movie_configs = []

    for i, slot_idx in enumerate(movie_slots):
      movie_file = movies_to_use[i % len(movies_to_use)]
      start_delay = i * 4  # Stagger start times
      movie_configs.append((slot_idx, movie_file, start_delay))

    for slot_idx, movie_file, start_delay in movie_configs:
      movie = lev2.MoviePlaybackContext()
      movie_path = str(path.stage() / "assetcache" / "movies" / movie_file)
      movie.init(movie_path)
      provider = movie.createImageProvider()
      self.imageviews[slot_idx].image = provider

      self.movies.append(movie)
      self.movie_start_times.append(start_delay)
      self.movie_started.append(False)

      self.movie_channel.log(f"[Movie {len(self.movies)-1}] Slot {slot_idx:2d}: {movie_file:15s} (start @ {start_delay:.1f}s)")

    ############################################
    # Setup Matplotlib FM Synthesis Plots
    ############################################

    colors = ['cyan', 'yellow', 'magenta', 'orange', 'green', 'red', 'lime', 'white', 'pink', 'purple', 'gold', 'coral', 'navy', 'teal', 'olive', 'maroon']
    fm_configs = []

    for i in range(len(plot_slots)):
      # Generate varied FM parameters for each plot
      num_mods = 1 + (i % 4)  # 1-4 modulators
      carrier_freq = 0.8 + (i * 0.2) % 2.0
      carrier_phase = (i * 0.3) % 2.0

      modulators = []
      for m in range(num_mods):
        modulators.append({
          'freq': 2.0 + m * 1.5 + (i * 0.5) % 3.0,
          'amplitude': 0.7 - m * 0.15,
          'phase': m * 0.5 + (i * 0.2) % 2.0,
          'offset': m * np.pi / num_mods
        })

      fm_configs.append({
        'title': f'FM: {num_mods}-Mod #{i}',
        'carrier_freq': carrier_freq,
        'carrier_phase': carrier_phase,
        'modulators': modulators,
        'ylim': (-3, 3),
        'color': colors[i % len(colors)],
      })

    for plot_idx, (slot_idx, fm_config) in enumerate(zip(plot_slots, fm_configs)):
      # Get the widget for this slot
      widget = self.imageviews[slot_idx]

      # Create matplotlib plot
      fig, canvas, ax, params = self.createMatplotlibPlot(plot_idx, fm_config)

      # Start rendering thread
      thread = threading.Thread(
        target=self.renderMatplotlibPlot,
        args=(plot_idx, fig, canvas, ax, params, widget)
      )
      thread.daemon = True
      thread.start()
      self.mpl_threads.append(thread)

      # Create image provider
      provider = lev2.ImageProvider.createFromLambda(
        lambda idx=plot_idx: self.imageProviderMatPlotLib(idx)
      )
      widget.setImageProvider(provider)

      self.plot_channel.log(f"[Plot  {plot_idx}] Slot {slot_idx:2d}: {fm_config['title']}")

    self.stress_channel.log("=" * 80)
    self.stress_channel.log(f"Total: {len(self.movies)} movies + {len(self.mpl_figures)} plots = {len(self.movies) + len(self.mpl_figures)} image sources")
    self.stress_channel.log("=" * 80)

  ##############################################

  def _onUpdate(self, updinfo):
    """Update logic - handle delayed movie starts"""
    # Handle delayed movie starts
    abstime = updinfo.absolutetime
    for i, (movie, start_time, started) in enumerate(zip(
        self.movies, self.movie_start_times, self.movie_started)):
      if not started and abstime >= start_time:
        movie.play()
        self.movie_started[i] = True
        self.movie_channel.log(f"[T={abstime:.2f}s] Started movie {i} (delay={start_time:.1f}s)")

###############################################################################

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='ImageView Stress Test - Variable grid with mixed movie players and matplotlib plots')
  parser.add_argument('-g', '--griddim', type=int, default=4, help='Grid dimension (NxN grid of ImageViews, default=4)')
  args = parser.parse_args()

  # Validate grid dimension
  if args.griddim < 2:
    print(f"Error: Grid dimension must be at least 2 (got {args.griddim})")
    sys.exit(1)
  if args.griddim > 8:
    print(f"Warning: Grid dimension {args.griddim} is very large, may impact performance")

  # Create and run the ComponentizedApplication
  app = ImageViewStressTest(griddim=args.griddim)
  app.ezapp.mainThreadLoop()
