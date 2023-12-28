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
    self.czdata = singularity.CzSynthData()
    self.czdata.loadBank("bank0", self.syn_data_base/"cz1_1.bnk")
    self.czdata.loadBank("bank1", self.syn_data_base/"cz1_2.bnk")
    self.czdata.loadBank("bank2", self.syn_data_base/"cz1_3.bnk")
    self.czdata.loadBank("bank3", self.syn_data_base/"cz1_4.bnk")
    self.soundbank = self.czdata.bankData
    krz_ok_list = [
      "Stereo_Grand",
      "Real_Drums",
      "Steel_Str_Guitar",
      "Solo_Trumpet",
      "Slo_Chorus_Gtr",
      "Native_Drum",
      "Kotolin",
      "Hip_Brass",
      "Hi_Res_Sweeper",
      "Guitar_Mutes_1",
      "Guitar_Mutes_2",
      "General_MIDI_kit",
      "Finger_Bass",
      "Default_Program",
      "Click",
      "Classical_Gtr",
      "5_8ve_Percussion",
      "40_Something",
      "20's_Trumpet",
      "Wood_Bars",
      "WonderSynth_Bass",
      "Trumpet+Bone",
      "Tine_Elec_Piano",
      "Tenor_Sax",
    ]
    self.krzdata = singularity.KrzSynthData()
    print("B<%s>" % self.krzdata.bankData.programsByName)
    B2 = self.krzdata.bankData.filterPrograms(krz_ok_list)
    print("B2<%s>" % B2.programsByName)
    self.soundbank.merge(B2)
    ############################
    newprog = singularity.ProgramData()
    newprog.name = "_HYBRID"
    newprog.merge(self.soundbank.programByName("Stereo_Grand"))
    newprog.merge(self.soundbank.programByName("Wood_Bars"))
    newprog.merge(self.soundbank.programByName("Delayed Pad"))
    newprog.merge(self.soundbank.programByName("Strings Chorus1"))    
    self.soundbank.addProgram(newprog)
    ############################
    self.czprogs = self.soundbank.programsByName
    self.sorted_progs = sorted(self.czprogs.keys())
    # find index of "Bells and Chimes" in sorted_progs
    self.prog_index = find_index(self.sorted_progs, "Casio Toms")
    print("prog_index<%d>" % self.prog_index)

  ##############################################

###############################################################################

app = HybridApp()
app.ezapp.mainThreadLoop()
