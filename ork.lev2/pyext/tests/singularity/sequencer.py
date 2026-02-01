#!/usr/bin/env ork.python

################################################################################
# singularity sequencer test
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, signal, argparse, time
from orkengine.core import *
from orkengine.lev2 import *

sys.path.append((thisdir()).normalized.as_string)
from _seq import midiToSingularitySequence
from mido import MidiFile

timestamp = singularity.TimeStamp

################################################################################
# arguments
################################################################################

parser = argparse.ArgumentParser(description='singularity sequencer test')
parser.add_argument('-s', '--seqid', type=int, default=0, help='sequence id')
parser.add_argument('-c', '--click', action='store_true', help='enable click track')
parser.add_argument("-g", "--gain", type=float, default=-48, help="gain(dB)")
args = parser.parse_args()

################################################################################

TEMPO = 120

################################################################################

class SequencerApp(object):

  def __init__(self):
    self.synth = None
    self.sequencer = None
    self.playback = None
    self.start_time = None
    self.song_duration = None  # Will be set when sequence is created

    self.ezapp = OrkEzApp.create(
      self,
      name="SequencerTest",
      use_subsystems=['opq', 'core', 'audioO']
    )

    def onCtrlC(signum, frame):
      print("Ctrl+C received, exiting...")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onSynthInit(self, synth):
    """Called when synth is ready."""
    self.synth = synth
    self.sequencer = synth.sequencer
    synth.system_tempo = TEMPO

    # Set up buses
    mainbus = synth.outputBus("main")
    mainbus.gain = 0
    synth.setEffect(mainbus, "IR-BH1")

    auxbus = synth.createOutputBus("aux")
    auxbus.gain = 0.0
    synth.setEffect(auxbus, "none")

    # Load sound data
    krzdata = singularity.KrzSynthData()

    # Create sequence
    sequence = singularity.Sequence("seq1")
    timebase = sequence.timebase
    timebase.numerator = 4
    timebase.denominator = 4
    timebase.tempo = TEMPO
    timebase.ppq = 100

    ts0 = timestamp(0, 0, 0)
    dur64m = timestamp(64, 0, 0)

    # Helper to create tracks
    def createTrack(name):
      program = krzdata.bankData.programByName(name)
      track = sequence.createTrack(name)
      track.program = program
      clip = track.createEventClipAtTimeStamp(name, ts0, dur64m)
      return (program, track, clip)

    # Create tracks
    PIANO = createTrack("Stereo_Grand")

    # Generate sequence from MIDI
    def genSingularitySequence(name, clip, temposcale=1, feel=0, gain=0):
      synth.masterGain = singularity.decibelsToLinear(gain)
      midi_path = singularity.baseDataPath() / "midifiles"
      midiToSingularitySequence(
        midifile=MidiFile(str(midi_path / name)),
        sequence=sequence,
        CLIP=clip,
        temposcale=temposcale,
        feel=feel
      )

    # Select sequence based on args
    seqid = args.seqid
    if seqid == 0:
      genSingularitySequence("moonlight.mid", PIANO[2], temposcale=1.9, feel=1, gain=6)
      synth.velCurvePower = 1.25
      auxbus.gain = +6
    elif seqid == 1:
      genSingularitySequence("castle1.mid", PIANO[2], temposcale=1.0, feel=30, gain=-6)
      synth.velCurvePower = 1.25
      auxbus.gain = -36
    elif seqid == 2:
      genSingularitySequence("castle2.mid", PIANO[2], temposcale=1.0, feel=10, gain=-6)
      synth.velCurvePower = 1.25
      auxbus.gain = -24
    elif seqid == 3:
      genSingularitySequence("castle3.mid", PIANO[2], temposcale=1.0, feel=30, gain=-6)
      synth.velCurvePower = 1.25
      auxbus.gain = -96

    # Add click track if requested
    if args.click:
      program = krzdata.bankData.programByName("Click")
      track = sequence.createTrack("click")
      track.program = program
      clip = track.createFourOnFloorClipAtTimeStamp("click", ts0, dur64m)
      track.outputbus = auxbus

    # Calculate song duration: measures * beats_per_measure * seconds_per_beat
    # At 120 BPM, one beat = 0.5 seconds. 64 measures * 4 beats = 256 beats = 128 seconds
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

  def onRunLoopIteration(self):
    """Called each main loop iteration. Exit when song is done."""
    if self.start_time is None or self.song_duration is None:
      return  # Not yet initialized

    elapsed = time.time() - self.start_time
    if elapsed >= self.song_duration:
      print(f"\nSong complete ({elapsed:.1f}s)")
      self.ezapp.signalExit()

################################################################################

app = SequencerApp()
app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
