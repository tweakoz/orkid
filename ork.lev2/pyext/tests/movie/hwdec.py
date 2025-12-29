#!/usr/bin/env ork.python

################################################################################
# Hardware Video Decode Test
# Minimal example of VideoToolbox → IOSurface → Vulkan GPU-direct playback
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import sys
from obt import path
from orkengine.core import vec4
from orkengine import lev2
from ork.app import application

################################################################################

class HardwareDecodeTest(application.ComponentizedApplication):

  def __init__(self, movie_file, use_videotoolbox=True, enable_audio=False):
    super().__init__()
    self.movie_file = movie_file
    self.use_videotoolbox = use_videotoolbox
    self.enable_audio = enable_audio
    self.movie = None
    self.synth = None
    self.voice = None

    # Create EzApp
    self.createEzApp(
      name="HardwareVideoDecodeTest",
      width=1280,
      height=720,
      fullscreen=False,
      enable_audio=enable_audio,
      enable_audio_output=enable_audio,
      enable_audio_synth=enable_audio
    )

  ##############################################

  def _onSynthInit(self, synth):
    """Store synth reference for audio playback"""
    self.synth = synth

  ##############################################

  def _onUiInit(self):
    """Initialize UI - single centered ImageView"""
    lg = self.ezapp.topLayoutGroup
    lg.clearColorGuide = vec4(0.1, 0.1, 0.15, 1)

    # Single ImageView filling viewport (using grid layout)
    self.griditems = lg.makeGrid(
      width=1,
      height=1,
      margin=4,
      uiclass=lev2.ui.ImageView,
      args=["video_view"],
    )
    self.imageview = self.griditems[0].widget

    self.imageview.maintain_aspect_ratio = True
    self.imageview.generate_mipmaps = False

  ##############################################

  def _onGpuInit(self, ctx):
    """Initialize video playback"""
    movie_path = str(path.stage() / "assetcache" / "movies" / self.movie_file)

    print("=" * 80)
    print(f"Hardware Video Decode Test")
    print(f"File: {self.movie_file}")
    print("=" * 80)

    self.movie = lev2.MoviePlaybackContext()

    if self.use_videotoolbox:
      # GPU-Direct: VideoToolbox → IOSurface → Vulkan
      print("Backend: VideoToolbox (Hardware Decode)")
      print("Pipeline: VideoToolbox → IOSurface → Metal → VkImage → Shader")
      print("  ✓ Zero-copy GPU-direct")
      print("  ✓ No CPU memory transfers")

      self.movie.init(
        movie_path,
        backend=lev2.MovieBackend.VIDEOTOOLBOX,
        format=lev2.MoviePixelFormat.AUTO  # BGRA (single-plane)
      )

      # Direct texture assignment - movie updates texture internally
      self.imageview.texture = self.movie.texture

    else:
      # CPU Path: FFmpeg (for comparison)
      print("Backend: FFmpeg (CPU Decode)")
      print("Pipeline: FFmpeg → CPU buffer → GPU upload → Texture")

      self.movie.init(movie_path)

      # Use image_provider for CPU upload
      self.imageview.image = self.movie.image_provider

    # Display movie info
    print("-" * 80)
    print(f"Resolution: {self.movie.width}x{self.movie.height}")
    print(f"FPS: {self.movie.fps:.2f}")
    print(f"Duration: {self.movie.duration:.2f}s")
    print(f"Format: {self.movie.format_name}")
    print(f"Video Codec: {self.movie.video_codec_name}")
    if self.movie.has_audio:
      print(f"Audio: {self.movie.audio_codec_name} @ {self.movie.audio_sample_rate}Hz")
    print("=" * 80)

    # Set up audio if enabled
    if self.enable_audio and self.synth and self.movie.has_audio:
      print("Setting up audio playback...")
      self.audio_program = self.movie.createAudioProgram(self.synth)
      self.voice = self.synth.keyOn(0, 60, self.audio_program, None)
      self.voice.gain = 0.0  # 0 dB
      print("Audio playback enabled")

    # Start playback
    self.movie.play()

###############################################################################

if __name__ == "__main__":
  import argparse

  parser = argparse.ArgumentParser(description='Hardware video decode test')
  parser.add_argument('movie', nargs='?', default='bunny.mp4',
                      help='Movie file to play (default: bunny.mp4)')
  parser.add_argument('--cpu', action='store_true',
                      help='Use CPU FFmpeg backend instead of VideoToolbox')
  parser.add_argument('-a', '--audio', action='store_true',
                      help='Enable audio playback')
  args = parser.parse_args()

  use_videotoolbox = not args.cpu

  app = HardwareDecodeTest(args.movie, use_videotoolbox, enable_audio=args.audio)
  app.ezapp.mainThreadLoop()
