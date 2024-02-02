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
from orkengine.lev2 import singularity as S

################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
from singularity._harness import SingulTestApp, find_index
################################################################################

class WaveformsApp(SingulTestApp):

  def __init__(self):
    super().__init__()
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    prgname = "waveforms"
    self.mainbus.gain = 0
    self.octave = 4
    ############################
    # create a new hybrid patch
    #  mixing different synth architectures
    ############################
    self.new_soundbank = singularity.BankData()
    ############################
    newprog = self.new_soundbank.newProgram(prgname)
    newprog.monophonic = True
    newprog.portamentoRate = 60000 # cents per second
    ############################
    newlyr = newprog.newLayer()
    dspstg = newlyr.appendStage("DSP")
    ampstg = newlyr.appendStage("AMP")
    dspstg.ioconfig.inputs = []
    dspstg.ioconfig.outputs = [0,1]
    ampstg.ioconfig.inputs = [0,1]
    ampstg.ioconfig.outputs = [0,1]
    pchblock = dspstg.appendDspBlock("Pitch","pitch")
    #########################################
    # carrier
    #########################################
    ampenv = newlyr.appendController("RateLevelEnv", "AMPENV")
    ampenv.ampenv = True
    ampenv.bipolar = False
    ampenv.sustainSegment = 0
    ampenv.addSegment("seg0", 0, 1,0.4)
    ampenv.addSegment("seg1", 0.2, .5,4)
    ampenv.addSegment("seg2", 0.2, 0,2.0)
    #
    #########################################
    # post amp
    #########################################
    #
    ampblock = ampstg.appendDspBlock("AmpAdaptive","amp")
    ampblock.paramByName("gain").mods.src1 = ampenv
    ampblock.paramByName("gain").mods.src1scale = 1.0
    #return newlyr
    sampleC4 = S.SampleData(
      name = "C4",
      waveform = [
        0.0, 0.25,
        0.5, 1.0
      ]
    )
    multisample = S.MultiSampleData("MSAMPLE",[sampleC4])
    keymap = S.KeyMapData("KMAP")
    R0 = keymap.addRegion(
      lokey=48,
      hikey=48,
      lovel=0,
      hivel=127,
      multisample=multisample,
      sample=sampleC4)
    ############################
    self.soundbank = self.new_soundbank
    ############################
    ok_list = [
      prgname
    ]
    ############################
    self.sorted_progs = sorted(ok_list)
    self.prog_index = find_index(self.sorted_progs, prgname)
    self.synth.programbus.uiprogram = newprog
    print(self.prog_index)
    if self.pgmview:
      self.pgmview.setProgram(newprog)
  ##############################################

###############################################################################

WaveformsApp().ezapp.mainThreadLoop()
