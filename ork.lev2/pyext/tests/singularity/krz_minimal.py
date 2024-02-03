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
from _sampler import createSampleLayer

################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
from singularity._harness import SingulTestApp, find_index
################################################################################

class KrzApp(SingulTestApp):

  def __init__(self):
    super().__init__()
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)

    main = self.synth.outputBus("main")
    aux8 = self.synth.outputBus("aux8")

    self.syn_data_base = singularity.baseDataPath()/"kurzweil"
    self.krzdata = singularity.KrzSynthData(base_objects=False)

    def do_bank(name,filename,remapbase=0):
      self.krzdata.loadBank( name=name, 
                             remap_base=remapbase,
                             path=self.syn_data_base/filename)
          
    do_bank( "quartet.krz", "quartet.krz")
    do_bank( "monopoly.krz", "monopoly.krz")
    do_bank( "rapkit.krz", "rapkit.krz")
    do_bank( "choicdrm.krz", "choicdrm.krz")

    do_bank( "alesisdr", "alesisdr.krz" )
    do_bank( "m1drums", "m1drums.krz")
    do_bank( "emusp12", "emusp12.krz")
    do_bank( "monksvox.kr1", "monksvox.kr1.krz")
    do_bank( "monksvox.kr2", "monksvox.kr2.krz")
    do_bank( "epsstrng.krz", "epsstrng.krz")
    #do_bank( "boychoir.krz", "boychoir.krz")
    do_bank( "lostring.krz", "lostring.krz")
    #do_bank( "seavoice.krz", "seavoice.krz")
    do_bank( "rain_1.krz", "rain_1.krz")
    do_bank( "moogbass.krz", "moogbass.krz")
    do_bank( "steinway.krz", "steinway.krz")
    do_bank( "trumpets.krz", "trumpets.krz")
    do_bank( "bothfrhn.krz", "bothfrhn.krz")
    do_bank( "b3.krz", "b3.krz")
    do_bank( "pipeorgn.krz", "pipeorgn.krz")
    do_bank( "harpsi.krz", "harpsi.krz")
    do_bank( "acguit.krz", "acguit.krz")
    do_bank( "classgtr.krz", "classgtr.krz")
    do_bank( "guitloop.krz", "guitloop.krz")
    do_bank( "sledge.krz", "sledge.krz")
    do_bank( "flute.krz", "flute.krz")
    do_bank( "indiperc.krz", "indiperc.krz")
    do_bank( "african.krz", "african.krz")
    do_bank( "gong.krz", "gong.krz")
    do_bank( "idstperc.krz", "idstperc.krz")
    
    
    self.soundbank = self.krzdata.bankData
    self.krzprogs = self.soundbank.programsByName
    self.krzsamps = self.soundbank.multiSamplesByName
    self.krzkmaps = self.soundbank.keymapsByName
    for key in self.krzprogs:
      print("krzprog<%s>" % key)
    
    for key in self.krzsamps:
      msample = self.krzsamps[key]
      print("krzsamp<%s> nums<%d>" % (key,msample.numSamples))
    
    for key in self.krzkmaps:
      print("krzkmap<%s>" % key)

    #############################
    # create TOZDRUMS Program
    #############################
    layers = [
      { "name":"Kick1", "lokey":0, "hikey":60, "tuning":0, "lowpass":12000.0, "pan": 5 },
      { "name":"UNNAMED_WS_206", "lokey":61, "hikey":61, "tuning":-2000, "lowpass":18000.0, "pan": 6 },
      { "name":"Kick2", "lokey":62, "hikey":62, "tuning":0, "lowpass":12000.0, "pan": 4 },
      { "name":"UNNAMED_WS_207", "lokey":63, "hikey":63, "tuning":-2000, "lowpass":12000.0, "pan": 4 },
      { "name":"Snare1", "lokey":64, "hikey":64, "tuning":-1200, "lowpass":12000.0, "pan": 8 },
      { "name":"UNNAMED_WS", "lokey":65, "hikey":65, "tuning":-1800, "lowpass":12000.0, "pan": 9 },
      { "name":"UNNAMED_WS_201", "lokey":66, "hikey":66, "tuning":-2400, "lowpass":18000.0, "pan": 9 },
      { "name":"Snare2", "lokey":67, "hikey":67, "tuning":-1200, "lowpass":12000.0, "pan": 10 },
      { "name":"Closed_HiHat", "lokey":68, "hikey":69, "tuning":-1200, "lowpass":18000.0, "pan": 7 },
      { "name":"Open_HiHat", "lokey":70, "hikey":71, "tuning":-1200, "lowpass":18000.0, "pan": 7 },
      { "name":"Crash", "lokey":72, "hikey":72, "tuning":-1800, "lowpass":18000.0, "pan": 7 },
      { "name":"UNNAMED_WS_205", "lokey":73, "hikey":73, "tuning":-2400, "lowpass":12000.0, "pan": 7 },
      { "name":"clank", "lokey":74, "hikey":74, "tuning":-2400, "lowpass":12000.0, "pan": 7 },
      { "name":"electro", "lokey":75, "hikey":75, "tuning":-1200, "lowpass":12000.0, "pan": 7 },
      { "name":"tom", "lokey":76, "hikey":76, "tuning":-1200, "lowpass":12000.0, "pan": 7 },
      { "name":"tom2", "lokey":77, "hikey":77, "tuning":-1200, "lowpass":12000.0, "pan": 7 },
    ]
    DRUM_PRG = "TOZDRUMS"
    newprog = self.soundbank.newProgram(DRUM_PRG)
    for item in layers:
      n = item["name"]
      lo = item["lokey"]
      hi = item["hikey"]
      t = item["tuning"]
      l = item["lowpass"]
      createSampleLayer(
        newprog,
        multisample=self.krzsamps[n],
        lokey=lo,
        hikey=hi,
        lowpass=l,
        tuning=t)
    #############################
    # create MONKSVOX Program
    #############################
    newprog2 = self.soundbank.newProgram("TOZMONKS")
    createSampleLayer(
        newprog2,
        multisample=self.krzsamps["Tibetan_Monks"],
        lokey=0,
        hikey=72,
        lowpass=10000,
        tuning=0)
    #############################
    self.sorted_progs = []
    for item in self.krzprogs:
      self.sorted_progs.append(item)
    print(self.sorted_progs)
    #############################
    self.setBusProgram(main,self.soundbank.programByName(DRUM_PRG))
    self.prog_index = find_index(self.sorted_progs, DRUM_PRG)
    self.prog = self.soundbank.programByName(DRUM_PRG)
    self.setUiProgram(self.prog)
    self.synth.setEffect(main,"Reverb:TEST")
    self.synth.setEffect(main,"none")

    
    self.synth.masterGain = singularity.decibelsToLinear(-24.0)

###############################################################################

app = KrzApp()
app.ezapp.mainThreadLoop()
