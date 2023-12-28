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

class Tx81zApp(SingulTestApp):

  def __init__(self):
    super().__init__()
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    self.syn_data_base = singularity.baseDataPath()/"tx81z"
    self.txdata = singularity.Tx81zSynthData()
    self.txdata.loadBank("bank1",self.syn_data_base/"tx81z_1.syx")
    self.txdata.loadBank("bank2",self.syn_data_base/"tx81z_2.syx")
    self.soundbank = self.txdata.bankData
    self.txprogs = self.soundbank.programsByName
    self.sorted_progs = sorted(self.txprogs.keys())
    print("txprogs<%s>" % self.txprogs)
    self.prog_index = 0

###############################################################################

app = Tx81zApp()
app.ezapp.mainThreadLoop()
