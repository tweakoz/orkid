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

class TestApp(frqdom.WaveformsApp):
  def __init__(self):
    super().__init__()
  ##########################################
  def modLayer(self,newlyr):
    irdataset = S.SpectralImpulseDataSet()
    irdataset.resize(256)
    strength = 32.0
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
    for i in range(0,256):
      sir = S.SpectralImpulseResponse()
      if( i<64 ):
        fi = i/64.0
        sir.blend(sirA,sirE,fi)
      elif( i<128 ):
        fi = (i-64)/64.0
        sir.blend(sirE,sirI,fi)
      elif( i<192 ):
        fi = (i-128)/64.0
        sir.blend(sirI,sirO,fi)
      else:
        fi = (i-192)/64.0
        sir.blend(sirO,sirU,fi)   
      irdataset.set(i, sir)
############################
    dspstg = newlyr.stage("DSP")
    frqdom = dspstg.appendDspBlock("ToFrequencyDomain","2frq")
    spccon = dspstg.appendDspBlock("SpectralConvolve","sop4")
    spccon.dataset = irdataset
    timdom = dspstg.appendDspBlock("ToTimeDomain","2tim")

###############################################################################

TestApp().ezapp.mainThreadLoop()
