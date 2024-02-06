import sys, math 
import numpy as np
from orkengine.core import *
from orkengine.lev2 import *
from orkengine.lev2 import singularity as S

sys.path.append(thisdir().normalized.as_string)
from sampler import createLayer, createSampleLayerEX
tokens = CrcStringProxy()

class NoDataSynthProgram:
  def __init__(self,do_lfo=False,do_rez=False):
    self.prgname = "waveforms"
    ############################
    # create a new hybrid patch
    #  mixing different synth architectures
    ############################
    self.soundbank = singularity.BankData()
    ############################
    self.patch = self.soundbank.newProgram(self.prgname)
    ############################
    self.newlyr, self.SOSCIL = createLayer(self.patch,pitch_lfo=do_lfo,resonator=do_rez)
    #########################################
    # waveform data
    #########################################
    wavelength = 512
    samplerate = 16000
    root_key = 36
    orig_pitch = (samplerate/wavelength)
    highestPitch = orig_pitch * 48000.0/samplerate
    highestPitchN = singularity.frequencyToMidiNote(highestPitch)
    highestPitchCents = int(highestPitchN*100.0)+1
    #############################
    # waveform: animated fourier series
    #############################
    final_waveform = np.zeros(256 * wavelength)
    for i in range(0,256):
      fi = i/256.0
      fs = 0.5+math.sin(fi*2.0*3.14159)+0.5
      t = np.linspace(0, 1, wavelength, endpoint=False)  # Time array for one cycle
      waveform = np.zeros(wavelength)
      for j in range(0,60):
        fj = (j/60.0)
        fn = math.pow(fs*fj + (1.0-fj),0.25)
        waveform = waveform + (np.sin(2 * np.pi * (j + 1)  * t) / (j + 1))*fn
      waveform = waveform / np.max(np.abs(waveform))*0.1  # Normalize to -1 to 1
      final_waveform[i * wavelength:(i + 1) * wavelength] = waveform
    #############################
    self.the_sample = S.SampleData(
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
    self.multisample = S.MultiSampleData("MSAMPLE",[self.the_sample])
    self.keymap = S.KeyMapData("KMAP")
    self.R0 = self.keymap.addRegion(
      lokey=0,
      hikey=96,
      lovel=0,
      hivel=127,
      multisample=self.multisample,
      sample=self.the_sample)
    self.newlyr.keymap = self.keymap
    