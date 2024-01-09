#!/usr/bin/env python3

################################################################################
# singularity test for timestamps
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, time, signal
from obt import path
from orkengine.core import *
from orkengine.lev2 import singularity
sys.path.append((thisdir()).normalized.as_string)
from _seq import GenMidi

timestamp = singularity.TimeStamp

################################################################################

TEMPO = 120

audiodevice = singularity.device.instance()
synth = singularity.synth.instance()
synth.masterGain = singularity.decibelsToLinear(18)
mainbus = synth.outputBus("main")
synth.system_tempo = TEMPO
sequencer = synth.sequencer
#synth.setEffect(mainbus,"Reverb:FDN4")
synth.setEffect(mainbus,"Reverb:FDN8")
#synth.setEffect(mainbus,"Reverb:FDNX")
#synth.setEffect(mainbus,"Reverb:NiceVerb")
#synth.setEffect(mainbus,"StereoChorus")
#synth.setEffect(mainbus,"none")

################################################################################

syn_data_base = singularity.baseDataPath()/"kurzweil"
krzdata = singularity.KrzSynthData()

################################################################################

sequence = singularity.Sequence("seq1")
timebase = sequence.timebase
timebase.numerator = 4
timebase.denominator = 4
timebase.tempo = TEMPO
timebase.ppq = 100 # pulses per beat

ts0 = timestamp(0,0,0)
dur1b = timestamp(0,1,0)
dur2b = timestamp(0,2,0)
dur3b = timestamp(0,3,0)
dur1m = timestamp(1,0,0)
dur2m = timestamp(2,0,0)
dur4m = timestamp(4,0,0)
dur16m = timestamp(16,0,0)

######################################################
# create programs/tracks/clips
######################################################

def createTrack(name):
  program = krzdata.bankData.programByName(name)
  track = sequence.createTrack(name)
  track.program = program
  clip = track.createEventClipAtTimeStamp(name,ts0,dur16m)
  return (program,track,clip)

DOOM = createTrack("Doomsday")
CLICK = createTrack("Click")
BASS = createTrack("WonderSynth_Bass")
MUTES = createTrack("Guitar_Mutes_1")
PIANO = createTrack("Stereo_Grand")
STAPS = createTrack("Syncro_Taps")
PIZZO = createTrack("Wet_Pizz_")

######################################################

TRIGGER = PIANO[2]
GenMidi(sequence,0.0,timebase,TRIGGER)

######################################################

playback = sequencer.playSequence(sequence)

print(playback)

######################################################
# main loop
######################################################

def onCtrlC(signum, frame):
  sys.exit(0)

signal.signal(signal.SIGINT, onCtrlC)

while True:
  synth.mainThreadHandler()
    