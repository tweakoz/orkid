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
sys.path.append((thisdir()).normalized.as_string) # add parent dir to path
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
from singularity._harness import SingulTestApp, find_index
from _t8x import makeT8XLayer
################################################################################

class HybridApp(SingulTestApp):

  def __init__(self):
    super().__init__()
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    self.synth.setEffect(self.mainbus,"Reverb:GuyWire")
    #self.synth.setEffect(self.mainbus,"none")
    self.mainbus.gain = 72
    ############################
    # create a new hybrid patch
    #  mixing different synth architectures
    ############################
    self.new_soundbank = singularity.BankData()
    ############################
    newprog = self.new_soundbank.newProgram("T8X")
    newprog.monophonic = True
    newprog.portamentoRate = 1200 # cents per second
    ############################

    for index in range(0,8):
      fi = index/7.0
      makeT8XLayer(newprog,fi,11.99)
      makeT8XLayer(newprog,fi,0)
      makeT8XLayer(newprog,fi,-12.01)
      makeT8XLayer(newprog,fi,-24.01)
      #makeT8XLayer(fi,-36.01)
    #assert(False)
    ############################
    self.soundbank = self.new_soundbank
    ############################
    ok_list = [
      "T8X"
    ]
    ############################
    self.sorted_progs = sorted(ok_list)
    self.prog_index = find_index(self.sorted_progs, "T8X")
    self.synth.programbus.uiprogram = newprog
    print(self.prog_index)
    if self.pgmview:
      self.pgmview.setProgram(newprog)
    self.octave = 5
  ##############################################

###############################################################################

app = HybridApp()
app.ezapp.mainThreadLoop()
