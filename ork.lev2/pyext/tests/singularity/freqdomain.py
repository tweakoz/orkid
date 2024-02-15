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

def createCombFilterIR(samplerate, notchSpacing, maxFrequency):
  delayInSamples = int(samplerate / notchSpacing)
  numberOfNotches = int(maxFrequency / notchSpacing)
  ir = np.zeros(delayInSamples + 1)
  ir[0] = 1.0
  ir[delayInSamples] = -1.0
  return ir

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

    if True:      
      newlyr, SOSCIL = createLayer(newprog)

      #########################################
      # waveform data
      #########################################

      print("#####################")

      wavelength = 512
      samplerate = 16000
      root_key = 36
      orig_pitch = (samplerate/wavelength)
      #rootpitch = singularity.midiNoteToFrequency(root_key) # 466.1637615180899
      highestPitch = orig_pitch * 48000.0/samplerate
      highestPitchN = singularity.frequencyToMidiNote(highestPitch)
      highestPitchCents = int(highestPitchN*100.0)+1
      fratio   = 96000.0 / math.floor(samplerate);
      frqerc = singularity.linearFrequencyRatioToCents(fratio)
      calch    = root_key * (frqerc / 100.0)
      pitchADJcents = calch - highestPitchCents
      delcents = frqerc-pitchADJcents
      #print("rootpitch", rootpitch )
      print("orig_pitch", orig_pitch )
      print("highestPitch", highestPitch )
      print("highestPitchN", highestPitchN )
      print("highestPitchCents", highestPitchCents )
      print("fratio", fratio )
      print("frqerc", frqerc )
      print("calch", calch )
      print("pitchADJcents", pitchADJcents )
      print("delcents", delcents )
      
      print("#####################")
      
      #############################
      # animated fourier series
      #############################

      final_waveform = np.zeros(256 * wavelength)
      for i in range(0,256):
        fi = i/256.0
        fs = 0.5+math.sin(fi*2.0*3.14159)+0.5
        t = np.linspace(0, 1, wavelength, endpoint=False)  # Time array for one cycle
        waveform = np.zeros(wavelength)
        for j in range(0,90):
          fj = (j/90.0)
          fn = fs*fj + (1.0-fj)
          waveform = waveform + (np.sin(2 * np.pi * (j + 1)  * t) / (j + 1))*fn
        waveform = waveform / np.max(np.abs(waveform))*0.1  # Normalize to -1 to 1
        final_waveform[i * wavelength:(i + 1) * wavelength] = waveform
        
      #############################

      the_sample = S.SampleData(
        name = "MySample",
        format = tokens.F32_NPARRAY,
        waveform = final_waveform,
        # key which will play back at original pitch
        rootKey = root_key,                
        # offset in cents
        pitchAdjustCents = 0.0,
        # samples per second of recording             
        sampleRate = samplerate,  
        highestPitchCents = highestPitchCents,
        # loop endpoint
        loopPoint = len(final_waveform)-1,        
      )
      multisample = S.MultiSampleData("MSAMPLE",[the_sample])
      keymap = S.KeyMapData("KMAP")
      R0 = keymap.addRegion(
        lokey=0,
        hikey=96,
        lovel=0,
        hivel=127,
        multisample=multisample,
        sample=the_sample)
      newlyr.keymap = keymap

    ############################
    irdataset = S.SpectralImpulseDataSet()
    irdataset.resize(100)
    for i in range(0,100):
      frq = 1000 + (i*100)
      ir = createCombFilterIR(48000, frq, 20000)
      sir = S.SpectralImpulseResponse()
      sir.set(ir,ir)
      irdataset.set(i, sir)
    ############################
    dspstg = newlyr.stage("DSP")
    frqdom = dspstg.appendDspBlock("ToFrequencyDomain","2frq")
    #sshdom = dspstg.appendDspBlock("SpectralShift","sop")
    #scadom = dspstg.appendDspBlock("SpectralScale","sop")
    #spctst = dspstg.appendDspBlock("SpectralTest","sop")
    spccon = dspstg.appendDspBlock("SpectralConvolve","sop")
    spccon.dataset = irdataset
    timdom = dspstg.appendDspBlock("ToTimeDomain","2tim")
    print("DSPSTG<%s>" % dspstg)
    print("frqdom<%s>" % frqdom)
    print("timdom<%s>" % timdom)
    #assert(False)

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
