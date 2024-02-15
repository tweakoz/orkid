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
    for i in range(0,256):
      fi = i / 256.0
      sir = S.SpectralImpulseResponse()
      frqA = 500 + (fi*500)
      frqB = 2500 + (fi*1000)
      frqC = 5500 + (fi*2000)
      frqD = 12000 - (fi*4000)
      frqs = vec4(frqA,frqB,frqC,frqD)
      gains = vec4(-24,-48,-48,-48)
      qvals = vec4(1,4,4,4)
      sir.parametricEQ4(frqs,gains,qvals)
      irdataset.set(i, sir)
    ############################
    dspstg = newlyr.stage("DSP")
    frqdom = dspstg.appendDspBlock("ToFrequencyDomain","2frq")
    #sshdom = dspstg.appendDspBlock("SpectralShift","sop1")
    #scadom = dspstg.appendDspBlock("SpectralScale","sop2")
    #spctst = dspstg.appendDspBlock("SpectralTest","sop3")
    spccon = dspstg.appendDspBlock("SpectralConvolve","sop4")
    spccon.dataset = irdataset
    timdom = dspstg.appendDspBlock("ToTimeDomain","2tim")
    print("DSPSTG<%s>" % dspstg)
    print("frqdom<%s>" % frqdom)
    print("timdom<%s>" % timdom)

###############################################################################

TestApp().ezapp.mainThreadLoop()
