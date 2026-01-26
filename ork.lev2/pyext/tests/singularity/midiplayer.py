#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Tests for ComponentizedApplication with HFSM Subsystem integration + Audio.

Plays Moonlight Sonata from MIDI file using Python-based sequencing
with synth.keyOn/keyOff, displays a piano keyboard visualization
showing active notes in red.

Usage:
  ./test_componentized_subsystems2.py
"""

import sys
import os
import time
from orkengine.core import *
from orkengine.lev2 import *
from ork.app.application import ComponentizedApplication
from mido import MidiFile

tokens = CrcStringProxy()

################################################################
# MIDI Parser
################################################################

def parseMidiFile(midi_path, tempo_scale=1.0):
    """Parse MIDI file and return list of (note, start_time, duration, velocity) tuples."""
    midifile = MidiFile(midi_path)

    tempo_usec = 500000
    for track in midifile.tracks:
        for msg in track:
            if msg.type == 'set_tempo':
                tempo_usec = msg.tempo
                break

    ticks_per_beat = midifile.ticks_per_beat
    seconds_per_tick = (tempo_usec / 1_000_000.0) / ticks_per_beat * tempo_scale

    note_ons = {}
    events = []

    for track in midifile.tracks:
        current_time = 0.0
        for msg in track:
            current_time += msg.time * seconds_per_tick
            if msg.type == 'note_on' and msg.velocity > 0:
                note_ons[msg.note] = (current_time, msg.velocity)
            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                if msg.note in note_ons:
                    start_time, velocity = note_ons[msg.note]
                    duration = current_time - start_time
                    events.append((msg.note, start_time, duration, velocity))
                    del note_ons[msg.note]

    events.sort(key=lambda e: e[1])

    if events:
        last_event = max(events, key=lambda e: e[1] + e[2])
        total_duration = last_event[1] + last_event[2]
    else:
        total_duration = 0.0

    return events, total_duration

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
        self.active_notes = set()
        self._gpu_initialized = False

        # Set up pre-render callback
        self.canvas.onPreRender = self._onPreRender

    def setActiveNotes(self, notes):
        """Update the set of currently active notes."""
        self.active_notes = set(notes)

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

            if isBlackKey(midi_note):
                # Black key - centered on the boundary between white keys
                # Bottom-aligned, shorter than white keys
                x = (rel_idx + 1) * white_key_w - black_key_w / 2
                y = h - black_key_h
                qd.setPosition(x, y)
                qd.setSize(black_key_w, black_key_h)

                # Color: red if active, dark gray if not
                if midi_note in self.active_notes:
                    qd.setColor(vec4(1.0, 0.2, 0.2, 1))
                else:
                    qd.setColor(vec4(0.15, 0.15, 0.15, 1))
            else:
                # White key - bottom-aligned, full height
                x = rel_idx * white_key_w
                y = h - white_key_h
                qd.setPosition(x + 1, y)  # +1 for gap
                qd.setSize(white_key_w - 2, white_key_h)

                # Color: red if active, white if not
                if midi_note in self.active_notes:
                    qd.setColor(vec4(1.0, 0.3, 0.3, 1))
                else:
                    qd.setColor(vec4(0.95, 0.95, 0.95, 1))

        self.canvas.markDirty()

################################################################
# Test Application
################################################################

class MoonlightApp(ComponentizedApplication):
    """Test app that plays Moonlight Sonata with piano keyboard visualization."""

    def __init__(self):
        super().__init__()
        self.frame_count = 0
        self.start_time = None
        self.synth = None
        self.program = None

        # Sequencing state
        self.note_events = []
        self.next_event_idx = 0
        self.active_voices = {}
        self.total_duration = 0.0

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
        """Initialize the synth and load the MIDI file."""
        self.synth = synth
        self.synth.masterGain = singularity.decibelsToLinear(6.0)

        mainbus = synth.outputBus("main")
        mainbus.gain = 0
        synth.setEffect(mainbus, "Reverb:NiceVerb")

        krzdata = singularity.KrzSynthData()
        self.program = krzdata.bankData.programByName("Stereo_Grand")
        synth.velCurvePower = 1.25

        midi_path = os.path.join(
            os.environ.get("OBT_STAGE", ""),
            "share", "singularity", "midifiles", "moonlight.mid"
        )
        print(f"Loading MIDI file: {midi_path}")

        if not os.path.exists(midi_path):
            print(f"ERROR: MIDI file not found: {midi_path}")
            return

        self.note_events, self.total_duration = parseMidiFile(midi_path, tempo_scale=1.2)
        print(f"Loaded {len(self.note_events)} note events, duration: {self.total_duration:.1f}s")

    ##############################################

    def _onUpdateInit(self):
        """Initialize the start time."""
        self.start_time = time.time()

    ##############################################

    def _onUpdate(self, updinfo):
        """Update - process note events based on elapsed time."""
        self.frame_count += 1
        if self.start_time is None or self.synth is None or self.program is None:
            return

        elapsed = time.time() - self.start_time

        # Process note-offs
        notes_to_remove = []
        for note, (voice, end_time) in self.active_voices.items():
            if elapsed >= end_time:
                self.synth.keyOff(voice, note, 0)
                notes_to_remove.append(note)
        for note in notes_to_remove:
            del self.active_voices[note]

        # Process note-ons
        while self.next_event_idx < len(self.note_events):
            note, start_time, duration, velocity = self.note_events[self.next_event_idx]
            if elapsed >= start_time:
                voice = self.synth.keyOn(note, velocity, self.program, None)
                end_time = start_time + duration
                self.active_voices[note] = (voice, end_time)
                self.next_event_idx += 1
            else:
                break

        # Update keyboard visualization
        if self.keyboard:
            self.keyboard.setActiveNotes(self.active_voices.keys())

        # Auto-exit after piece completes
        if elapsed >= self.total_duration + 2.0:
            for note, (voice, _) in self.active_voices.items():
                self.synth.keyOff(voice, note, 0)
            self.active_voices.clear()
            print(f"  Playback complete after {self.frame_count} frames")
            self.ezapp.signalExit()

################################################################

def main():
    print("Testing ComponentizedApplication with MIDI Playback")
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
