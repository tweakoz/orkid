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
    lg_group.margin = 4

    self.griditems = lg_group.makeGrid(
      width = 2,
      height = 2,
      margin = 4,
      uiclass = ui.LambdaBox,
      args = ["label",vec4(1,0,1,1)],
    )

    for g in self.griditems:
      g.widget.onPressed(lambda: print("GRIDITEM0 PUSHED"))

    item = lg_group.makeEvTestBox( w=100, #
                                    h=100, #
                                    x=100, #
                                    y=100, #
                                    color_normal=vec4(0.75,0.75,0.75,0.5), #
                                    color_click=vec4(0.5,0.0,0.0,0.5), #
                                    color_doubleclick=vec4(0.5,1.0,0.5,0.5), #
                                    color_drag=vec4(0.5,0.5,1.0,0.5), #
                                    name="testbox1")

    lg_group.replaceChild(self.griditems[1].layout,item)

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
    for i in range(6):
      self.synth.nextEffect()
    self.gain = -12.0
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
    if self.prog:
      LD = self.prog.layer(self.layerID)
      if LD:
        DST = LD.stage("DSP")
        DST.dump()
        AST = LD.stage("AMP")
        AST.dump()
    print("###########################################")
    pass

  def onUiEvent(self,uievent):
    res = ui.HandlerResult()
    res.setHandler( self.ezapp.topWidget )
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
        return res
      elif KC == ord("."): # next program
        self.prog_index += 1
        if self.prog_index >= len(self.sorted_progs):
          self.prog_index = 0
        prgname = self.sorted_progs[self.prog_index]
        self.prog = self.soundbank.programByName(prgname)
        print("prgname<%s> %s" % (prgname, self.prog.name))
        return res
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
        elif KC == ord("1"): # toggle 1 bit in layermask
          self.layermask = self.layermask ^ (1<<0)
          self.layerID = 0
          self.prLayerMask()
          return res
        elif KC == ord("2"): # toggle 1 bit in layermask
          self.layermask = self.layermask ^ (1<<1)
          self.layerID = 1
          self.prLayerMask()
          return res
        elif KC == ord("3"): # toggle 1 bit in layermask
          self.layermask = self.layermask ^ (1<<2)
          self.layerID = 2
          self.prLayerMask()
          return res
        elif KC == ord("4"): # toggle 1 bit in layermask
          self.layermask = self.layermask ^ (1<<3)
          self.layerID = 3
          self.prLayerMask()
          return res
        elif KC == ord("-"): # next effect
          self.synth.prevEffect()
          return res
        elif KC == ord("="): # next effect
          self.synth.nextEffect()
          return res
        elif KC == ord("!"): # panic
          for voice in self.voices:
            self.synth.keyOff(voice)
            self.voices.clear()
          return res
        elif KC == ord("4"): # 
          print(self.sorted_progs)
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
        elif KC == ord("N"): # new chart 
          self.clearChart()
          return res
        elif KC == ord("M"): # show chart
          self.showCharts()
          return res

  
    elif uievent.code == tokens.KEY_UP.hashed:
      KC = uievent.keycode
      if KC in self.voices:
        voice = self.voices[KC]
        self.synth.keyOff(voice)
        del self.voices[KC]
        return res

    return ui.HandlerResult()


###############################################################################

