#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Tests for ComponentizedApplication with HFSM Subsystem integration + Audio.

This test verifies that ComponentizedApplication correctly works with
the C++ HFSM subsystem infrastructure and audio playback.

Plays 4 bars of "Axel F" (Beverly Hills Cop theme) using the synth,
displays notes in a TextBox, then exits.

Usage:
  ./test_componentized_subsystems2.py
"""

import sys
import time
import math
import numpy as np
from orkengine.core import *
from orkengine.lev2 import *
from orkengine.lev2 import singularity as S
from ork.app.application import ComponentizedApplication, ApplicationComponent

tokens = CrcStringProxy()

################################################################
# Axel F melody - 4 bars at 120 BPM
# Each beat = 0.5 seconds, each bar = 2 seconds
# Notes: (midi_note, start_beat, duration_beats)
################################################################

# MIDI note numbers
F4, Gs4, Ab4, Bb4, Eb4, C4, C5, Cs5, Fs4 = 65, 68, 68, 70, 63, 60, 72, 73, 66
REST = -1

# Axel F main riff (simplified, 4 bars)
# Bar 1-2: The iconic opening riff
# Bar 3-4: Repeat with variation
AXEL_F_MELODY = [
    # Bar 1
    (F4,  0.0,  0.25),
    (Ab4, 0.5,  0.25),
    (F4,  0.75, 0.125),
    (F4,  1.0,  0.25),
    (Bb4, 1.25, 0.25),
    (F4,  1.5,  0.25),
    (Eb4, 1.75, 0.25),
    # Bar 2
    (F4,  2.0,  0.25),
    (C5,  2.5,  0.25),
    (F4,  2.75, 0.125),
    (F4,  3.0,  0.25),
    (Cs5, 3.25, 0.25),
    (C5,  3.5,  0.25),
    (Ab4, 3.75, 0.25),
    # Bar 3 (repeat of bar 1)
    (F4,  4.0,  0.25),
    (Ab4, 4.5,  0.25),
    (F4,  4.75, 0.125),
    (F4,  5.0,  0.25),
    (Bb4, 5.25, 0.25),
    (F4,  5.5,  0.25),
    (Eb4, 5.75, 0.25),
    # Bar 4 (ending)
    (F4,  6.0,  0.25),
    (C5,  6.5,  0.25),
    (F4,  6.75, 0.125),
    (F4,  7.0,  0.25),
    (Ab4, 7.25, 0.5),
    (F4,  7.75, 0.25),
]

# Note names for display
NOTE_NAMES = {
    60: "C4", 63: "Eb4", 65: "F4", 66: "F#4", 68: "G#4",
    70: "Bb4", 72: "C5", 73: "C#5"
}

################################################################
# Simple synth patch creator (sine wave based)
################################################################

def create_simple_patch():
    """Create a simple synth patch for playing notes."""
    soundbank = singularity.BankData()
    patch = soundbank.newProgram("axelf_synth")

    # Create a layer with proper DSP stages
    newlyr = patch.newLayer()

    # Set up DSP and AMP stages (required for sound output!)
    dspstg = newlyr.appendStage("DSP")
    ampstg = newlyr.appendStage("AMP")
    dspstg.ioconfig.inputs = [0, 1]
    dspstg.ioconfig.outputs = [0, 1]
    ampstg.ioconfig.inputs = [0, 1]
    ampstg.ioconfig.outputs = [0, 1]

    # Pitch block
    pchblock = dspstg.appendDspBlock("Pitch", "pitch")
    newlyr.pitchBlock = pchblock
    newlyr.panmode = 2
    newlyr.pan = 7

    # Amplitude envelope
    ampenv = newlyr.appendController("RateLevelEnv", "AMPENV")
    ampenv.ampenv = True
    ampenv.bipolar = False
    ampenv.sustainSegment = 0
    ampenv.addSegment("seg0", 0, 1, 0.01)   # Fast attack
    ampenv.addSegment("seg1", 1, 0.7, 0.1)  # Quick decay to sustain
    ampenv.addSegment("seg2", 1, 0, 0.3)    # Release

    # Amp block with envelope
    ampblock = ampstg.appendDspBlock("AmpAdaptive", "amp")
    ampblock.paramByName("gain").mods.src1 = ampenv
    ampblock.paramByName("gain").mods.src1scale = 1.0

    # Sampler oscillator block
    SOSCIL = dspstg.appendDspBlock("Sampler", "soscil")

    # Create waveform data - simple sine with harmonics
    wavelength = 512
    samplerate = 16000
    root_key = 36
    orig_pitch = samplerate / wavelength
    highestPitch = orig_pitch * 48000.0 / samplerate
    highestPitchN = singularity.frequencyToMidiNote(highestPitch)
    highestPitchCents = int(highestPitchN * 100.0) + 1

    # Create waveform with harmonics (synth brass-like)
    final_waveform = np.zeros(256 * wavelength)
    for i in range(256):
        fi = i / 256.0
        fs = 0.5 + math.sin(fi * 2.0 * 3.14159) + 0.5
        t = np.linspace(0, 1, wavelength, endpoint=False)
        waveform = np.zeros(wavelength)
        for j in range(30):
            fj = j / 30.0
            fn = fs * fj + (1.0 - fj)
            waveform = waveform + (np.sin(2 * np.pi * (j + 1) * t) / (j + 1)) * fn
        waveform = waveform / np.max(np.abs(waveform)) * 0.1
        final_waveform[i * wavelength:(i + 1) * wavelength] = waveform

    the_sample = S.SampleData(
        name="AxelSample",
        format=tokens.F32_NPARRAY,
        waveform=final_waveform,
        rootKey=root_key,
        pitchAdjustCents=0.0,
        sampleRate=samplerate,
        highestPitchCents=highestPitchCents,
        loopPoint=len(final_waveform) - 1,
    )
    multisample = S.MultiSampleData("MSAMPLE", [the_sample])
    keymap = S.KeyMapData("KMAP")
    keymap.addRegion(
        lokey=0, hikey=127,
        lovel=0, hivel=127,
        multisample=multisample,
        sample=the_sample
    )
    newlyr.keymap = keymap

    return soundbank, patch

################################################################
# Test Application
################################################################

class AxelFApp(ComponentizedApplication):
    """Test app that plays Axel F melody using ComponentizedApplication + subsystems."""

    def __init__(self):
        super().__init__()
        self.frame_count = 0
        self.start_time = None
        self.melody = AXEL_F_MELODY
        self.tempo = 120.0  # BPM
        self.beat_duration = 60.0 / self.tempo  # seconds per beat
        self.total_beats = 8.0  # 4 bars at 4/4
        self.total_duration = self.total_beats * self.beat_duration

        # Note tracking
        self.active_voices = {}  # note -> (voice, end_time)
        self.note_index = 0
        self.current_note_name = ""
        self.synth = None
        self.patch = None
        self.soundbank = None

        self.ezapp_args = {
            'width': 640,
            'height': 480,
            'offscreen': False,
            # New list-based subsystem selection API
            # audioO = audio output + synth (implicit)
            'use_subsystems': ['opq', 'core', 'gpu', 'audioO', 'lev2'],
        }

    ##############################################

    def _onUiInit(self):
        """Set up minimal UI with a TextBox."""
        lg_group = self.ezapp.topLayoutGroup
        self.text_box_item = lg_group.makeChild(
            uiclass=ui.TextBox,
            args=["axelf", vec4(0.1, 0.1, 0.3, 1), "Axel F"]
        )
        self.text_box_item.layout.fill(lg_group.layout)
        self.text_box = self.text_box_item.widget
        self.text_box.setText("Axel F - Beverly Hills Cop Theme\n\nInitializing...")

    ##############################################

    def _onSynthInit(self, synth):
        """Initialize the synth and create our patch."""
        self.synth = synth
        self.synth.masterGain = singularity.decibelsToLinear(-6.0)

        # Create our synth patch
        self.soundbank, self.patch = create_simple_patch()

        # Set up the main bus
        self.mainbus = self.synth.outputBus("main")
        self.mainbus.gain = singularity.decibelsToLinear(18.0)
        self.synth.setEffect(self.mainbus, "none")
        self.synth.programbus.uiprogram = self.patch

        self.text_box.setText("Axel F - Beverly Hills Cop Theme\n\nSynth ready! Starting playback...")

    ##############################################

    def _onUpdateInit(self):
        """Initialize the start time."""
        self.start_time = time.time()

    ##############################################

    def _onUpdate(self, updinfo):
        """Update melody playback."""
        self.frame_count += 1
        if self.start_time is None or self.synth is None or self.patch is None:
            return

        elapsed = time.time() - self.start_time
        current_beat = elapsed / self.beat_duration

        # Check for notes that need to end
        notes_to_remove = []
        for note, (voice, end_time) in self.active_voices.items():
            if elapsed >= end_time:
                self.synth.keyOff(voice, note, 0)
                notes_to_remove.append(note)
        for note in notes_to_remove:
            del self.active_voices[note]

        # Check for notes that need to start
        while self.note_index < len(self.melody):
            note, start_beat, duration_beats = self.melody[self.note_index]
            if current_beat >= start_beat:
                # Start this note
                if note != REST and note not in self.active_voices:
                    voice = self.synth.keyOn(note, 100, self.patch, None)
                    end_time = (start_beat + duration_beats) * self.beat_duration
                    self.active_voices[note] = (voice, end_time)
                    self.current_note_name = NOTE_NAMES.get(note, f"N{note}")
                    print(f"  Playing: {self.current_note_name}")
                self.note_index += 1
            else:
                break

        # Update display
        bar = int(current_beat / 4) + 1
        beat_in_bar = (current_beat % 4) + 1
        active_notes = ", ".join(NOTE_NAMES.get(n, f"N{n}") for n in self.active_voices.keys())
        if not active_notes:
            active_notes = "---"

        display_text = "Axel F - Beverly Hills Cop Theme\n"
        display_text += "=" * 35 + "\n\n"
        display_text += f"Bar: {bar}/4  Beat: {beat_in_bar:.1f}\n\n"
        display_text += f"Playing: {active_notes}\n\n"
        display_text += f"Time: {elapsed:.1f}s / {self.total_duration:.1f}s"
        self.text_box.setText(display_text)

        # Exit after melody completes (plus a little extra for the last note)
        if elapsed >= self.total_duration + 0.5:
            # Release any remaining notes
            for note, (voice, _) in self.active_voices.items():
                self.synth.keyOff(voice, note, 0)
            self.active_voices.clear()

            print(f"  Melody complete after {self.frame_count} frames")
            self.text_box.setText("Axel F - Complete!\n\nExiting...")
            self.ezapp.signalExit()

################################################################

def main():
    print("Testing ComponentizedApplication with Audio Playback")
    print("=" * 60)
    print("Playing: Axel F (Beverly Hills Cop Theme) - 4 bars")
    print()

    # Create app and ezapp
    app = AxelFApp()
    ezapp = app.createEzApp()

    print("[Subsystem Status]")
    gpu = ezapp.getSubsystem("gpu")
    audio = ezapp.getSubsystem("audio")
    lev2 = ezapp.getSubsystem("lev2")
    print(f"  GPU:   {'READY' if gpu and gpu.currentState() == gpu.state_ready else 'NOT READY'}")
    print(f"  Audio: {'READY' if audio and audio.currentState() == audio.state_ready else 'NOT READY'}")
    print(f"  Lev2:  {'READY' if lev2 and lev2.currentState() == lev2.state_ready else 'NOT READY'}")

    print("\n[Running main loop - Axel F playback]")
    ezapp.mainThreadLoop()

    print("\n[Shutdown]")
    ezapp.shutdown()

    print("Done!")
    return 0

if __name__ == '__main__':
    sys.exit(main())
