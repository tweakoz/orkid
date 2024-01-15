#!/usr/bin/env python3

################################################################################
# singularity test for editing layers in a soundbank
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
from singularity._harness import SingulTestApp, find_index
################################################################################

class HybridApp(SingulTestApp):

  def __init__(self):
    super().__init__()
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    ############################
    # load patches
    ############################
    self.syn_data_base = singularity.baseDataPath()/"casioCZ"
    self.cz1data = singularity.CzSynthData()
    self.cz1data.loadBank("banka", self.syn_data_base/"cz1_2.bnk")
    self.cz1data.loadBank("bankb", self.syn_data_base/"cz1_4.bnk")
    self.krzdata = singularity.KrzSynthData()
    krz_bank = self.krzdata.bankData
    cz1_bank = self.cz1data.bankData
    ############################
    # create a new hybrid patch
    #  mixing different synth architectures
    ############################
    self.new_soundbank = singularity.BankData()
    newprog = self.new_soundbank.newProgram("HYBRID1")
    newprog.merge(krz_bank.programByName("Waterflute"))
    newprog.merge(cz1_bank.programByName("Bells and Chimes"))
    newprog.merge(cz1_bank.programByName("Delayed Pad"))
    ############################
    if True:
      def override(lid,bus,gain,coarse,kt):
         L = newprog.layer(lid)
         L.gain = gain
         L.outputBus = bus
         PB = L.pitchBlock
         if PB != None:
           P = PB.paramByName("pitch")
           P.coarse=coarse
           P.keyTrack=kt
           P.evaluatorID = "pitch"
      override(0,"aux1",-18,24,0)
      override(1,"aux4",-18,12,0)
      override(2,"aux2",0,-12,0)
      override(3,"aux4",18,0,0)
    ############################
    newprog = self.new_soundbank.newProgram("FromScratch")
    newlyr = newprog.newLayer()
    newlyr.gain = 0.0
    dspstg = newlyr.appendStage("DSP")
    ampstg = newlyr.appendStage("AMP")
    dspstg.ioconfig.inputs = [0]
    dspstg.ioconfig.outputs = [0]
    ampstg.ioconfig.inputs = [0]
    ampstg.ioconfig.outputs = [0]
    pchblock = dspstg.appendDspBlock("Pitch","pitch")
    pmxblock = dspstg.appendDspBlock("OscilSine","sin")
    ampblock = ampstg.appendDspBlock("AmpMono","amp")
    env = newlyr.appendController("RateLevelEnv", "AMPENV")
    env.ampenv = True
    env.bipolar = False
    env.addSegment("seg0", .2, .7,1)
    env.addSegment("seg1", .2, .7,1)
    env.addSegment("seg2", 1, 1,1)
    env.addSegment("seg3", 120, .3,1)
    env.addSegment("seg4", 120, 0,1)
    ampblock.paramByName("gain").mods.src1 = env
    ampblock.paramByName("gain").mods.src1depth = 1.0
    print(env)
    #pmxblock.properties.Waveform = 3
    #pmxblock.properties.InputChannel = 2
    print(pmxblock)
    print(pmxblock.params)
    print(pmxblock.properties.dict)
    #assert(False)
    ############################
    self.soundbank = self.new_soundbank
    ############################
    ok_list = [
      "HYBRID1",
      "FromScratch"
    ]
    ############################
    self.sorted_progs = sorted(ok_list)
    self.prog_index = find_index(self.sorted_progs, "HYBRID1")
    self.octave = 3
  ##############################################

###############################################################################

app = HybridApp()
app.ezapp.mainThreadLoop()
