#!/usr/bin/env ork.python

################################################################################
# Hardware Video Decode Test
# Minimal example of VideoToolbox → IOSurface → Vulkan GPU-direct playback
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import sys, time, argparse
from pathlib import Path
from obt import path as obt_path
from orkengine.core import vec4
from orkengine import lev2
from ork.app import application, loggerui

################################################################################
# Build movie shortname map from filesystem
################################################################################

def build_movie_shortname_map():
  """Scan assetcache/movies directory and build shortname -> path map"""
  shortname_to_path = {}

  movies_dir = obt_path.stage() / "assetcache" / "movies"
  if not movies_dir.exists():
    return shortname_to_path

  # Scan for video files
  video_extensions = [".mp4", ".mov", ".mkv", ".avi", ".webm", ".m4v"]
  for ext in video_extensions:
    for video_file in movies_dir.glob(f"*{ext}"):
      shortname = video_file.stem  # filename without extension
      shortname_to_path[shortname] = video_file.name  # just the filename

  return shortname_to_path

################################################################################

class HardwareDecodeTest(application.ComponentizedApplication):

  def __init__(self, movie_file, use_videotoolbox=True, enable_audio=False, fullscreen=False, antialias=False):
    super().__init__()
    self.movie_file = movie_file
    self.use_videotoolbox = use_videotoolbox
    self.enable_audio = enable_audio
    self.antialias = antialias
    self.movie = None
    self.synth = None
    self.voice = None

    ############################################
    # Setup logger UI component
    ############################################

    self.addComponent("logger", loggerui.LoggerUIComponent,
                      filter_regex=[".*"],
                      background_color=vec4(0.2, 0.2, 0.2, 0.8))

    ############################################
    # Create EzApp
    ############################################

    self.createEzApp(
      name="HardwareVideoDecodeTest",
      width=1280,
      height=720,
      fullscreen=fullscreen,
      enable_audio=enable_audio,
      enable_audio_output=enable_audio,
      enable_audio_synth=enable_audio
    )
    movie_path = str(obt_path.stage() / "assetcache" / "movies" / self.movie_file)

    print("=" * 80)
    print(f"Hardware Video Decode Test")
    print(f"File: {self.movie_file}")
    print("=" * 80)

    self.movie = lev2.MoviePlaybackContext()
    self.movie.audio_timeshift = -2.45  # No audio delay
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
    else:
      # CPU Path: FFmpeg (for comparison)
      print("Backend: FFmpeg (CPU Decode)")
      print("Pipeline: FFmpeg → CPU buffer → GPU upload → Texture")

      self.movie.init(movie_path)
    time.sleep(2.0)

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
    self.imageview.fs_antialias = self.antialias
    if self.antialias:
      print("Antialiasing: Adaptive Lanczos enabled")

  ##############################################

  def _onGpuInit(self, ctx):
    """Initialize video playback"""

    if self.use_videotoolbox:
      # Direct texture assignment - movie updates texture internally
      self.imageview.texture = self.movie.texture
    else:
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
    LUI = self.findComponentByName("logger")
    channel = LUI._logger.getChannel("vtb.decode")
    channel.status_interval = 1.0
    channel.perf_interval = 0.1

###############################################################################

if __name__ == "__main__":
  # Build shortname map before parsing args
  shortname_map = build_movie_shortname_map()

  parser = argparse.ArgumentParser(description='Hardware video decode test')
  parser.add_argument('movie', nargs='?', default='bunny',
                      help='Movie file or shortname to play (default: bunny)')
  parser.add_argument('--cpu', action='store_true',
                      help='Use CPU FFmpeg backend instead of VideoToolbox')
  parser.add_argument('-a', '--audio', action='store_true',
                      help='Enable audio playback')
  parser.add_argument('-f', '--fullscreen', action='store_true',
                      help='Run in fullscreen mode')
  parser.add_argument('-A', '--aa', action='store_true',
                      help='Enable adaptive Lanczos antialiasing')
  parser.add_argument('-l', '--list', action='store_true',
                      help='List available movie shortnames')
  args = parser.parse_args()

  # Handle --list option
  if args.list:
    print("\nAvailable movies:")
    print("=" * 60)

    if not shortname_map:
      print("  (no movies found in assetcache/movies)")
    else:
      # Sort by shortname and display in columns
      sorted_names = sorted(shortname_map.keys())
      col_width = max(len(n) for n in sorted_names) + 2
      cols = max(1, 60 // col_width)

      for i in range(0, len(sorted_names), cols):
        row = sorted_names[i:i+cols]
        line = "  " + "".join(f"{n:<{col_width}}" for n in row)
        print(line)

    print("=" * 60)
    print(f"Total: {len(shortname_map)} movies")
    print("Usage: hwdec.py <shortname> [-a] [-f] [-A]")
    sys.exit(0)

  # Resolve shortname to filename
  movie_file = args.movie
  if movie_file in shortname_map:
    movie_file = shortname_map[movie_file]
    print(f"Resolved shortname '{args.movie}' -> {movie_file}")
  elif not any(movie_file.endswith(ext) for ext in [".mp4", ".mov", ".mkv", ".avi", ".webm", ".m4v"]):
    # Try adding .mp4 extension
    if args.movie + ".mp4" in [shortname_map.get(k, "") for k in shortname_map]:
      movie_file = args.movie + ".mp4"

  use_videotoolbox = not args.cpu

  app = HardwareDecodeTest(movie_file, use_videotoolbox, enable_audio=args.audio, fullscreen=args.fullscreen, antialias=args.aa)
  app.ezapp.mainThreadLoop()
