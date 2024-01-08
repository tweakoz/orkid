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
mid = MidiFile(str(midi_path/'moonlight.mid'), clip=True)

timestamp = singularity.TimeStamp

################################################################################

TEMPO = 120

audiodevice = singularity.device.instance()
synth = singularity.synth.instance()
synth.masterGain = singularity.decibelsToLinear(24)
mainbus = synth.outputBus("main")
synth.system_tempo = TEMPO
sequencer = synth.sequencer
#synth.setEffect(mainbus,"Reverb:FDN4")
#synth.setEffect(mainbus,"Reverb:FDN8")
synth.setEffect(mainbus,"Reverb:FDNX")
#synth.setEffect(mainbus,"Reverb:NiceVerb")
#synth.setEffect(mainbus,"StereoChorus")

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
      print("micros_per_quarter<%s>" % micros_per_quarter)
      TEMPO = 60000000.0/micros_per_quarter
      timebase.tempo = TEMPO
    if msg.type == 'time_signature':
      timebase.numerator = msg.numerator
      timebase.denominator = msg.denominator

print("numtracks<%d>" % len(mid.tracks))
print("timebase<%s>" % timebase)
print("micros_per_quarter",micros_per_quarter)

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

ppq = timebase.ppb

def calculate_timescale():
    return micros_per_quarter / (ppq * 1e6)
  

for miditrack in mid.tracks:
  note_map = {}
  time = 0  # Time in seconds
  TRIGGER = PIANO[2]
  timescale = calculate_timescale() 
  
  for msg in miditrack:
    # Update time with the current message's time
    time += msg.time * timescale

    # Check for tempo change
    if msg.type == 'set_tempo':
      micros_per_quarter = msg.tempo*0.45
      timescale = calculate_timescale() 
      
    # Process note_on events
    elif msg.type == 'note_on':
      n = msg.note
      v = msg.velocity
      if v == 0:
        if n in note_map:
          start_time = note_map[n][2]
          duration = time - start_time
          ts = timebase.timeToTimeStamp(start_time)
          dur = timebase.timeToTimeStamp(duration)
          TRIGGER.createNoteEvent(ts, dur, n, v)
          del(note_map[n])
      note_map[n] = (n, v, time)
      print("note_on<%s> vel<%d> time<%s> " % (n,v,time))

    # Process note_off events
    elif msg.type == 'note_off':
      n = msg.note
      if n in note_map:
        start_time = note_map[n][2]
        duration = time - start_time  # Calculate duration based on start and end times
        ts = timebase.timeToTimeStamp(start_time)
        dur = timebase.timeToTimeStamp(duration)
        TRIGGER.createNoteEvent(ts, dur, n, v)
        del(note_map[n])

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
    