#!/usr/bin/env ork.python

################################################################################
# singularity test for editing layers in a soundbank
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import numpy as np
import sys, random
from orkengine.core import *
from orkengine.lev2 import *
from orkengine.lev2 import singularity as S
from _sampler import createLayer, createSampleLayer

################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
from singularity._harness import SingulTestApp, find_index
tokens = CrcStringProxy()
################################################################################

class HwInputApp(SingulTestApp):

  def __init__(self):
    super().__init__(enable_input=True)
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    prgname = "hwinput"
    self.mainbus.gain = 18
    self.octave = 4
    ############################
    # create a new hybrid patch
    #  mixing different synth architectures
    ############################
    self.new_soundbank = singularity.BankData()
    ############################
    newprog = self.new_soundbank.newProgram(prgname)
    ############################
    newlyr = newprog.newLayer()
    dspstg = newlyr.appendStage("DSP")
    ampstg = newlyr.appendStage("AMP")
    dspstg.ioconfig.inputs = [0,1]
    dspstg.ioconfig.outputs = [0,1]
    ampstg.ioconfig.inputs = [0,1]
    ampstg.ioconfig.outputs = [0,2]
    pchblock = dspstg.appendDspBlock("Pitch","pitch")
    newlyr.pitchBlock = pchblock
    newlyr.panmode = 0
    newlyr.pan = 0
    ampblock = ampstg.appendDspBlock("AmpAdaptive","amp")
    hwinput = dspstg.appendDspBlock("HwInput","hwi")
    noisegate = dspstg.appendDspBlock("AmpNoiseGate","ng")
    noisegate.threshold = 0.003
    noisegate.input_gain = 50.0
    noisegate.output_gain = 5 / noisegate.input_gain
    noisegate.attack = 0.03
    noisegate.release = 0.001
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

HwInputApp().ezapp.mainThreadLoop()
