#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, random, numpy, obt.path, time, bisect
import plotly.graph_objects as go
from collections import defaultdict
import re
from orkengine.core import *
from orkengine.lev2 import *
from _seq import midiToSingularitySequence
from mido import MidiFile 

################################################################################
sys.path.append((thisdir()).normalized.as_string) # add parent dir to path
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
################################################################################

def find_index(sorted_list, value):
    index = bisect.bisect_left(sorted_list, value)
    if index != len(sorted_list) and sorted_list[index] == value:
        return index
    return -1  # Return -1 or some other value to indicate "not found"

class TrackAndClip:
  def __init__(self,track,clip):
    self.track = track
    self.clip = clip

class SingulTestApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,left=420, top=100, height=720,width=1280)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 5
    self.held_voices = []
    rccounts = [3,2]
    self.griditems = lg_group.makeRowsColumns(
      rccounts = rccounts,
      margin = 4,
      uiclass = ui.Box,
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )

    for g in self.griditems:
      g.widget.ignoreEvents = True

    ######################### 

    root_layout = lg_group.layout
    self.time = 0.0
    self.octave = 5
    self.charts = {}
    self.chart_events = {}
    self.layermask = 0xffffffff
    self.layerID = 0
    self.click_prog = None
    self.click_noteL = 60
    self.click_noteH = 60
    
    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):
    self.context = ctx
    self.curseq = None
    self.audiodevice = singularity.device.instance()
    self.synth = singularity.synth.instance()
    self.synth.system_tempo = 120.0
    self.sequencer = self.synth.sequencer

    self.mainbus = self.synth.outputBus("main")
    self.mainbus_source = self.mainbus.createScopeSource()
    #self.synth.setEffect(self.mainbus,"Reverb:OilTank")
    self.synth.setEffect(self.mainbus,"none")
    self.numaux = 9
    self.auxbusses = []
    self.auxbus_sources = []
    for i in range(0,self.numaux):
      self.auxbusses += [self.synth.createOutputBus("aux%d" % (i+1))]
      self.auxbus_sources += [self.auxbusses[i].createScopeSource()]
      self.synth.setEffect(self.auxbusses[i],"none")

    lg_group = self.ezapp.topLayoutGroup


    self.rec_trackclips = {}

    ######################### 
    # create an profiler view
    #  replace the 2nd griditem with it
    ######################### 

    item = lg_group.makeChild( uiclass = singularity.ProfilerView,
                               args = ["YO"] )
    
    self.profview = lg_group.getUserVar("profilerviews.YO")
    lg_group.replaceChild(self.griditems[1].layout,item)
    item.widget.ignoreEvents = True

    ######################### 
    # create an program view
    #  replace the 3nd griditem with it
    ######################### 

    item = lg_group.makeChild( uiclass = singularity.ProgramView,
                               args = ["PROGRAM"] )
    
    self.pgmview = lg_group.getUserVar("programviews.PROGRAM")
    lg_group.replaceChild(self.griditems[2].layout,item)
    item.widget.ignoreEvents = True

    ######################### 
    # create an oscope
    #  replace the 4nd griditem with it
    ######################### 

    item = lg_group.makeChild( uiclass = singularity.Oscilloscope,
                               args = ["MAINBUS"] )
    
    self.oscope = lg_group.getUserVar("oscilloscopes.MAINBUS")
    self.oscope_sink = self.oscope.sink

    lg_group.replaceChild(self.griditems[3].layout,item)

    ######################### 
    # create an spectrum analyzer
    #  replace the 5th griditem with it
    ######################### 

    item = lg_group.makeChild( uiclass = singularity.SpectrumAnalyzer,
                               args = ["MAINBUS"] )
    
    self.spectra = lg_group.getUserVar("analyzers.MAINBUS")
    self.spectra_sink = self.spectra.sink

    lg_group.replaceChild(self.griditems[4].layout,item)
    item.widget.ignoreEvents = True


    ######################### 

    self.mainbus_source.connect(self.oscope_sink)
    self.mainbus_source.connect(self.spectra_sink)
    self.program_source = self.mainbus_source
    ######################### 

    self.gain = -24.0
    self.synth.masterGain = singularity.decibelsToLinear(self.gain)
    self.sorted_progs = []
    self.octave = 5
    self.charts = {}
    self.chart_events = {}

    # find index of "Bells and Chimes" in sorted_progs
    self.prog_index = 0

    self.base_notes = {
        ord("A"): 0,
        ord("W"): 1,
        ord("S"): 2,
        ord("E"): 3,
        ord("D"): 4,
        ord("F"): 5,
        ord("T"): 6,
        ord("G"): 7,
        ord("Y"): 8,
        ord("H"): 9,
        ord("U"): 10,
        ord("J"): 11,
        ord("K"): 12,
        ord("O"): 13,
        ord("L"): 14,
        ord("P"): 15,
        ord(";"): 16,
        ord("'"): 17,
    }
    self.voices = dict()
  #####################################
  def onUpdate(self,updinfo):
    self.time = updinfo.absolutetime
  #####################################
  def onGpuUpdate(self,ctx):
    pass
  #####################################
  def genMods(self):
    return None
  #####################################
  def onNote(self,voice):
    pass
  #####################################
  def prLayerMask(self):
    as_bools = binary_str = format(self.layermask, '032b')[::-1]
    print("###########################################")
    print("layerID<%d> layermask<%s> " % (self.layerID,as_bools))
    print("###########################################")
    pass
  #####################################
  def setBusProgram(self,bus,prg):
    self.synth.programbus = bus
    self.synth.programbus.uiprogram = prg
  #####################################
  def setUiProgram(self,prg):
    self.synth.programbus.uiprogram = prg
    if self.pgmview:
      self.pgmview.setProgram(prg)
  #####################################
  def assignTRC(self,trc):
    self.TRC = trc
  #####################################
  def createTrackAndClipForBus(self,bus):
    if bus.uiprogram != None and \
       self.curseq != None:
      track = self.curseq.createTrack("track-"+bus.name)
      track.program = bus.uiprogram
      ts0 = singularity.TimeStamp()
      dur4 = singularity.TimeStamp()
      dur4.measures = 4
      clip = track.createEventClipAtTimeStamp("clip-"+bus.name,ts0,dur4)
      track.outputbus = bus
      trandclip = TrackAndClip(track, clip)
      self.rec_trackclips[bus] = trandclip
      return trandclip
    return TrackAndClip(None,None)
  #####################################
  def _setSource(self,bus,source):
    self.program_source.disconnect(self.oscope_sink)
    self.program_source.disconnect(self.spectra_sink)
    prev_bus = self.synth.programbus
    self.synth.programbus = bus
    self.program_source = source
    source.connect(self.oscope_sink)
    source.connect(self.spectra_sink)
    if bus.uiprogram != None:
      self.prog = bus.uiprogram
    else:
      self.prog = prev_bus.uiprogram
    self.setUiProgram(self.prog)
    if bus in self.rec_trackclips:
      trandclip = self.rec_trackclips[bus]
      trandclip.track.program = self.prog
      self.assignTRC(trandclip)
    else:
      trandclip = self.createTrackAndClipForBus(bus)
      if trandclip.track != None:
        trandclip.track.program = self.prog
        self.assignTRC(trandclip)
  #####################################
  def onUiEvent(self,uievent):
    res = ui.HandlerResult()
    res.setHandler( self.ezapp.topWidget )
    if uievent.code == tokens.KEY_REPEAT.hashed or uievent.code==tokens.KEY_DOWN.hashed:
      KC = uievent.keycode
      ###############  
      def _xxx(layer,bus,source):
        if uievent.shift:
          self.synth.soloLayer = layer
        else:
          self._setSource(bus,source)
      ###############  
      def _xyz():
        if self.prog_index < 0:
          self.prog_index = len(self.sorted_progs)-1
        elif self.prog_index >= len(self.sorted_progs):
          self.prog_index = 0
        prgname = self.sorted_progs[self.prog_index]
        self.prog = self.soundbank.programByName(prgname)
        self.synth.programbus.uiprogram = self.prog
        if self.pgmview:
          self.pgmview.setProgram(self.prog)
        #TODO : update seq-track programs
        seqr = self.sequencer
        if self.synth.programbus in self.rec_trackclips.keys():
          track = self.rec_trackclips[self.synth.programbus].track
          track.program = self.prog
        pass
      ###############  
      if KC == ord("0"): # solo layer off
        _xxx(-1,self.mainbus,self.mainbus_source)
      ###############  
      elif KC >= ord("1") and KC <= ord("9"): 
        i = KC - ord("1")
        _xxx(i,self.auxbusses[i],self.auxbus_sources[i])
      ###############  
      elif KC == ord(" "): # hold drones
        for KC in self.voices:
          voice = self.voices[KC]
          self.held_voices += [voice]
        self.voices = dict()
      ###############  
      elif KC == ord("C"): # release drones
        for v in self.held_voices:
          note = v.note
          self.synth.keyOff(v,note,0)
        self.held_voices = []
      ###############  
      elif KC == ord(","): # prev program
        self.prog_index -= 1
        _xyz()
        return res
      ###############  
      elif KC == ord("."): # next program
        self.prog_index += 1
        _xyz()
        return res
      ###############  
    if uievent.code == tokens.KEY_DOWN.hashed:
      KC = uievent.keycode
      if KC in self.base_notes:
       if KC not in self.voices:
         #index_fixed = self.prog_index % len(self.sorted_progs)
         #prgname = self.sorted_progs[index_fixed]
         self.prog = self.synth.programbus.uiprogram
         if self.prog!=None:
           note = self.base_notes[KC] + (self.octave*12)
           mods = self.genMods()
           voice = self.synth.keyOn(note,127,self.prog,mods)
           self.onNote(voice)
           self.voices[KC] = voice
         return res
      else:
        if KC == ord("["): # decr gain
          if uievent.shift:
            self.gain -= 6.0
            self.synth.masterGain = singularity.decibelsToLinear(self.gain)
          else:
            self.synth.programbus.gain = self.synth.programbus.gain - 3.0
          return res
        if KC == ord("]"): # incr gain
          if uievent.shift:
            self.gain += 6.0
            self.synth.masterGain = singularity.decibelsToLinear(self.gain)
          else:
            self.synth.programbus.gain = self.synth.programbus.gain + 3.0
          return res
        elif KC == ord("-"): # next effect
          if uievent.shift:
            self.synth.system_tempo -= 1
            self.curseq.timebase.tempo = self.synth.system_tempo
          else:
            self.synth.prevEffect(self.synth.programbus)
          return res
        elif KC == ord("="): # next effect
          if uievent.shift:
            self.synth.system_tempo += 1
            self.curseq.timebase.tempo = self.synth.system_tempo
          else:
            self.synth.nextEffect(self.synth.programbus)
          return res
        elif KC == ord("!"): # panic
          for voice in self.voices:
            self.synth.keyOff(voice)
            self.voices.clear()
          return res
        elif KC == ord("Z"): # 
          self.octave -= 1
          if self.octave < 0:
            self.octave = 0
          return res
        elif KC == ord("X"): # 
          self.octave += 1
          if self.octave > 8:
            self.octave = 8
          return res
        ######################################
        # NEW sequence
        ######################################
        elif KC == ord("N"): # 
          if uievent.shift:
            if self.click_prog != None:
              self.synth.panic()
              self.curseq = singularity.Sequence("NewSequence")
              self.curseq.timebase.numerator = 4
              self.curseq.timebase.denominator = 4
              self.curseq.timebase.tempo = 120.0
              self.curseq.timebase.ppq = 256
              self.curseq.timebase.measureMax = 4
              self.clicktrack = self.curseq.createTrack("click")
              self.clicktrack.program = self.click_prog
              clip = self.clicktrack.createClickClip("click")
              clip.noteL = self.click_noteL
              clip.noteH = self.click_noteH
              clip.velL = 64
              clip.velH = 128
              self.clicktrack.outputbus = self.auxbusses[0]
              self.clicktrack.outputbus.uiprogram = self.click_prog
              #####################################
              self.rec_trackclips = {}
              trc = self.createTrackAndClipForBus(self.mainbus)
              self.assignTRC(trc)
              for i in range(0,self.numaux):
                trc = self.createTrackAndClipForBus(self.auxbusses[i])
              #####################################
              self.synth.resetTimer()
              self.playback = self.sequencer.clearPlaybacks()
              self.playback = self.sequencer.playSequence(self.curseq,0.0)
          return res
        ######################################
        # clear track events
        ######################################
        elif KC == ord("M"): # 
          seqr = self.sequencer
          if uievent.shift:
            if self.click_prog != None:
              # clear recording clip
              if seqr.recording_clip != None:
                seqr.recording_clip.clear()
          else:
            trc = self.TRC
            if seqr.recording_track==None:
              seqr.recording_track = trc.track
              seqr.recording_clip = trc.clip
            else:
              seqr.recording_track = None
              seqr.recording_clip = None
            pass
        elif KC == ord("Q"): # 
          seqr = self.sequencer
          if uievent.shift:
            if seqr.recording_clip != None:
              if self.curseq != None:
                PPQ = self.curseq.timebase.ppq
                seqr.recording_clip.quantize(int(PPQ/4)) # 
                self.synth.resetTimer()
                self.playback = self.sequencer.clearPlaybacks()
                self.playback = self.sequencer.playSequence(self.curseq,0.0)

  
    elif uievent.code == tokens.KEY_UP.hashed:
      KC = uievent.keycode
      if KC in self.base_notes:
        note = self.base_notes[KC] + (self.octave*12)
        if KC in self.voices:
          voice = self.voices[KC]
          self.synth.keyOff(voice,note,0)
          del self.voices[KC]
          return res

    return ui.HandlerResult()


###############################################################################

