#!/usr/bin/env python3

################################################################################
# singularity test for editing layers in a soundbank
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import numpy as np
import sys, random
from obt import path
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

  def modLayer(self,newlyr):
    ############################
    irdataset = S.SpectralImpulseDataSet()
    IRdir = path.share()/"singularity"/"IRs"
    #irwave = IRdir/"Fender SuperChamp AT4050.wav"
    #irwave = IRdir/"Fender SuperChamp Neumann U-87.wav"
    #irwave = IRdir/"Fender SuperChamp SM57 off Axis.wav"
    #irwave = IRdir/"Fender SuperChamp SM57.wav"
    irwave = IRdir/"Fender Bassman AT4050.wav"
    
    ir0 = S.SpectralImpulseResponse()
    ir0.loadAudioFileX(str(irwave))
    irdataset.resize(1)
    irdataset.set(0, ir0)
    ############################
    dspstg = newlyr.stage("DSP")
    #frqdom = dspstg.appendDspBlock("ToFrequencyDomain","2frq")
    spccon = dspstg.appendDspBlock("SpectralConvolveTD","sop4")
    spccon.dataset = irdataset
    #timdom = dspstg.appendDspBlock("ToTimeDomain","2tim")
    # create LFO
    lfo = newlyr.appendController("Lfo","vowel-lfo")
    lfo.minRate = 0.1
    lfo.maxRate = 0.1
    # set LFO as modulator
    spec_index_param = spccon.paramByIndex(0)
    spec_index_param.mods.src1 = lfo
    spec_index_param.mods.src1scale = 0.5
    spec_index_param.mods.src1bias = 0.5

###############################################################################

TestApp().ezapp.mainThreadLoop()

