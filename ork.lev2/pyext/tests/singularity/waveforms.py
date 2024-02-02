#!/usr/bin/env python3

################################################################################
# singularity test for editing layers in a soundbank
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, random
from orkengine.core import *
from orkengine.lev2 import *
from orkengine.lev2 import singularity as S

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
    newlyr = newprog.newLayer()
    dspstg = newlyr.appendStage("DSP")
    ampstg = newlyr.appendStage("AMP")
    dspstg.ioconfig.inputs = []
    dspstg.ioconfig.outputs = [0,1]
    ampstg.ioconfig.inputs = [0,1]
    ampstg.ioconfig.outputs = [0,1]
    pchblock = dspstg.appendDspBlock("Pitch","pitch")
    newlyr.pitchBlock = pchblock
    #########################################
    # carrier
    #########################################
    ampenv = newlyr.appendController("RateLevelEnv", "AMPENV")
    ampenv.ampenv = True
    ampenv.bipolar = False
    ampenv.sustainSegment = 0
    ampenv.addSegment("seg0", 0, 1,0.4)
    ampenv.addSegment("seg1", 0.2, .5,4)
    ampenv.addSegment("seg2", 0.2, 0,2.0)
    #
    #########################################
    # post amp
    #########################################

    ampblock = ampstg.appendDspBlock("AmpAdaptive","amp")
    ampblock.paramByName("gain").mods.src1 = ampenv
    ampblock.paramByName("gain").mods.src1scale = 1.0

    #########################################
    # waveform data
    #########################################

    print("#####################")

    wavelength = 512
    samplerate = 16000
    root_key = 48
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
    
    waveform = [math.sin(2.0 * math.pi * (i / wavelength)) for i in range(wavelength)]
    
    the_sample = S.SampleData(
      name = "MySample",
      format = tokens.FLOAT32_LIST,
      waveform = waveform,
      # key which will play back at original pitch
      rootKey = root_key,                
      # offset in cents
      pitchAdjustCents = 0.0,
      # samples per second of recording             
      sampleRate = samplerate,  
      highestPitchCents = highestPitchCents,
      # loop endpoint
      loopPoint = wavelength-1,        
    )
    multisample = S.MultiSampleData("MSAMPLE",[the_sample])
    keymap = S.KeyMapData("KMAP")
    R0 = keymap.addRegion(
      lokey=0,
      hikey=84,
      lovel=0,
      hivel=127,
      multisample=multisample,
      sample=the_sample)
    newlyr.keymap = keymap
    ############################
    # sample playback oscillator
    ############################
    SOSCIL = dspstg.appendDspBlock("Sampler","soscil")
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
