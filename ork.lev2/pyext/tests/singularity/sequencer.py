#!/usr/bin/env python3

################################################################################
# singularity test for timestamps
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, time, signal
from orkengine.core import *
from orkengine.lev2 import singularity

timestamp = singularity.TimeStamp

################################################################################

TEMPO = 120

audiodevice = singularity.device.instance()
synth = singularity.synth.instance()
synth.system_tempo = TEMPO
sequencer = synth.sequencer

################################################################################

syn_data_base = singularity.baseDataPath()/"kurzweil"
krzdata = singularity.KrzSynthData()
doomsday = krzdata.bankData.programByName("Doomsday")
click = krzdata.bankData.programByName("Click")

print(doomsday)
print(click)

################################################################################

sequence = singularity.Sequence("seq1")
timebase = sequence.timebase
timebase.numerator = 4
timebase.denominator = 4
timebase.tempo = TEMPO
timebase.ppb = 100 # pulses per beat

print(timebase)

ts0 = timestamp(0,0,0)
dur1b = timestamp(0,1,0)
dur2b = timestamp(0,2,0)
dur3b = timestamp(0,3,0)
dur1m = timestamp(1,0,0)
dur2m = timestamp(2,0,0)
dur4m = timestamp(4,0,0)
dur16m = timestamp(16,0,0)
tr_doom = sequence.createTrack("doomsday-track")
tr_doom.program = doomsday

cl1 = tr_doom.createEventClipAtTimeStamp("clip1",timestamp(0,0,0),dur2m)
#cl2 = tr_doom.createEventClipAtTimeStamp("clip2",timestamp(1,0,0),dur2b)
#cl3 = tr_doom.createEventClipAtTimeStamp("clip3",timestamp(2,1,0),dur1m)

ev1 = cl1.createNoteEvent(ts0,cl1.duration,60,127)
#ev2 = cl2.createNoteEvent(ts0,cl2.duration,72,127)
#ev3 = cl3.createNoteEvent(ts0,cl3.duration,84,127)
print(ts0)
print(tr_doom)
print(cl1)

tr_click = sequence.createTrack("click-track")
tr_click.program = click
cl_click = tr_click.createFourOnFloorClipAtTimeStamp("clip4",timestamp(0,0,0),dur4m)


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
    