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
from singularity._harness import SingulTestApp, find_index
################################################################################

class HybridApp(SingulTestApp):

  def __init__(self):
    super().__init__()
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    self.syn_data_base = singularity.baseDataPath()/"casioCZ"
    self.cz1data = singularity.CzSynthData()
    self.cz1data.loadBank("bank0", self.syn_data_base/"cz1_1.bnk")
    self.cz1data.loadBank("bank1", self.syn_data_base/"cz1_2.bnk")
    self.cz1data.loadBank("bank2", self.syn_data_base/"cz1_3.bnk")
    self.cz1data.loadBank("bank3", self.syn_data_base/"cz1_4.bnk")
    self.krzdata = singularity.KrzSynthData()
    krz_bank = self.krzdata.bankData
    cz1_bank = self.cz1data.bankData
    ############################
    self.new_soundbank = singularity.BankData()
    newprog = self.new_soundbank.newProgram("_HYBRID")
    newprog.merge(krz_bank.programByName("Waterflute"))
    #newprog.merge(cz1_bank.programByName("Bells and Chimes"))
    #newprog.merge(cz1_bank.programByName("Delayed Pad"))
    ####
    L0 = newprog.cloneLayer(0)
    L0.gain = -18 # dB
    L0.pitchBlock.paramByName("pitch").coarse=36
    L0.pitchBlock.paramByName("pitch").keyTrack=0
    ####
    L1 = newprog.cloneLayer(1)
    L1.gain = -18 # dB
    L1.pitchBlock.paramByName("pitch").coarse=36
    L1.pitchBlock.paramByName("pitch").keyTrack=0
    ####
    #assert(False)
    ############################
    self.soundbank = self.new_soundbank
    ############################
    ok_list = [
      "HYBRID1",
    ]
    ############################
    self.sorted_progs = sorted(ok_list)
    self.prog_index = find_index(self.sorted_progs, "HYBRID1")

  ##############################################

###############################################################################

app = HybridApp()
app.ezapp.mainThreadLoop()
