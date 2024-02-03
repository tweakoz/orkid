#!/usr/bin/env python3

from mido import MidiFile
from orkengine.core import *
from orkengine.lev2 import singularity

midi_path = singularity.baseDataPath()/"midifiles"

def calculate_mbc(mid, ticks_per_beat):
    current_tempo = 500000  # Default tempo (120 BPM)
    current_time_signature = (4, 4)  # Default time signature
    elapsed_ticks = 0

    for track in mid.tracks:
        for msg in track:
            elapsed_ticks += msg.time

            if msg.type == 'set_tempo':
                current_tempo = msg.tempo
            elif msg.type == 'time_signature':
                current_time_signature = (msg.numerator, msg.denominator)

            measure_length = ticks_per_beat * current_time_signature[0]
            measure = elapsed_ticks // measure_length
            beat = (elapsed_ticks % measure_length) // ticks_per_beat
            clock = elapsed_ticks % ticks_per_beat

            print(f"M{measure} B{beat} C{clock} T{elapsed_ticks}")

midi_file = MidiFile(str(midi_path/'castle1.mid'), clip=True)
calculate_mbc(midi_file, midi_file.ticks_per_beat)

