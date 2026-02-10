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
from ork.app import application
from ork.app.frame_profiler import FrameProfilerComponent

tokens = CrcStringProxy()

################################################################################

class VideoToolboxStressTest(application.ComponentizedApplication):

  def __init__(self, griddim=3, use_videotoolbox=True, enable_audio=False, fullscreen=False, antialias=False):
    super().__init__()

    # Grid configuration
    self.griddim = griddim
    self.total_cells = griddim * griddim
    self.use_videotoolbox = use_videotoolbox
    self.enable_audio = enable_audio
    self.fullscreen = fullscreen
    self.antialias = antialias

    # Movie state tracking
    self.movies = []
    self.movie_start_times = []
    self.movie_started = []

    # Audio state tracking (one voice per movie)
    self.synth = None
    self.audio_programs = []
    self.voices = []
    self.movie_gains = []

    ############################################
    # Setup profiler UI component
    ############################################

    self.profiler = self.addComponent("profiler", FrameProfilerComponent)

    ############################################
    # Create EzApp and initialize
    ############################################

    self.createEzApp(name="VideoToolboxGpuDirectTest",
                     width=1600,
                     height=900,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'],
                     fullscreen=fullscreen,
                     enable_audio=enable_audio,
                     enable_audio_output=enable_audio,
                     enable_audio_synth=enable_audio,
                     enable_freerun_ups=True,
                     enable_freerun_fps=True)

    self.ezapp.uicontext.debug_event_routing = False

  ##############################################

  def _onSynthInit(self, synth):
    """Store synth reference for audio playback"""
    self.synth = synth

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
      imv_widget.fs_antialias = self.antialias
      self.imageviews.append(imv_widget)

  ##############################################

  def _onGpuInit(self, ctx):
    """Initialize GPU resources - GPU-direct video playback"""
    backend_name = "VideoToolbox (GPU-Direct)" if self.use_videotoolbox else "FFmpeg (CPU)"

    print("=" * 80)
    print(f"GPU-Direct Video Test - {self.griddim}x{self.griddim} Grid")
    print(f"Backend: {backend_name}")
    print(f"Total Video Players: {self.total_cells}")
    print("=" * 80)

    ############################################
    # Setup Movie Players
    ############################################

    movies_to_use = ["bunny.mp4", "wipeout.mp4", "fr-098.mp4", "charge.mp4", "starstruck.mp4"]

    # Per-movie gain table (dB) - adjust individual movie volumes here
    movie_gains = {
      "bunny.mp4": 0.0,
      "wipeout.mp4": -12.0,
      "fr-098.mp4": -12.0,
      "charge.mp4": -12.0,
      "starstruck.mp4": -12.0,
    }

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
        print(f"[VTB {i}] Slot {i:2d}: {movie_file:15s} → IOSurface → Vulkan")
      else:
        # CPU path: FFmpeg (for comparison)
        movie.init(movie_path)  # Defaults to FFMPEG backend
        provider = movie.image_provider  # CPU image upload
        print(f"[CPU {i}] Slot {i:2d}: {movie_file:15s} → CPU upload")

      self.imageviews[i].image = provider

      # Create audio program for this movie (if audio enabled and movie has audio)
      audio_program = None
      if self.enable_audio and self.synth and movie.has_audio:
        audio_program = movie.createAudioProgram(self.synth)
        print(f"[AUDIO {i}] Created audio program for {movie_file}")
        movie.audio_timeshift = -2.45  # No audio delay

      self.movies.append(movie)
      self.audio_programs.append(audio_program)
      self.voices.append(None)  # Voice created when movie starts
      self.movie_gains.append(movie_gains.get(movie_file, 0.0))
      self.movie_start_times.append(start_delay)
      self.movie_started.append(False)

    print("=" * 80)
    if self.use_videotoolbox:
      print("Zero-copy GPU pipeline:")
      print("  VideoToolbox decode → IOSurface → Metal → VkImage → Shader")
      print("  ✓ No CPU involvement")
      print("  ✓ No memory copies")
      print("  ✓ Hardware YCbCr conversion (when NV12 enabled)")
    else:
      print("CPU pipeline:")
      print("  FFmpeg decode → CPU buffer → GPU upload → Texture")
    print("=" * 80)

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
        print(f"[T={abstime:.2f}s] Started movie {i} [{backend}]")

        # Start audio voice for this movie
        if self.audio_programs[i] and self.synth:
          voice = self.synth.keyOn(i, 60, self.audio_programs[i], None)
          voice.gain = self.movie_gains[i]
          self.voices[i] = voice
          print(f"[T={abstime:.2f}s] Started audio voice {i} (gain={self.movie_gains[i]:.1f}dB)")

###############################################################################

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='GPU-Direct VideoToolbox Test')
  parser.add_argument('-g', '--griddim', type=int, default=4,
                      help='Grid dimension (NxN grid, default=4 for 16 decoders)')
  parser.add_argument('--cpu', action='store_true',
                      help='Use CPU FFmpeg backend instead of GPU VideoToolbox')
  parser.add_argument('-a', '--audio', action='store_true',
                      help='Enable audio playback (each movie gets its own voice)')
  parser.add_argument('-f', '--fullscreen', action='store_true',
                      help='Run in fullscreen mode')
  parser.add_argument('-A', '--aa', action='store_true',
                      help='Enable adaptive Lanczos antialiasing')
  args = parser.parse_args()

  # Validate grid dimension
  if args.griddim < 1:
    print(f"Error: Grid dimension must be at least 1 (got {args.griddim})")
    sys.exit(1)

  use_videotoolbox = not args.cpu

  # Create and run the ComponentizedApplication
  app = VideoToolboxStressTest(
    griddim=args.griddim,
    use_videotoolbox=use_videotoolbox,
    enable_audio=args.audio,
    fullscreen=args.fullscreen,
    antialias=args.aa
  )
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
