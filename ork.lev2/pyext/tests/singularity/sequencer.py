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
from mido import MidiFile 

midi_path = singularity.baseDataPath()/"midifiles"
mid = MidiFile(str(midi_path/'castle1.mid'), clip=True)

timestamp = singularity.TimeStamp

################################################################################

TEMPO = 120

audiodevice = singularity.device.instance()
synth = singularity.synth.instance()
mainbus = synth.outputBus("main")
synth.system_tempo = TEMPO
sequencer = synth.sequencer
#synth.setEffect(mainbus,"Reverb:FDN4")
#synth.setEffect(mainbus,"Reverb:FDN8")
synth.setEffect(mainbus,"Reverb:FDNX")
#synth.setEffect(mainbus,"Reverb:NiceVerb")

################################################################################

syn_data_base = singularity.baseDataPath()/"kurzweil"
krzdata = singularity.KrzSynthData()

################################################################################

sequence = singularity.Sequence("seq1")
timebase = sequence.timebase
timebase.numerator = 4
timebase.denominator = 4
timebase.tempo = TEMPO
timebase.ppb = 100 # pulses per beat

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

micros_per_quarter = 0
timebase.ppb = mid.ticks_per_beat

for miditrack in mid.tracks:
  for msg in mid.tracks[0]:
    if msg.type == 'set_tempo':
      micros_per_quarter = msg.tempo
      TEMPO = 60000000.0/micros_per_quarter
      timebase.tempo = TEMPO
      #print("TEMPO<%s>" % TEMPO)
    if msg.type == 'time_signature':
      timebase.numerator = msg.numerator
      timebase.denominator = msg.denominator

print("timebase<%s>" % timebase)
print("micros_per_quarter",micros_per_quarter)

#assert(False)

TEMPO = 100.0
micros_per_quarter = 60000000.0/TEMPO
timebase.tempo = TEMPO

timescale = micros_per_quarter / (timebase.ppb*1e6)


num_note_ons = 0
num_note_offs = 0

for miditrack in mid.tracks:
  for msg in miditrack:
    if msg.type == 'note_on':
      num_note_ons += 1
    elif msg.type == 'note_off':
      num_note_offs += 1

print("num_note_ons<%d>" % num_note_ons)
print("num_note_offs<%d>" % num_note_offs)

for miditrack in mid.tracks:
  note_map = {}
  time = 0
  TRIGGER = PIANO[2]
  for msg in miditrack:
    if msg.type == 'note_on':
      n = msg.note
      v = msg.velocity
      t = msg.time
      if num_note_ons==num_note_offs:
        if n not in note_map:
          note_map[n] = (n,v,t)
      elif num_note_offs==0:
        time2 = time + 1.0
        ts = timebase.timeToTimeStamp(time)
        dur = timebase.timeToTimeStamp(time2-time)
        TRIGGER.createNoteEvent(ts,dur,n,v)
      time += t*timescale
      #print("non %d %d %g" % (n,v,time))
    elif msg.type == 'note_off':
      n = msg.note
      t = msg.time
      time2 = time + t*timescale 
      if n in note_map:
        event = note_map[n]
        ts = timebase.timeToTimeStamp(time)
        dur = timebase.timeToTimeStamp(0.5+time2-time)
        TRIGGER.createNoteEvent(ts,dur,event[0],event[1])
        del(note_map[n])
      time = time2
      #print("noff")

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
    