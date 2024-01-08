from mido import MidiFile 
from orkengine.core import *
from orkengine.lev2 import singularity

midi_path = singularity.baseDataPath()/"midifiles"
mid = MidiFile(str(midi_path/'moonlight.mid'), clip=True)

######################################################

micros_per_quarter = 0
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

  

def GenMidi(timeoffset,timebase,TRIGGER):
  timebase.ppq = mid.ticks_per_beat

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

  timescale = micros_per_quarter / (timebase.ppq*1e6)

  ppq = timebase.ppq
  def calculate_timescale():
    return micros_per_quarter / (ppq * 1e6)
  for miditrack in mid.tracks:
    note_map = {}
    time = 0  # Time in seconds
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
            start_time = note_map[n][2]+timeoffset
            duration = timeoffset+time - start_time
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
          start_time = note_map[n][2]+timeoffset
          duration = timeoffset+time - start_time  # Calculate duration based on start and end times
          ts = timebase.timeToTimeStamp(start_time)
          dur = timebase.timeToTimeStamp(duration)
          TRIGGER.createNoteEvent(ts, dur, n, v)
          del(note_map[n])