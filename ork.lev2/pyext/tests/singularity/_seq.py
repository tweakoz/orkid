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

  

def GenMidi(sequence,timeoffset,timebase,TRIGGER):
  timebase.ppq = int(mid.ticks_per_beat*2.5)

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

  timebase_start = timebase.clone()

  timescale = micros_per_quarter / (timebase.ppq*1e6)

  ppq = timebase_start.ppq
  def calculate_timescale():
    return micros_per_quarter / (ppq * 1e6)
  def calculate_tempo():
    return 60000000.0 / micros_per_quarter

  ##################################################3
  # create tempo map
  ##################################################3

  for miditrack in mid.tracks:
    time = 0  # Time in seconds
    timescale = calculate_timescale() 
    for msg in miditrack:
      # Update time with the current message's time
      time += msg.time * timescale

      # Check for tempo change
      if msg.type == 'set_tempo':
        micros_per_quarter = msg.tempo#*0.45
        timescale = calculate_timescale() 
        tbnext = timebase.clone()
        tbnext.tempo = calculate_tempo()
        timebase.tempo = tbnext.tempo*2
        tbnext.parent = timebase
        tbnext.basetime = time
        tbnext.duration = 0.0
        print("tempo<%s>" % tbnext.tempo)
        sequence.setTimeBaseForTime(time,tbnext)

  ##################################################3
  # perform piece
  ##################################################3

  for miditrack in mid.tracks:
    note_map = {}
    clock = 0
    for msg in miditrack:
      msg_num_clocks = msg.time
      clock += msg_num_clocks # Update time with the current message's time

      #########################################
      # enqueue event to sequence
      #########################################

      def ENQUEUE(n,v):
        ts = singularity.TimeStamp()
        dur = singularity.TimeStamp()
        ts.clocks = note_map[n][3]
        ts = timebase.reduce(ts)
        dur.clocks = int(msg_num_clocks+100)
        dur = timebase.reduce(dur)
        TRIGGER.createNoteEvent(ts, dur, n, v)
        del(note_map[n])

      #########################################
      # Check for tempo change
      #########################################

      if msg.type == 'set_tempo':
        micros_per_quarter = msg.tempo #*0.45
        timebase.tempo = calculate_tempo()
    
      #########################################
      # Process note_on events
      #########################################

      elif msg.type == 'note_on':
        n = msg.note
        v = msg.velocity
        # note_on with velocity==0 is a note_off
        if v == 0:
          if n in note_map:
            ENQUEUE(n,v)
        note_map[n] = (n, v, time,clock)

      #########################################
      # Process note_off events
      #########################################

      elif msg.type == 'note_off':
        n = msg.note
        if n in note_map:
          ENQUEUE(n,v)
