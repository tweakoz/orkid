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

def create_violin_formant_response(cplxlen, sample_rate):
  # Initialize arrays for the complex spectrum (real and imaginary parts)
  real = np.ones(cplxlen)*0.35
  imag = np.zeros(cplxlen)
  
  # Define violin resonant frequencies and their bandwidth
  formants = [
      (280, 100), # Main air resonance (Helmholtz resonance)
      (450, 100), # First major wood resonance
      (600, 100), # Second wood resonance
      (1000, 120), # Additional body resonance
      (1400, 150), # Upper body resonance
      (2500, 200), # Brilliance range start
      (3500, 300), # Brilliance range peak
      (5000, 400), # High-end brilliance and projection
  ]
  for freq, bandwidth in formants:
    center_bin = int((freq / sample_rate) * cplxlen)
    bandwidth_bins = int((bandwidth / sample_rate) * cplxlen)
    for bin in range(max(0, center_bin-bandwidth_bins), 
                      min(cplxlen, center_bin+bandwidth_bins+1)):
      real[bin] *= 4.5 # Example boost factor
  return real, imag
  
################################################################################
  
class TestApp(frqdom.WaveformsApp):
  def __init__(self):
    super().__init__()
  ##########################################
  def modLayer(self,newlyr):
    irdataset = S.SpectralImpulseDataSet()
    cplxlen = S.spectralComplexSize()
    violinR,violinI = create_violin_formant_response(cplxlen, 48000)
    COUNT = 256
    irdataset.resize(COUNT)
    for i in range(0,COUNT):
      fi = (i/float(COUNT))
      sir = S.SpectralImpulseResponse()
      sir.violinFormant(1.0+fi*64.0)
      #sir.setFrequencyResponse(violinR,violinI,violinR,violinI)
      irdataset.set(i, sir)
    ############################
    dspstg = newlyr.stage("DSP")
    frqdom = dspstg.appendDspBlock("ToFrequencyDomain","2frq")
    spccon = dspstg.appendDspBlock("SpectralConvolve","sop4")
    spccon.dataset = irdataset
    timdom = dspstg.appendDspBlock("ToTimeDomain","2tim")

###############################################################################

TestApp().ezapp.mainThreadLoop()
