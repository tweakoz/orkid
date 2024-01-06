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

print(doomsday)

################################################################################

sequence = singularity.Sequence()
timebase = sequence.timebase
timebase.numerator = 4
timebase.denominator = 4
timebase.tempo = TEMPO
timebase.ppb = 100

print(timebase)

ts0 = singularity.TimeStamp(0,0,0)
tr1 = sequence.createTrack("track1")
tr1.program = doomsday

cl1 = tr1.createEventClipAtTimeStamp("clip1",ts0)
print(ts0)
print(tr1)
print(cl1)

######################################################
# main loop
######################################################

def onCtrlC(signum, frame):
  sys.exit(0)

signal.signal(signal.SIGINT, onCtrlC)

while True:
  synth.mainThreadHandler()
    