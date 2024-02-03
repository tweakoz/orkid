#!/usr/bin/env python3

################################################################################
# singularity test for editing layers in a soundbank
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, random
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
    self.synth.setEffect(self.mainbus,"Reverb:FDNX")
    self.mainbus.gain = 24
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
    self.soundbank = self.new_soundbank
    ############################
    ok_list = [
      "HYBRID1",
    ]
    ############################
    self.sorted_progs = sorted(ok_list)
    self.prog_index = find_index(self.sorted_progs, "HYBRID1")
    self.synth.programbus.uiprogram = newprog
    if self.pgmview:
      self.pgmview.setProgram(newprog)
    print(self.prog_index)
    self.octave = 4
  ##############################################

###############################################################################

app = HybridApp()
app.ezapp.mainThreadLoop()
