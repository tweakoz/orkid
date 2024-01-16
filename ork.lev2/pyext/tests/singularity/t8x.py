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
    def makeT8XLayer(fi,offset):
      fibi = (fi-0.5)*2.0
      newlyr = newprog.newLayer()
      dspstg = newlyr.appendStage("DSP")
      ampstg = newlyr.appendStage("AMP")
      dspstg.ioconfig.inputs = [0]
      dspstg.ioconfig.outputs = [0]
      ampstg.ioconfig.inputs = [0]
      ampstg.ioconfig.outputs = [0]
      pchblock = dspstg.appendDspBlock("Pitch","pitch")
      sawblock = dspstg.appendDspBlock("OscilSaw","saw1")
      panblock = dspstg.appendDspBlock("AmpPanner","pan")
      ampblock = ampstg.appendDspBlock("AmpAdaptive","amp")
      #
      panblock.paramByName("POS").coarse=fibi
      #
      ampenv = newlyr.appendController("RateLevelEnv", "AMPENV")
      ampenv.ampenv = True
      ampenv.bipolar = False
      ampenv.sustainSegment = 0
      ampenv.addSegment("seg0", 0.5, 1,0.4)
      ampenv.addSegment("seg1", 0.5, .5,4)
      ampenv.addSegment("seg2", 0.5, 0,2.0)
      #
      ampblock.paramByName("gain").mods.src1 = ampenv
      ampblock.paramByName("gain").mods.src1depth = 1.0
      #
      r = random.randrange(-4,4)
      pchenv = newlyr.appendController("RateLevelEnv", "PITCHENV")
      pchenv.ampenv = False
      pchenv.bipolar = True
      pchenv.releaseSegment=1
      pchenv.addSegment("seg0", 0.1, 0,2)
      pchenv.addSegment("seg1", 16+r, 1,0.5)
      pchenv.addSegment("seg2", 10, 1,0.5)
      #
      coarse = offset
      fine = fi*7200 # -4500 ..4500
      finehz = 0
      r = random.randrange(0,3)
      newlyr.gain = -30
      if r>0:
        coarse = -12
      if r>1:
        coarse = +12
      print(coarse)
      newlyr.gain -= (60+(coarse*0.5))
      #
      sawblock.paramByName("pitch").coarse = coarse
      sawblock.paramByName("pitch").fine = fine
      sawblock.paramByName("pitch").fineHZ = finehz
      sawblock.paramByName("pitch").mods.src1 = pchenv
      sawblock.paramByName("pitch").mods.src1depth = -fine*0.997

      return newlyr
    for index in range(0,8):
      fi = index/7.0
      makeT8XLayer(fi,11.99)
      makeT8XLayer(fi,0)
      makeT8XLayer(fi,-12.01)
      makeT8XLayer(fi,-24.01)
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
