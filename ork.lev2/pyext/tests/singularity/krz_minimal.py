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

    self.krzdata.loadBank( name="alesisdr", 
                           path=self.syn_data_base/"alesisdr.krz")
    self.krzdata.loadBank( name="m1drums", 
                           remap_base=300, 
                           path=self.syn_data_base/"m1drums.krz")
    
    
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

    newprog = self.soundbank.newProgram("YO")

    createSampleLayer(
      newprog,
      multisample=self.krzsamps["Kick1"],
      lokey=0,
      hikey=63,
      lowpass=8000)
    
    PRG = "YO"
    self.setBusProgram(main,self.soundbank.programByName(PRG))
    self.prog_index = find_index(self.sorted_progs, PRG)
    self.prog = self.soundbank.programByName(PRG)
    self.setUiProgram(self.prog)

    
    self.synth.masterGain = singularity.decibelsToLinear(-24.0)

###############################################################################

app = KrzApp()
app.ezapp.mainThreadLoop()
