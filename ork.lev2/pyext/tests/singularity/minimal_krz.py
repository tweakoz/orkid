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

class KrzApp(SingulTestApp):

  def __init__(self):
    super().__init__()
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    self.syn_data_base = singularity.baseDataPath()/"kurzweil"
    self.krzdata = singularity.KrzSynthData()
    self.krzdata.loadBank("alesisdr", self.syn_data_base/"alesisdr.krz")
    self.krzdata.loadBank("m1drums", self.syn_data_base/"m1drums.krz")
    self.soundbank = self.krzdata.bankData
    self.krzprogs = self.soundbank.programsByName
    self.sorted_progs = sorted(self.krzprogs.keys())
    ok_list = [
      "Stereo_Grand",
      "Real_Drums",
      "Steel_Str_Guitar",
      "Solo_Trumpet",
      "Chorus_Gtr",
      "Slo_Chorus_Gtr",
      "Native_Drum",
      "Kotolin",
      "Kotobira",
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
      "Bright_Piano",
      "Straight_Strat",
      "Soft_Trumpet",
      "Pizzicato_String",
      "Orch_EnglishHorn",
      "Orch_Clarinet",
      "Mbira_Stack",
      "Jazz_Quartet",
      "Jam_Corp",
      "India_Jam",
      "Harp_w_8ve_CTL",
      "Harmon_Section",
      "Elect_12_String",
      "Econo_Kit_",
      #"EDrum_Kit_1_",
      "Dynamic_Harp",
      "Dyn_Brass_+_Horn",
      "Dual_Tri_Bass",
      "DigiBass", # fix pan (alg 5)
      "Xylophone", # fix pan (alg 9)
      "Touch_MiniBass", # fix pan (alg 5)
      "CowGogiBell",
      "Chimes",
      "Cartoon_Perc",
      "Brt_Dbl_Bass",
      "BrazKnuckles",
      "Big_Drum_Corp",
      "Arco_Dbl_Bass",
      "Acoustic_Bass",
      "Touch_Clav",
      "Timpani_Chimes",
      "Sweetar"




    ]
    self.sorted_progs = ok_list
    print("krzprogs<%s>" % self.krzprogs)    
    self.prog_index = find_index(self.sorted_progs, "Straight_Strat")

###############################################################################

app = KrzApp()
app.ezapp.mainThreadLoop()
