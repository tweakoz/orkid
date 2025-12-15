################################################################################
# Simple waveform player for non-audio programmers
# Copyright 1996-2024, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

from orkengine.core import *
from orkengine.lev2 import *
from orkengine.lev2 import singularity as S
from ork.singularity.sampler import createLayer, createSampleLayer

################################################################################

class SimpleWavePlayer:
    """
    Simple fire-and-forget waveform player for non-audio programmers.

    Usage:
        player = SimpleWavePlayer(synth, waveform, gain_db=-6, name="MySound")
        player.play()  # fire and forget - mono-trigger (stops previous)
        player.stop()  # explicit stop
        player.gain_db = -12  # adjust gain
    """

    def __init__(self, synth, waveform, gain_db=0.0, name="SimpleWave", sample_rate=48000):
        """
        Create a simple waveform player

        Args:
            synth: singularity synth instance
            waveform: numpy array of float samples
            gain_db: initial gain in decibels
            name: name for the sound
            sample_rate: sample rate of the waveform (default 48000)
        """
        self._synth = synth
        self._waveform = waveform
        self._gain_db = gain_db
        self._name = name
        self._sample_rate = sample_rate
        self._current_voice = None
        self._program = self._create_program()
        self._apply_gain()

    def _create_program(self):
        """Create program with sampler layer from waveform"""

        # Create bank and program
        bank = S.BankData()
        program = bank.newProgram(self._name)
        newlyr, SOSCIL, dspstg, ampstg = createLayer(program)

        # Create sample using the new loadFromFloatWaveformBuffer method
        # This ensures all internal fields are set properly, same as loadFromAudioFile
        sample = S.SampleData(name=self._name)

        # Calculate originalPitch for rootKey 60 (middle C)
        # A4 (MIDI 69) = 440Hz, so C4 (MIDI 60) = 440 * 2^((60-69)/12)
        root_freq = 440.0 * pow(2.0, (60 - 69) / 12.0)

        # Load from buffer - this sets all fields properly like loadFromAudioFile
        sample.loadFromFloatWaveformBuffer(
            self._waveform,
            self._sample_rate,
            1,  # num_channels (mono)
            root_freq,  # original_pitch
            True  # normalize
        )

        # Set additional properties
        sample.root_key = 60
        #sample._interpMethod = 2

        # Create multisample and keymap
        multisample = S.MultiSampleData(self._name + "_MS", [sample])
        keymap = S.KeyMapData(self._name + "_KM")
        keymap.addRegion(
            lokey=0,
            hikey=127,  # all keys trigger same sample
            lovel=0,
            hivel=127,
            multisample=multisample,
            sample=sample
        )

        newlyr.keymap = keymap

        # Store layer for gain control
        self._layer = newlyr

        return program

    def _apply_gain(self):
        """Apply gain to the layer"""
        if hasattr(self, '_layer'):
            self._layer.gain = self._gain_db

    def play(self):
        """
        Fire and forget playback.
        Stops previous instance if playing (mono-trigger behavior).
        """
        # Stop previous if exists
        if self._current_voice is not None:
            self._synth.keyOff(self._current_voice, 60, 0)
            self._current_voice = None

        # Start new (always at note 60, max velocity)
        self._current_voice = self._synth.keyOn(60, 127, self._program, None)

    def stop(self):
        """Explicitly stop current playback"""
        if self._current_voice is not None:
            self._synth.keyOff(self._current_voice, 60, 0)
            self._current_voice = None

    @property
    def is_playing(self):
        """Check if currently playing (best-effort)"""
        return self._current_voice is not None

    @property
    def gain_db(self):
        """Get current gain in dB"""
        return self._gain_db

    @gain_db.setter
    def gain_db(self, value):
        """Set gain in dB"""
        self._gain_db = value
        self._apply_gain()
