#!/usr/bin/env python3

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

################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from singularity import _frqdom as frqdom
################################################################################

def generateVowelDataset():
  irdataset = S.SpectralImpulseDataSet()
  strength = 64.0
  sirA = S.SpectralImpulseResponse()
  sirA.vowelFormant('A',strength)
  sirE = S.SpectralImpulseResponse()
  sirE.vowelFormant('E',strength)
  sirI = S.SpectralImpulseResponse()
  sirI.vowelFormant('I',strength)
  sirO = S.SpectralImpulseResponse()
  sirO.vowelFormant('O',strength)
  sirU = S.SpectralImpulseResponse()
  sirU.vowelFormant('U',strength)
  
  COUNT = 8
  S0 = COUNT//4
  S1 = COUNT//2
  S2 = COUNT*3//4
  irdataset.resize(COUNT)
  
  for i in range(0,COUNT):
    sir = S.SpectralImpulseResponse()
    if( i<S0 ):
      fi = i/float(S0)
      sir.blend(sirA,sirE,fi)
    elif( i<S1 ):
      fi = (i-S0)/float(S0)
      sir.blend(sirE,sirI,fi)
    elif( i<S2 ):
      fi = (i-S1)/float(S0)
      sir.blend(sirI,sirO,fi)
    else:
      fi = (i-S2)/float(S0)
      sir.blend(sirO,sirU,fi)   
    irdataset.set(i, sir)
  return irdataset

################################################################################

class TestApp(frqdom.WaveformsApp):
  def __init__(self):
    super().__init__()
  ##########################################
  def modLayer(self,newlyr):
    # fetch dsp stage
    dspstg = newlyr.stage("DSP")
    # create post-effect dsp blocks
    frqdom = dspstg.appendDspBlock("ToFrequencyDomain","2frq")
    vowels = dspstg.appendDspBlock("SpectralConvolve","sop4")
    timdom = dspstg.appendDspBlock("ToTimeDomain","2tim")
    # create LFO
    lfo = newlyr.appendController("Lfo","vowel-lfo")
    lfo.minRate = 1
    lfo.maxRate = 1
    # set LFO as modulator
    spec_index_param = vowels.paramByIndex(0)
    spec_index_param.mods.src1 = lfo
    spec_index_param.mods.src1scale = 0.5
    spec_index_param.mods.src1bias = 0.5
    # assign IR dataset
    vowels.dataset = generateVowelDataset()
    

###############################################################################

TestApp().ezapp.mainThreadLoop()
