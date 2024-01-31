#!/usr/bin/env python3

################################################################################
# singularity test for timestamps
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, time, signal, argparse
from obt import path
from orkengine.core import *
from orkengine.lev2 import singularity
sys.path.append((thisdir()).normalized.as_string)
from _seq import midiToSingularitySequence
from mido import MidiFile 

timestamp = singularity.TimeStamp

################################################################################
# arguments
# -s seqid {0..3} : default 0
# -c : enable click track
################################################################################

parser = argparse.ArgumentParser(description='singularity sequencer test')
parser.add_argument('-s', '--seqid', type=int, default=0, help='sequence id')
parser.add_argument('-c', '--click', action='store_true', help='enable click track')
parser.add_argument("-g", "--gain", type=float, default=-48, help="gain(dB)")
args = parser.parse_args()
seqid = args.seqid
add_click = args.click

################################################################################

TEMPO = 120

audiodevice = singularity.device.instance()
synth = singularity.synth.instance()
mainbus = synth.outputBus("main")
synth.system_tempo = TEMPO
sequencer = synth.sequencer


#synth.setEffect(mainbus,"Reverb:FDN4")
#synth.setEffect(mainbus,"Reverb:FDN8")
#synth.setEffect(mainbus,"Reverb:FDNX")
synth.setEffect(mainbus,"Reverb:NiceVerb")
#synth.setEffect(mainbus,"StereoChorus")
#synth.setEffect(mainbus,"none")

auxbus = synth.createOutputBus("aux")
synth.setEffect(auxbus,"none")
auxbus.gain = 0.0 #args.gain
mainbus.gain = 0
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
dur32m = timestamp(32,0,0)
dur64m = timestamp(64,0,0)

######################################################
# create programs/tracks/clips
######################################################

def createTrack(name):
  program = krzdata.bankData.programByName(name)
  track = sequence.createTrack(name)
  track.program = program
  clip = track.createEventClipAtTimeStamp(name,ts0,dur64m)
  return (program,track,clip)

DOOM = createTrack("Doomsday")
CLICK = createTrack("Click")
BASS = createTrack("WonderSynth_Bass")
MUTES = createTrack("Guitar_Mutes_1")
PIANO = createTrack("Stereo_Grand")
STAPS = createTrack("Syncro_Taps")
PIZZO = createTrack("Wet_Pizz_")

######################################################

def genSingularitySequence(
  name="",      # midi file name
  clip=None,    # clip to which add the events
  temposcale=1, # tempo scale factor
  feel=0,       # clock ticks to randomly add to each note
  gain=0):      # master synth gain in dB

  synth.masterGain = singularity.decibelsToLinear(gain)
  midi_path = singularity.baseDataPath()/"midifiles"
  midiToSingularitySequence(
    midifile=MidiFile(str(midi_path/name)),
    sequence=sequence,
    CLIP=clip,
    temposcale=temposcale,
    feel=feel)

######################################################
if seqid==0:
  genSingularitySequence(name="moonlight.mid",temposcale=1.9,feel=1,clip=PIANO[2],gain=6)
  synth.velCurvePower = 1.25
  auxbus.gain = +6
elif seqid==1:
  genSingularitySequence(name="castle1.mid",temposcale=1.0,feel=30,clip=PIANO[2],gain=-6)
  synth.velCurvePower = 1.25
  auxbus.gain = -36
elif seqid==2:
  genSingularitySequence(name="castle2.mid",temposcale=1.0,feel=10,clip=PIANO[2],gain=-6)
  synth.velCurvePower = 1.25
  auxbus.gain = -24
elif seqid==3:
  genSingularitySequence(name="castle3.mid",temposcale=1.0,feel=30,clip=PIANO[2],gain=-6)
  synth.velCurvePower = 1.25
  auxbus.gain = -96

if add_click:
  program = krzdata.bankData.programByName("Click")
  track = sequence.createTrack("click")
  track.program = program
  clip = track.createFourOnFloorClipAtTimeStamp("click",ts0,dur64m)
  track.outputbus = auxbus
  
######################################################

playback = sequencer.playSequence(sequence,0.0)

print(playback)

######################################################
# main loop
######################################################

def onCtrlC(signum, frame):
  sys.exit(0)

signal.signal(signal.SIGINT, onCtrlC)

while True:
  synth.mainThreadHandler()
    
