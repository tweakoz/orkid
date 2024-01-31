from orkengine.core import *
from orkengine.lev2 import singularity
import random 

######################################################
micros_per_quarter = 0
######################################################

def midiToSingularitySequence(
  midifile = None,   # midi file object
  sequence = None,   # singularity.Sequence object
  CLIP = None,       # clip to which add the events
  temposcale = 1.0,  # tempo scale factor
  feel = 0):         # clock ticks to randomly add to each note

  timebase_seq = sequence.timebase
  timebase_seq.ppq = int(midifile.ticks_per_beat)

  for miditrack in midifile.tracks:
    for msg in midifile.tracks[0]:
      if msg.type == 'set_tempo':
        micros_per_quarter = msg.tempo
        print("micros_per_quarter<%s>" % micros_per_quarter)
        TEMPO = temposcale*(60000000.0/micros_per_quarter)
        timebase_seq.tempo = TEMPO
      if msg.type == 'time_signature':
        timebase_seq.numerator = msg.numerator
        timebase_seq.denominator = msg.denominator

  print("numtracks<%d>" % len(midifile.tracks))
  print("timebase<%s>" % timebase_seq)
  print("micros_per_quarter",micros_per_quarter)
  
  timebase_start = timebase_seq.clone()

  TPQ = timebase_start.ppq
 
  def calculate_tempo():
    UPQ = micros_per_quarter
    usptick = UPQ / TPQ
    QPU = 1.0 / UPQ
    QPS = QPU * 1e6
    QPM = QPS * 60.0
    return QPM*temposcale

  ##################################################3
  # create tempo map
  ##################################################3

  tr = 0
  timebase_mut = timebase_start.clone()
  for miditrack in midifile.tracks:
    clocks = 0  # Time in seconds
    for msg in miditrack:
      # Update time with the current message's time
      clocks += msg.time 
      # Check for tempo change
      if msg.type == 'set_tempo':
        micros_per_quarter = msg.tempo
        tbnext = timebase_mut.clone()
        tbnext.tempo = calculate_tempo()
        timebase_mut.tempo = tbnext.tempo
        tbnext.parent = timebase_mut
        tbnext.basetime = clocks
        tbnext.duration = 0.0
        print("tr<%d> mpq<%d> tempo<%s>" % (tr, micros_per_quarter,tbnext.tempo))
        #sequence.setTimeBaseForTime(time,tbnext)
    tr += 1

  ##################################################3
  # perform piece
  ##################################################3

  timebase_mut = timebase_start.clone()
  for miditrack in midifile.tracks:
    note_map = {}
    clock = 0
    random.seed(0)
    for msg in miditrack:
      msg_num_clocks = msg.time 
      
      clock += msg_num_clocks # Update time with the current message's time

      #########################################
      # enqueue event to sequence
      #########################################

      def ENQUEUE(n,v):
        ts = singularity.TimeStamp()
        dur = singularity.TimeStamp()
        feel_clocks = int(random.uniform(-1,1)*feel)
        ts.clocks = note_map[n][2] + feel_clocks
        ts = timebase_mut.reduce(ts)
        dur.clocks = int(msg_num_clocks+100)
        dur = timebase_mut.reduce(dur)
        CLIP.createNoteEvent(ts, dur, n, v)
        del(note_map[n])

      #########################################
      # Check for tempo change
      #########################################

      if msg.type == 'set_tempo':
        micros_per_quarter = msg.tempo
        timebase_mut.tempo = calculate_tempo()
    
      #########################################
      # Process note_on events
      #########################################

      elif msg.type == 'note_on':
        n = msg.note
        v = msg.velocity
        # note_on with velocity==0 is a note_off
        if v == 0:
          if n in note_map:
            orig_vel = note_map[n][1]
            ENQUEUE(n,orig_vel)
        note_map[n] = (n, v, clock)

      #########################################
      # Process note_off events
      #########################################

      elif msg.type == 'note_off':
        n = msg.note
        if n in note_map:
          ENQUEUE(n,v)


  #assert(False)