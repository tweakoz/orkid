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
    newprog = self.new_soundbank.newProgram("_HYBRID")
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
    newlyr = newprog.newLayer()
    newstg = newlyr.appendStage("TESTX")
    ioc = newstg.ioconfig
    ioc.inputs = [0]
    ioc.outputs = [0]
    ampblock = newstg.appendDspBlock("AmpAdaptive","amp1")
    print(ampblock)
    print(ampblock.params)
    print(ampblock.properties)
    assert(False)
    ############################
    self.soundbank = self.new_soundbank
    ############################
    # hook up aux4 bus to oscope and spectra
    ############################
    #self.aux4_source = self.aux4bus.createScopeSource()
    #self.mainbus_source.disconnect(self.oscope_sink)
    #self.mainbus_source.disconnect(self.spectra_sink)
    #self.aux4_source.connect(self.oscope_sink)
    #self.aux4_source.connect(self.spectra_sink)
    ############################
    ok_list = [
      "HYBRID1",
    ]
    ############################
    self.sorted_progs = sorted(ok_list)
    self.prog_index = find_index(self.sorted_progs, "HYBRID1")
    self.octave = 3
  ##############################################

###############################################################################

app = HybridApp()
app.ezapp.mainThreadLoop()
