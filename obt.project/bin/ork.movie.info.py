#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

import argparse
import sys
from orkengine import core
from orkengine.lev2 import MoviePlaybackContext

def format_bitrate(bitrate):
    """Format bitrate in human-readable form"""
    if bitrate == 0:
        return "N/A"
    if bitrate < 1000:
        return f"{bitrate} bps"
    elif bitrate < 1000000:
        return f"{bitrate/1000:.1f} kbps"
    else:
        return f"{bitrate/1000000:.2f} Mbps"

def format_duration(seconds):
    """Format duration in HH:MM:SS.mmm"""
    if seconds <= 0:
        return "N/A"
    hours = int(seconds // 3600)
    minutes = int((seconds % 3600) // 60)
    secs = seconds % 60
    if hours > 0:
        return f"{hours:02d}:{minutes:02d}:{secs:06.3f}"
    else:
        return f"{minutes:02d}:{secs:06.3f}"

def print_movie_info(filename):
    """Print comprehensive movie information"""

    # Create playback context and initialize
    ctx = MoviePlaybackContext()

    try:
        ctx.init(filename)
    except Exception as e:
        print(f"ERROR: Failed to load movie file: {e}", file=sys.stderr)
        return 1

    # Print header
    print("=" * 70)
    print(f"Movie Information: {ctx.filename}")
    print("=" * 70)

    # Container/Format information
    print("\n[Container Format]")
    print(f"  Format:           {ctx.format_long_name or ctx.format_name or 'Unknown'}")
    print(f"  Duration:         {format_duration(ctx.duration)}")
    print(f"  Total Bit Rate:   {format_bitrate(ctx.bit_rate)}")
    print(f"  Number of Streams: {ctx.nb_streams}")

    # Video stream information
    print("\n[Video Stream]")
    print(f"  Stream Index:     {ctx.video_stream_index}")
    print(f"  Codec:            {ctx.video_codec_name or 'Unknown'}")
    print(f"  Resolution:       {ctx.width}x{ctx.height}")
    print(f"  Frame Rate:       {ctx.fps:.2f} fps")
    print(f"  Frame Duration:   {ctx.frame_duration*1000:.3f} ms")
    print(f"  Bit Rate:         {format_bitrate(ctx.video_bit_rate)}")

    # Audio stream information
    print("\n[Audio Stream]")
    if ctx.has_audio:
        print(f"  Stream Index:     {ctx.audio_stream_index}")
        print(f"  Codec:            {ctx.audio_codec_name}")
        print(f"  Sample Rate:      {ctx.audio_sample_rate} Hz")
        print(f"  Channels:         {ctx.audio_channels}")
    else:
        print(f"  No audio stream detected")

    # Playback state information
    print("\n[Playback State]")
    print(f"  Current State:    {ctx.state}")
    print(f"  Current Frame:    {ctx.current_frame_index}")
    print(f"  Max Queue Size:   {ctx.max_queue_size} frames")

    print("=" * 70)

    return 0

def main():
    parser = argparse.ArgumentParser(
        description='Display comprehensive information about a movie file',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  ork.movie.info.py -i myvideo.mp4
  ork.movie.info.py --input ~/Movies/sample.mkv
        """)

    parser.add_argument('-i', '--input',
                        required=True,
                        help='Input movie filename')

    args = parser.parse_args()

    return print_movie_info(args.input)

if __name__ == '__main__':
    sys.exit(main())
