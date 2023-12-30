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
    self.ezapp = OrkEzApp.create(self,left=420, top=100, height=720,width=480)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    lg_group = self.ezapp.topLayoutGroup
    root_layout = lg_group.layout
    self.time = 0.0
    self.octave = 5
    self.charts = {}
    self.chart_events = {}

  ##############################################

  def onGpuInit(self,ctx):
    self.context = ctx
    self.audiodevice = singularity.device.instance()
    self.synth = singularity.synth.instance()
    self.mainbus = self.synth.outputBus("main")
    for i in range(6):
      self.synth.nextEffect()
    self.synth.masterGain = singularity.decibelsToLinear(-12.0)
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
        ord("]"): 18,
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

  def onUiEvent(self,uievent):
    if uievent.code == tokens.KEY_REPEAT.hashed or uievent.code==tokens.KEY_DOWN.hashed:
      KC = uievent.keycode
      if KC == ord(","): # prev program
        self.prog_index -= 1
        if self.prog_index < 0:
          self.prog_index = len(self.sorted_progs)-1
        prgname = self.sorted_progs[self.prog_index]
        print("prgname<%s>" % prgname)
        self.prog = self.soundbank.programByName(prgname)
        print("prgname<%s> %s" % (prgname, self.prog.name))
        return True
      elif KC == ord("."): # next program
        self.prog_index += 1
        if self.prog_index >= len(self.sorted_progs):
          self.prog_index = 0
        prgname = self.sorted_progs[self.prog_index]
        self.prog = self.soundbank.programByName(prgname)
        print("prgname<%s> %s" % (prgname, self.prog.name))
        return True
    if uievent.code == tokens.KEY_DOWN.hashed:
      KC = uievent.keycode
      if KC in self.base_notes:
       if KC not in self.voices:
         index_fixed = self.prog_index % len(self.sorted_progs)
         prgname = self.sorted_progs[index_fixed]
         self.prog = self.soundbank.programByName(prgname)
         note = self.base_notes[KC] + (self.octave*12)
         mods = self.genMods()
         voice = self.synth.keyOn(note,127,self.prog,mods)
         self.onNote(voice)
         self.voices[KC] = voice
         return True
      else:
        if KC == ord("1"): # next effect
          self.synth.masterGain = singularity.decibelsToLinear(-24.0)
          return True
        elif KC == ord("2"): # next effect
          self.synth.masterGain = singularity.decibelsToLinear(-18.0)
          return True
        elif KC == ord("3"): # next effect
          self.synth.masterGain = singularity.decibelsToLinear(-12.0)
          return True
        elif KC == ord("4"): # next effect
          self.synth.masterGain = singularity.decibelsToLinear(-6.0)
          return True
        elif KC == ord("5"): # next effect
          self.synth.masterGain = singularity.decibelsToLinear(0)
          return True
        elif KC == ord("-"): # next effect
          self.synth.prevEffect()
          return True
        elif KC == ord("="): # next effect
          self.synth.nextEffect()
          return True
        elif KC == ord("!"): # panic
          for voice in self.voices:
            self.synth.keyOff(voice)
            self.voices.clear()
          return True
        elif KC == ord("4"): # 
          print(self.sorted_progs)
          return True
        elif KC == ord("Z"): # 
          self.octave -= 1
          if self.octave < 0:
            self.octave = 0
          return True
        elif KC == ord("X"): # 
          self.octave += 1
          if self.octave > 8:
            self.octave = 8
          return True
        elif KC == ord("N"): # new chart 
          self.clearChart()
          return True
        elif KC == ord("M"): # show chart
          self.showCharts()
          return True

  
    elif uievent.code == tokens.KEY_UP.hashed:
      KC = uievent.keycode
      if KC in self.voices:
        voice = self.voices[KC]
        self.synth.keyOff(voice)
        del self.voices[KC]
        return True

    return False


###############################################################################

