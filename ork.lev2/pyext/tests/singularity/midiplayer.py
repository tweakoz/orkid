#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
MIDI Player with Piano Keyboard Visualization

Plays Moonlight Sonata from MIDI file using C++ sequencer,
displays a piano keyboard visualization showing active notes in red.

Usage:
  ./midiplayer.py
"""

import sys
import time
from orkengine.core import *
from orkengine.lev2 import *
from ork.app.application import ComponentizedApplication
from mido import MidiFile

# Import midiToSingularitySequence from local _seq module
sys.path.append((thisdir()).normalized.as_string)
from _seq import midiToSingularitySequence

tokens = CrcStringProxy()
timestamp = singularity.TimeStamp

TEMPO = 120

################################################################
# Piano Keyboard Layout Helper
################################################################

BLACK_KEY_OFFSETS = {1, 3, 6, 8, 10}  # C#, D#, F#, G#, A#

def isBlackKey(midi_note):
    return (midi_note % 12) in BLACK_KEY_OFFSETS

def whiteKeyIndex(midi_note):
    octave = midi_note // 12
    semitone = midi_note % 12
    white_in_octave = [0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6][semitone]
    return octave * 7 + white_in_octave

################################################################
# Piano Keyboard Widget
################################################################

class PianoKeyboard:
    """Piano keyboard visualization using PrimCanvas."""

    # 88-key piano: A0 (21) to C8 (108)
    FIRST_NOTE = 21  # A0
    LAST_NOTE = 108  # C8
    NUM_WHITE_KEYS = 52  # 88-key piano has 52 white keys

    def __init__(self, canvas):
        self.canvas = canvas
        self.key_quads = {}
        self.active_notes = {}  # note -> count
        self._gpu_initialized = False

        # Set up pre-render callback
        self.canvas.onPreRender = self._onPreRender

    def setActiveNotes(self, notes_dict):
        """Update the dict of currently active notes (note -> count)."""
        self.active_notes = dict(notes_dict)

    def _onPreRender(self):
        """Called before each render."""
        if not self._gpu_initialized:
            self._gpuInit()
        self._render()

    def _gpuInit(self):
        """Initialize GPU resources on first render."""
        # Create layers - white keys first (back), then black keys (front)
        self.white_layer = self.canvas.createLayer("white_keys")
        self.black_layer = self.canvas.createLayer("black_keys")

        # Create primitives
        pipeline = self.canvas.pipelineSolid
        self.white_prim = ui.QuadPrimitive(pipeline=pipeline)
        self.black_prim = ui.QuadPrimitive(pipeline=pipeline)

        # Create quads for each key
        for midi_note in range(self.FIRST_NOTE, self.LAST_NOTE + 1):
            qd = ui.QuadData()
            self.key_quads[midi_note] = qd

            if isBlackKey(midi_note):
                self.black_prim.addQuad(qd)
            else:
                self.white_prim.addQuad(qd)

        self.white_layer.addPrimitive(self.white_prim)
        self.black_layer.addPrimitive(self.black_prim)
        self._gpu_initialized = True

    def _render(self):
        """Update keyboard geometry and colors."""
        w, h = self.canvas.width, self.canvas.height
        if w < 1 or h < 1:
            return

        # Calculate keyboard dimensions
        white_key_w = w / self.NUM_WHITE_KEYS
        white_key_h = h
        black_key_w = white_key_w * 0.6
        black_key_h = h * 0.6

        first_wk_idx = whiteKeyIndex(self.FIRST_NOTE)

        for midi_note, qd in self.key_quads.items():
            wk_idx = whiteKeyIndex(midi_note)
            rel_idx = wk_idx - first_wk_idx

            # Get note count (0 if not active)
            note_count = self.active_notes.get(midi_note, 0)

            if isBlackKey(midi_note):
                # Black key - centered on the boundary between white keys
                # Bottom-aligned, shorter than white keys
                x = (rel_idx + 1) * white_key_w - black_key_w / 2
                y = h - black_key_h
                qd.setPosition(x, y)
                qd.setSize(black_key_w, black_key_h)

                # Color based on count: red=1, magenta=2, yellow=3+
                if note_count >= 3:
                    qd.setColor(vec4(1.0, 1.0, 0.2, 1))  # yellow
                elif note_count == 2:
                    qd.setColor(vec4(1.0, 0.2, 1.0, 1))  # magenta
                elif note_count == 1:
                    qd.setColor(vec4(1.0, 0.2, 0.2, 1))  # red
                else:
                    qd.setColor(vec4(0.15, 0.15, 0.15, 1))  # dark gray
            else:
                # White key - bottom-aligned, full height
                x = rel_idx * white_key_w
                y = h - white_key_h
                qd.setPosition(x + 1, y)  # +1 for gap
                qd.setSize(white_key_w - 2, white_key_h)

                # Color based on count: red=1, magenta=2, yellow=3+
                if note_count >= 3:
                    qd.setColor(vec4(1.0, 1.0, 0.3, 1))  # yellow
                elif note_count == 2:
                    qd.setColor(vec4(1.0, 0.3, 1.0, 1))  # magenta
                elif note_count == 1:
                    qd.setColor(vec4(1.0, 0.3, 0.3, 1))  # red
                else:
                    qd.setColor(vec4(0.95, 0.95, 0.95, 1))  # white

        self.canvas.markDirty()

################################################################
# Test Application
################################################################

class MoonlightApp(ComponentizedApplication):
    """App that plays Moonlight Sonata with piano keyboard visualization."""

    def __init__(self):
        super().__init__()
        self.frame_count = 0
        self.start_time = None
        self.synth = None
        self.sequencer = None
        self.playback = None
        self.song_duration = 0.0

        # Active notes tracked via sequencer callback (note -> count)
        self.active_notes = {}

        # Piano keyboard
        self.keyboard = None

        self.ezapp_args = {
            'width': 1024,
            'height': 300,
            'offscreen': False,
            'use_subsystems': ['gpu', 'audioO', 'lev2'],
        }

    ##############################################

    def _onUiInit(self):
        """Set up UI with PrimCanvas for piano keyboard."""
        lg_group = self.ezapp.topLayoutGroup
        lg_group.clearColorStd = vec4(0.1, 0.1, 0.15, 1)

        # Create PrimCanvas widget
        canvas_item = lg_group.makeChild(uiclass=ui.PrimCanvas, args=["piano_canvas"])

        # Anchor to fill parent
        for edge in ['top', 'left', 'bottom', 'right']:
            getattr(canvas_item.layout, edge).anchorTo(getattr(lg_group.layout, edge))

        canvas = canvas_item.widget
        canvas.bg_color = vec4(0.2, 0.2, 0.25, 1)
        canvas.draw_background = True

        # Create keyboard widget
        self.keyboard = PianoKeyboard(canvas)

    ##############################################

    def _onSynthInit(self, synth):
        """Initialize the synth and set up C++ sequencer playback."""
        self.synth = synth
        self.sequencer = synth.sequencer
        synth.system_tempo = TEMPO
        synth.masterGain = singularity.decibelsToLinear(6.0)
        synth.velCurvePower = 1.25

        # Set up buses
        mainbus = synth.outputBus("main")
        mainbus.gain = 0
        synth.setEffect(mainbus, "IR-BH1")

        # Load sound data
        krzdata = singularity.KrzSynthData()

        # Create sequence
        sequence = singularity.Sequence("moonlight")
        timebase = sequence.timebase
        timebase.numerator = 4
        timebase.denominator = 4
        timebase.tempo = TEMPO
        timebase.ppq = 100

        ts0 = timestamp(0, 0, 0)
        dur64m = timestamp(64, 0, 0)

        # Create piano track
        program = krzdata.bankData.programByName("Stereo_Grand")
        track = sequence.createTrack("piano")
        track.program = program
        clip = track.createEventClipAtTimeStamp("piano", ts0, dur64m)

        # Load MIDI file into sequence
        midi_path = singularity.baseDataPath() / "midifiles" / "moonlight.mid"
        print(f"Loading MIDI file: {midi_path}")
        midiToSingularitySequence(
            midifile=MidiFile(str(midi_path)),
            sequence=sequence,
            CLIP=clip,
            temposcale=1.9,
            feel=1
        )

        # Set up sequencer event callback for keyboard visualization
        def on_sequencer_event(note, velocity, duration, track_name):
            if velocity > 0:
                # Note on - increment count
                self.active_notes[note] = self.active_notes.get(note, 0) + 1
            else:
                # Note off - decrement count
                if note in self.active_notes:
                    self.active_notes[note] -= 1
                    if self.active_notes[note] <= 0:
                        del self.active_notes[note]

        self.sequencer.on_event = on_sequencer_event

        # Calculate song duration
        measures = 64
        beats_per_measure = timebase.numerator
        seconds_per_beat = 60.0 / TEMPO
        self.song_duration = measures * beats_per_measure * seconds_per_beat
        print(f"Song duration: {self.song_duration:.1f} seconds")

        # Start playback
        self.playback = self.sequencer.playSequence(sequence, 0.0)
        self.start_time = time.time()
        print(f"Playback started: {self.playback}")

    ##############################################

    def _onUpdate(self, updinfo):
        """Update - sync keyboard visualization with active notes."""
        self.frame_count += 1
        if self.start_time is None or self.synth is None:
            return

        # Update keyboard visualization from callback-tracked active notes
        if self.keyboard:
            self.keyboard.setActiveNotes(self.active_notes)

        # Auto-exit after piece completes
        elapsed = time.time() - self.start_time
        if elapsed >= self.song_duration + 2.0:
            print(f"  Playback complete after {self.frame_count} frames")
            self.ezapp.signalExit()

################################################################

def main():
    print("MIDI Player with C++ Sequencer")
    print("=" * 60)
    print("Playing: Moonlight Sonata (with piano keyboard visualization)")
    print()

    app = MoonlightApp()
    ezapp = app.createEzApp(name="MidiPlayer", width=1280, height=240)

    print("[Subsystem Status]")
    gpu = ezapp.getSubsystem("gpu")
    audio = ezapp.getSubsystem("audio")
    lev2 = ezapp.getSubsystem("lev2")
    print(f"  GPU:   {'READY' if gpu and gpu.currentState() == gpu.state_ready else 'NOT READY'}")
    print(f"  Audio: {'READY' if audio and audio.currentState() == audio.state_ready else 'NOT READY'}")
    print(f"  Lev2:  {'READY' if lev2 and lev2.currentState() == lev2.state_ready else 'NOT READY'}")

    print("\n[Running main loop - Moonlight Sonata playback]")
    ezapp.mainThreadLoop()

    print("\n[Shutdown]")
    ezapp.shutdown()

    print("Done!")
    return 0

if __name__ == '__main__':
    sys.exit(main())
