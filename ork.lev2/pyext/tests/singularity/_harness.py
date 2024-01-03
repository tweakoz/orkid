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
################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
################################################################################

def find_index(sorted_list, value):
    index = bisect.bisect_left(sorted_list, value)
    if index != len(sorted_list) and sorted_list[index] == value:
        return index
    return -1  # Return -1 or some other value to indicate "not found"

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

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):
    self.context = ctx
    self.audiodevice = singularity.device.instance()
    self.synth = singularity.synth.instance()

    self.mainbus = self.synth.outputBus("main")
    self.mainbus_source = self.mainbus.createScopeSource()
    self.synth.setEffect(self.mainbus,"Reverb:OilTank")
    self.numaux = 8
    self.auxbusses = []
    self.auxbus_sources = []
    for i in range(0,self.numaux):
      self.auxbusses += [self.synth.createOutputBus("aux%d" % (i+1))]
      self.auxbus_sources += [self.auxbusses[i].createScopeSource()]
      self.synth.setEffect(self.auxbusses[i],"none")

    lg_group = self.ezapp.topLayoutGroup

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

  def onUpdate(self,updinfo):
    self.time = updinfo.absolutetime

  def onGpuUpdate(self,ctx):
    pass

  def showCharts(self):
    pass

  def clearChart(self):
    self.charts = dict()
    self.chart_events = dict()

  def genMods(self):
    return None
  
  def onNote(self,voice):
    pass

  def prLayerMask(self):
    as_bools = binary_str = format(self.layermask, '032b')[::-1]
    print("###########################################")
    print("layerID<%d> layermask<%s> " % (self.layerID,as_bools))
    print("###########################################")
    pass
  def setUiProgram(self,prg):
    self.synth.programbus.uiprogram = prg
    if self.pgmview:
      self.pgmview.setProgram(prg)
  def onUiEvent(self,uievent):
    res = ui.HandlerResult()
    res.setHandler( self.ezapp.topWidget )
    if uievent.code == tokens.KEY_REPEAT.hashed or uievent.code==tokens.KEY_DOWN.hashed:
      KC = uievent.keycode
      def _setSource(bus,source):
        self.program_source.disconnect(self.oscope_sink)
        self.program_source.disconnect(self.spectra_sink)
        self.synth.programbus = bus
        self.program_source = source
        source.connect(self.oscope_sink)
        source.connect(self.spectra_sink)
        self.prog = bus.uiprogram
        self.setUiProgram(self.prog)
      if KC == ord("0"): # solo layer off
        if uievent.shift:
          self.synth.soloLayer = -1
        else:
          _setSource(self.mainbus,self.mainbus_source)
      elif KC == ord("1"): # solo layer 1
        if uievent.shift:
          self.synth.soloLayer = 0
        else:
          _setSource(self.auxbusses[0],self.auxbus_sources[0])
      elif KC == ord("2"): # solo layer 2
        if uievent.shift:
          self.synth.soloLayer = 1
        else:
          _setSource(self.auxbusses[1],self.auxbus_sources[1])
      elif KC == ord("3"): # solo layer 3
        if uievent.shift:
          self.synth.soloLayer = 2
        else:
          _setSource(self.auxbusses[2],self.auxbus_sources[2])
      elif KC == ord("4"): # solo layer 4
        if uievent.shift:
          self.synth.soloLayer = 3
        else:
          _setSource(self.auxbusses[3],self.auxbus_sources[3])
      elif KC == ord("5"): # solo layer 5
        if uievent.shift:
          self.synth.soloLayer = 4
        else:
          _setSource(self.auxbusses[4],self.auxbus_sources[4])
      elif KC == ord("6"): # solo layer 6
        if uievent.shift:
          self.synth.soloLayer = 5
        else:
          _setSource(self.auxbusses[5],self.auxbus_sources[5])
      elif KC == ord("7"): # solo layer 7
        if uievent.shift:
          self.synth.soloLayer = 6
        else:
          _setSource(self.auxbusses[6],self.auxbus_sources[6])
      elif KC == ord("8"): # solo layer 8
        if uievent.shift:
          self.synth.soloLayer = 7
        else:
          _setSource(self.auxbusses[7],self.auxbus_sources[7])
      elif KC == ord(" "): # hold drones
        for KC in self.voices:
          voice = self.voices[KC]
          self.held_voices += [voice]
        self.voices = dict()
      elif KC == ord("C"): # release drones
        for v in self.held_voices:
          self.synth.keyOff(v)
        self.held_voices = []
      elif KC == ord(","): # prev program
        self.prog_index -= 1
        if self.prog_index < 0:
          self.prog_index = len(self.sorted_progs)-1
        prgname = self.sorted_progs[self.prog_index]
        self.prog = self.soundbank.programByName(prgname)
        self.synth.programbus.uiprogram = self.prog
        if self.pgmview:
          self.pgmview.setProgram(self.prog)
        return res
      elif KC == ord("."): # next program
        self.prog_index += 1
        if self.prog_index >= len(self.sorted_progs):
          self.prog_index = 0
        prgname = self.sorted_progs[self.prog_index]
        self.prog = self.soundbank.programByName(prgname)
        self.synth.programbus.uiprogram = self.prog
        if self.pgmview:
          self.pgmview.setProgram(self.prog)
        return res
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
          self.gain -= 6.0
          self.synth.masterGain = singularity.decibelsToLinear(self.gain)
          return res
        if KC == ord("]"): # incr gain
          self.gain += 6.0
          self.synth.masterGain = singularity.decibelsToLinear(self.gain)
          return res
        elif KC == ord("-"): # next effect
          self.synth.prevEffect(self.synth.programbus)
          return res
        elif KC == ord("="): # next effect
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
        #elif KC == ord("N"): # new chart 
        #  self.clearChart()
        #  return res
        #elif KC == ord("M"): # show chart
        #  self.showCharts()
        #  return res

  
    elif uievent.code == tokens.KEY_UP.hashed:
      KC = uievent.keycode
      if KC in self.voices:
        voice = self.voices[KC]
        self.synth.keyOff(voice)
        del self.voices[KC]
        return res

    return ui.HandlerResult()


###############################################################################

