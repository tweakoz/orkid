#!/usr/bin/env ork.python

################################################################################
# ImageView GPU-Direct Video Test
# Demonstrates VideoToolbox → IOSurface → Vulkan zero-copy video playback
# Uses TextureProvider for direct GPU texture sampling (no CPU transfer)
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

tokens = CrcStringProxy()

################################################################################

class VideoToolboxStressTest(application.ComponentizedApplication):

  def __init__(self, griddim=3, use_videotoolbox=True):
    super().__init__()

    # Grid configuration
    self.griddim = griddim
    self.total_cells = griddim * griddim
    self.use_videotoolbox = use_videotoolbox

    # Movie state tracking
    self.movies = []
    self.movie_start_times = []
    self.movie_started = []

    ############################################
    # Setup logger UI component
    ############################################

    self.addComponent("logger", loggerui.LoggerUIComponent,
                      filter_regex=[".*"],
                      background_color=vec4(0.2, 0.2, 0.2, 0.8))

    ############################################
    # Create EzApp and initialize
    ############################################

    self.createEzApp(name="VideoToolboxGpuDirectTest",
                     width=1600,
                     height=900,
                     fullscreen=False,
                     enable_audio=False,
                     enable_audio_output=False,
                     enable_audio_synth=False,
                     enable_freerun_ups=True,
                     enable_freerun_fps=True)

    self.ezapp.uicontext.debug_event_routing = False

  ##############################################

  def _onUiInit(self):
    """Initialize UI layout and widgets"""
    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorGuide = vec4(0, 0.5, 1, 1)  # Blue

    ############################################
    # Create NxN grid of ImageViews
    ############################################

    self.griditems = lg_group.makeGrid(
      width=self.griddim,
      height=self.griddim,
      margin=4,
      uiclass=lev2.ui.Box,
      args=["placeholder", vec4(0.1, 0.1, 0.1, 1)],
    )
    lg_group.margin = 4

    ############################################
    # Replace each grid cell with an ImageView
    ############################################

    self.imageviews = []
    for i in range(self.total_cells):
      imv = lg_group.makeChild(uiclass=lev2.ui.ImageView, args=[f"imgview_{i}"])
      lg_group.replaceChild(self.griditems[i].layout, imv)
      imv_widget = imv.widget
      imv_widget.maintain_aspect_ratio = True
      imv_widget.generate_mipmaps = False  # Video frames don't need mipmaps
      self.imageviews.append(imv_widget)

  ##############################################

  def _onAppLink(self):
    """Configure logger channels after component initialization"""
    logger_comp = self.findComponentByName("logger")

    # Configure GPU channel for GPU-direct events
    self.gpu_channel = logger_comp.configureChannel(
        "GPU",
        vec3(0.0, 1.0, 0.5),  # Green
        enable_channel=True
    )

    # Configure MOVIE channel for movie playback events
    self.movie_channel = logger_comp.configureChannel(
        "MOVIE",
        vec3(0.3, 0.8, 1.0),  # Cyan
        enable_channel=True
    )

    # Configure VTB channel for VideoToolbox backend events
    self.vtb_channel = logger_comp.configureChannel(
        "VTB",
        vec3(1.0, 0.5, 0.0),  # Orange
        enable_channel=True
    )

  ##############################################

  def _onGpuInit(self, ctx):
    """Initialize GPU resources - GPU-direct video playback"""
    backend_name = "VideoToolbox (GPU-Direct)" if self.use_videotoolbox else "FFmpeg (CPU)"

    self.gpu_channel.log("=" * 80)
    self.gpu_channel.log(f"GPU-Direct Video Test - {self.griddim}x{self.griddim} Grid")
    self.gpu_channel.log(f"Backend: {backend_name}")
    self.gpu_channel.log(f"Total Video Players: {self.total_cells}")
    self.gpu_channel.log("=" * 80)

    ############################################
    # Setup Movie Players
    ############################################

    movies_to_use = ["bunny.mp4", "wipeout.mp4", "fr-098.mp4", "charge.mp4", "starstruck.mp4"]

    for i in range(self.total_cells):
      movie_file = movies_to_use[i % len(movies_to_use)]
      start_delay = i * 2  # Stagger start times

      movie = lev2.MoviePlaybackContext()
      movie_path = str(path.stage() / "assetcache" / "movies" / movie_file)

      # Initialize with backend selection
      if self.use_videotoolbox:
        # GPU-direct: VideoToolbox → IOSurface → Vulkan
        movie.init(
          movie_path,
          backend=lev2.MovieBackend.VIDEOTOOLBOX,
          format=lev2.MoviePixelFormat.AUTO  # BGRA for now
        )
        provider = movie.texture_provider  # Direct GPU texture
        self.vtb_channel.log(f"[VTB {i}] Slot {i:2d}: {movie_file:15s} → IOSurface → Vulkan")
      else:
        # CPU path: FFmpeg (for comparison)
        movie.init(movie_path)  # Defaults to FFMPEG backend
        provider = movie.image_provider  # CPU image upload
        self.movie_channel.log(f"[CPU {i}] Slot {i:2d}: {movie_file:15s} → CPU upload")

      self.imageviews[i].image = provider

      self.movies.append(movie)
      self.movie_start_times.append(start_delay)
      self.movie_started.append(False)

    self.gpu_channel.log("=" * 80)
    if self.use_videotoolbox:
      self.gpu_channel.log("Zero-copy GPU pipeline:")
      self.gpu_channel.log("  VideoToolbox decode → IOSurface → Metal → VkImage → Shader")
      self.gpu_channel.log("  ✓ No CPU involvement")
      self.gpu_channel.log("  ✓ No memory copies")
      self.gpu_channel.log("  ✓ Hardware YCbCr conversion (when NV12 enabled)")
    else:
      self.gpu_channel.log("CPU pipeline:")
      self.gpu_channel.log("  FFmpeg decode → CPU buffer → GPU upload → Texture")
    self.gpu_channel.log("=" * 80)

  ##############################################

  def _onUpdate(self, updinfo):
    """Update logic - handle delayed movie starts"""
    abstime = updinfo.absolutetime
    for i, (movie, start_time, started) in enumerate(zip(
        self.movies, self.movie_start_times, self.movie_started)):
      if not started and abstime >= start_time:
        movie.play()
        self.movie_started[i] = True
        backend = "VTB" if self.use_videotoolbox else "CPU"
        self.movie_channel.log(f"[T={abstime:.2f}s] Started movie {i} [{backend}]")

###############################################################################

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='GPU-Direct VideoToolbox Test')
  parser.add_argument('-g', '--griddim', type=int, default=4,
                      help='Grid dimension (NxN grid, default=4 for 16 decoders)')
  parser.add_argument('--cpu', action='store_true',
                      help='Use CPU FFmpeg backend instead of GPU VideoToolbox')
  args = parser.parse_args()

  # Validate grid dimension
  if args.griddim < 1:
    print(f"Error: Grid dimension must be at least 1 (got {args.griddim})")
    sys.exit(1)

  use_videotoolbox = not args.cpu

  # Create and run the ComponentizedApplication
  app = VideoToolboxStressTest(griddim=args.griddim, use_videotoolbox=use_videotoolbox)
  app.ezapp.mainThreadLoop()
