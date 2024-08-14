#!/usr/bin/env python3

# generate stereo wav file with impulse (to later generate impulse response)

import sys, os, argparse, subprocess
from scipy.io import wavfile
import numpy as np

parser = argparse.ArgumentParser(description="Generate stereo WAV file with impulse.")
parser.add_argument("output_file", help="Output WAV file.")
parser.add_argument("duration", type=float, help="Duration of impulse in seconds.")
parser.add_argument("sample_rate", type=int, help="Sample rate of output WAV file.")
args = parser.parse_args()

def generate_impulse_wav(output_file, duration, sample_rate):
  # Generate impulse
  impulse = np.zeros(int(duration * sample_rate))
  impulse[0] = 1.0

  # Save impulse to WAV file
  wavfile.write(output_file, sample_rate, impulse)

generate_impulse_wav(args.output_file, args.duration, args.sample_rate)

