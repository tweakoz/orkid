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

  def genMods(self):
    timebase = self.time
    modrate = math.sin(self.time)*5
    mods = singularity.KeyOnModifiers()
    mods.layerMask = self.layermask
    mods.outputbus = self.synth.programbus
    #def sub(name,value):
    #  print("sub<%s> value<%s>" % (name,value))
    #mods.controllers.subscribers = {
    #"AMPENV": sub,
    #}
    return mods

  ##############################################

  def onNote(self,voice):
    if False:
      LD = self.prog.layer(self.layerID)
      LD = self.prog.layer(0)
      DST = LD.stage("DSP")
      DST.dspblock(2).bypass = True
    #ST.dspblock(0).paramByName("pitch").debug = True
    #ST.dspblock(4).paramByName("cutoff").debug = True
    #ST.dspblock(2).bypass = True
    #ST.dspblock(3).bypass = True
    #ST.dspblock(4).bypass = True
    #print("dspblk<%s>" % dspblk.name)
    pass

  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    self.syn_data_base = singularity.baseDataPath()/"kurzweil"
    self.krzdata = singularity.KrzSynthData(base_objects=False)

    self.krzdata.loadBank( name="alesisdr", 
                           path=self.syn_data_base/"alesisdr.krz")
    self.krzdata.loadBank( name="m1drums", 
                           remap_base=300, 
                           path=self.syn_data_base/"m1drums.krz")
    
    self.soundbank = self.krzdata.bankData
    self.krzprogs = self.soundbank.programsByName

    self.synth.masterGain = singularity.decibelsToLinear(-24.0)
    main = self.synth.outputBus("main")
    aux8 = self.synth.outputBus("aux8")

###############################################################################

app = KrzApp()
app.ezapp.mainThreadLoop()
