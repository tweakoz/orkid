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
from _sampler import createLayer, createSampleLayer

################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
from singularity._harness import SingulTestApp, find_index
tokens = CrcStringProxy()
################################################################################

class WaveformsApp(SingulTestApp):

  def __init__(self):
    super().__init__()
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    prgname = "waveforms"
    self.mainbus.gain = 0
    self.octave = 4
    ############################
    # create a new hybrid patch
    #  mixing different synth architectures
    ############################
    self.new_soundbank = singularity.BankData()
    ############################
    newprog = self.new_soundbank.newProgram(prgname)
    #newprog.monophonic = True
    #newprog.portamentoRate = 60000 # cents per second
    ############################

    ############################
    # from wave file
    ############################
    
    def createSampleLayer(filename,orig_pitch,lokey,hikey,lowpass):          
        newlyr, SOSCIL = createLayer(newprog)
        the_sample = S.SampleData(
          name = "MySample2",
          #format = tokens.F32_NPARRAY,
          originalPitch = orig_pitch,
          audiofile = str(filename),
          rootKey = 48,                
          pitchAdjustCents = 0.0,
          #sampleRate = samplerate,  
          #highestPitchCents = highestPitchCents,
          _interpMethod = 2,
        )
        multisample = S.MultiSampleData("MSAMPLE",[the_sample])
        keymap = S.KeyMapData("KMAP")
        R0 = keymap.addRegion(
          lokey=lokey,
          hikey=hikey,
          lovel=0,
          hivel=127,
          multisample=multisample,
          sample=the_sample)
        newlyr.keymap = keymap    
        SOSCIL.lowpassfreq = lowpass
    
    createSampleLayer(singularity.baseDataPath()/"wavs"/"feb142023.mp3",31,0,36,12000)
    createSampleLayer(singularity.baseDataPath()/"wavs"/"VlnEns_Trem_A2_v1.wav",110.5,37,57,7000)
    createSampleLayer(singularity.baseDataPath()/"wavs"/"bdrum2_pp_1.wav",47,58,59,12000)
    createSampleLayer(singularity.baseDataPath()/"wavs"/"snare_f3.wav",96,60,63,12000)
    #createSampleLayer(singularity.baseDataPath()/"wavs"/"bdrum_f_2.wav",73,96)

    ############################
    self.soundbank = self.new_soundbank
    ############################
    ok_list = [
      prgname
    ]
    ############################
    self.sorted_progs = sorted(ok_list)
    self.prog_index = find_index(self.sorted_progs, prgname)
    self.synth.programbus.uiprogram = newprog
    print(self.prog_index)
    if self.pgmview:
      self.pgmview.setProgram(newprog)
  ##############################################

###############################################################################

WaveformsApp().ezapp.mainThreadLoop()
