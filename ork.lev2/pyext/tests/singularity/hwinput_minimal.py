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
from ork.singularity.sampler import createLayer, createSampleLayer

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
    self.mainbus.gain = 30
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
    ############################
    dspstg = newlyr.appendStage("DSP")
    ampstg = newlyr.appendStage("AMP")
    ############################
    dspstg.ioconfig.inputs = [0]    # mono input
    dspstg.ioconfig.outputs = [0]   # mono output
    ampstg.ioconfig.inputs = [0]    # mono input
    ampstg.ioconfig.outputs = [0,1] # stereo output
    ############################
    pchblock = dspstg.appendDspBlock("Pitch","pitch")
    hwinput = dspstg.appendDspBlock("HwInput","hwi")
    noisegate = dspstg.appendDspBlock("AmpNoiseGate","ng")
    ampblock = ampstg.appendDspBlock("AmpAdaptive","amp")
    ############################
    newlyr.pitchBlock = pchblock
    newlyr.panmode = 4     # fixed / floatpan (-1 .. +1)
    newlyr.floatPan = 0.0
    ############################
    noisegate.threshold = 0.0005
    noisegate.input_gain = 1.0
    noisegate.output_gain = 1 / noisegate.input_gain
    noisegate.attack = 0.05
    noisegate.release = 0.0005
    ############################
    #m2s.params["gain"] = 0.0
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
