#!/usr/bin/env ork.python

################################################################################
# Simple waveform player test
# Copyright 1996-2024, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import numpy as np
import sys, math, wave
from orkengine.core import *
from orkengine.lev2 import *
from orkengine.lev2 import singularity as S
from ork.singularity import simplewav

################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
from singularity._harness import SingulTestApp, find_index
tokens = CrcStringProxy()
################################################################################

def load_wav_as_numpy(filepath):
    """Load a WAV file and return as normalized numpy array (mono, float32) with sample rate"""
    with wave.open(str(filepath), 'rb') as wav:
        # Get audio parameters
        n_channels = wav.getnchannels()
        sampwidth = wav.getsampwidth()
        n_frames = wav.getnframes()
        sample_rate = wav.getframerate()

        # Read all frames
        frames = wav.readframes(n_frames)

        # Convert to numpy array based on sample width
        if sampwidth == 1:  # 8-bit
            data = np.frombuffer(frames, dtype=np.uint8)
            data = (data - 128) / 128.0  # Convert to float -1 to 1
        elif sampwidth == 2:  # 16-bit
            data = np.frombuffer(frames, dtype=np.int16)
            data = data / 32768.0  # Convert to float -1 to 1
        elif sampwidth == 3:  # 24-bit
            # Convert 24-bit to 32-bit
            data = np.frombuffer(frames, dtype=np.uint8)
            data = data.reshape(-1, 3)
            # Pad to 32-bit
            data32 = np.zeros((len(data), 4), dtype=np.uint8)
            data32[:, :3] = data
            data = data32.flatten().view(np.int32)
            data = data / (2**23)
        else:
            raise ValueError(f"Unsupported sample width: {sampwidth}")

        # If stereo, convert to mono by averaging channels
        if n_channels == 2:
            data = data.reshape(-1, 2).mean(axis=1)
        elif n_channels > 2:
            data = data.reshape(-1, n_channels).mean(axis=1)

        return data.astype(np.float32), sample_rate

################################################################################

def create_sine_wave(frequency=440.0, duration=1.0, amplitude=0.3):
    """Create a sine wave"""
    sample_rate = 48000
    num_samples = int(sample_rate * duration)
    t = np.linspace(0, duration, num_samples, endpoint=False)
    waveform = amplitude * np.sin(2.0 * np.pi * frequency * t)
    return waveform

def create_noise_burst(duration=0.2, amplitude=0.1):
    """Create a noise burst"""
    sample_rate = 48000
    num_samples = int(sample_rate * duration)
    waveform = amplitude * np.random.randn(num_samples)
    # Apply envelope to avoid clicks
    fade_samples = int(sample_rate * 0.01)
    fade_in = np.linspace(0, 1, fade_samples)
    fade_out = np.linspace(1, 0, fade_samples)
    waveform[:fade_samples] *= fade_in
    waveform[-fade_samples:] *= fade_out
    return waveform


################################################################################

class SimpleWaveTest(SingulTestApp):

    def __init__(self):
        super().__init__()
        self.players = {}

    ##############################################

    def onGpuInit(self, ctx):
        super().onGpuInit(ctx)

        self.mainbus.gain = 0

        ############################
        # Create test waveforms and players
        ############################

        print("Creating waveform players...")

        # Get wav directory path
        wav_dir = S.baseDataPath() / "wavs"

        # Load real 808-style drum samples
        print(f"Loading samples from {wav_dir}")

        # Load real drum samples with their original sample rates
        kick_wave, kick_sr = load_wav_as_numpy(wav_dir / "bdrum_f_2.wav")
        snare_wave, snare_sr = load_wav_as_numpy(wav_dir / "snare_f3.wav")

        # Trim to 0.25 seconds based on original sample rate
        kick_wave = kick_wave[:int(kick_sr * 0.35)]
        snare_wave = snare_wave[:int(snare_sr * 0.35)]

        # Create drum players with real samples
        self.players['kick'] = simplewav.SimpleWavePlayer(
            self.synth,
            kick_wave,
            gain_db=-6,
            name="Kick808",
            sample_rate=kick_sr
        )

        self.players['snare'] = simplewav.SimpleWavePlayer(
            self.synth,
            snare_wave,
            gain_db=-3,
            name="Snare808",
            sample_rate=snare_sr
        )

        self.players['sine_low'] = simplewav.SimpleWavePlayer(
            self.synth,
            create_sine_wave(220.0, 0.5),
            gain_db=-12,
            name="Sine220"
        )

        self.players['sine_mid'] = simplewav.SimpleWavePlayer(
            self.synth,
            create_sine_wave(440.0, 0.5),
            gain_db=-12,
            name="Sine440"
        )

        self.players['sine_hi'] = simplewav.SimpleWavePlayer(
            self.synth,
            create_sine_wave(880.0, 0.5),
            gain_db=-12,
            name="Sine880"
        )

        self.players['noise'] = simplewav.SimpleWavePlayer(
            self.synth,
            create_noise_burst(),
            gain_db=-6,
            name="Noise"
        )

        print("Players created!")
        print("")
        print("Keyboard controls:")
        print("  Z - Kick drum")
        print("  X - Snare drum")
        print("  A - Sine 220Hz")
        print("  S - Sine 440Hz")
        print("  D - Sine 880Hz")
        print("  N - Noise burst")
        print("  Q - Stop all")
        print("  [ - Decrease kick gain")
        print("  ] - Increase kick gain")
        print("")

    ##############################################

    def _onKeyEvent(self, widget, uievent):
        """Custom keyboard handler using SimpleWavePlayer.play() interface"""
        res = ui.HandlerResult()

        if uievent.code == tokens.KEY_DOWN.hashed:
            KC = uievent.keycode

            # Drum triggers
            if KC == ord('A'):
                self.players['kick'].play()
                print("▶ Kick")
            elif KC == ord('W'):
                self.players['snare'].play()
                print("▶ Snare")

            # Sine wave triggers
            elif KC == ord('S'):
                self.players['sine_low'].play()
                print("▶ Sine 220Hz")
            elif KC == ord('E'):
                self.players['sine_mid'].play()
                print("▶ Sine 440Hz")
            elif KC == ord('D'):
                self.players['sine_hi'].play()
                print("▶ Sine 880Hz")

            # Noise trigger
            elif KC == ord('F'):
                self.players['noise'].play()
                print("▶ Noise")
            if KC == ord("["): # decr gain
              if uievent.shift:
                self.gain -= 6.0
                self.synth.masterGain = singularity.decibelsToLinear(self.gain)
              else:
                self.synth.programbus.gain = self.synth.programbus.gain - 3.0
              return res
            if KC == ord("]"): # incr gain
              if uievent.shift:
                self.gain += 6.0
                self.synth.masterGain = singularity.decibelsToLinear(self.gain)
              else:
                self.synth.programbus.gain = self.synth.programbus.gain + 3.0
              return res
            elif KC == ord("-"): # next effect
              if uievent.shift:
                self.synth.system_tempo -= 1
                self.curseq.timebase.tempo = self.synth.system_tempo
              else:
                self.synth.prevEffect(self.synth.programbus)
              return res
            elif KC == ord("="): # next effect
              if uievent.shift:
                self.synth.system_tempo += 1
                self.curseq.timebase.tempo = self.synth.system_tempo
              else:
                self.synth.nextEffect(self.synth.programbus)
              return res

        return res

    ##############################################

###############################################################################

SimpleWaveTest().ezapp.mainThreadLoop()
