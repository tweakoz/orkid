from orkengine.core import *
from orkengine.lev2 import singularity as S
import numpy as np
import math

tokens = CrcStringProxy()

class TestLayer:
  ###########################################################
  def __init__(self,program):
    newlyr = program.newLayer()
    newlyr.panmode = 2
    newlyr.pan = 7
    #########################################
    dspstg = newlyr.appendStage("DSP")
    #########################################
    ampstg = newlyr.appendStage("AMP")
    dspstg.ioconfig.inputs = [0,1]
    dspstg.ioconfig.outputs = [0,1]
    ampstg.ioconfig.inputs = [0,1]
    ampstg.ioconfig.outputs = [0,1]
    #########################################
    pchblock = dspstg.appendDspBlock("Pitch","pitch")
    newlyr.pitchBlock = pchblock
    self.layer = newlyr
    self.pchblock = pchblock
    self.ampstg = ampstg
    self.dspstg = dspstg
    self.program = program
  ###########################################################
  def addSampler(self):
    self.soscil = self.dspstg.appendDspBlock("Sampler","soscil")
  ###########################################################
  def addSamplerEX(self,
                   filename,
                   orig_pitch,
                   lokey,
                   hikey,
                   lowpass,
                   tuning=0,
                   pan=7):
    self.addSampler()
    self.layer.pan = pan
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
      tuning=tuning,
      multisample=multisample,
      sample=the_sample)
    self.layer.keymap = keymap    
    self.soscil.lowpassfreq = lowpass
    self.the_sample = the_sample
    self.the_multisample = multisample
    self.the_keymap = keymap
  ###########################################################
  def addAmpEnv(self):
    ampenv = self.layer.appendController("RateLevelEnv", "AMPENV")
    ampenv.ampenv = True
    ampenv.bipolar = False
    ampenv.sustainSegment = 0
    ampenv.addSegment("seg0", 0, 1,0.4)
    ampenv.addSegment("seg1", 1, .5,4)
    ampenv.addSegment("seg2", 1, 0,2.0)
    self.ampenv = ampenv
    self.ampblock = self.ampstg.appendDspBlock("AmpAdaptive","AMP")
  ###########################################################
  def addPitchLfo(self):
    pitch_lfo = self.layer.appendController("Lfo", "PitchLFO")
    pitch_lfo.properties.minRate = 0.7
    self.pchblock.paramByName("pitch").mods.src1 = pitch_lfo
    self.pchblock.paramByName("pitch").mods.src1scale = 25.0
    self.pitch_lfo = pitch_lfo
  ###########################################################
  def testWaveform(self):
    self.addSampler()
    self.addAmpEnv()
    self.addPitchLfo()
    #########################################
    # waveform data
    #########################################
    wavelength = 512
    samplerate = 16000
    root_key = 36
    orig_pitch = (samplerate/wavelength)
    highestPitch = orig_pitch * 48000.0/samplerate
    highestPitchN = S.frequencyToMidiNote(highestPitch)
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
      for j in range(0,128):
        fj = (j/128.0)
        fn = math.pow(fs*fj + (1.0-fj),0.5)
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
    self.layer.keymap = self.keymap

################################################################################

class KrzBankSource:
  def __init__(self):
    self.syn_data_base = S.baseDataPath()/"kurzweil"
    self.krzdata = S.KrzSynthData(base_objects=False)
    def do_bank(name,filename,remapbase=0):
      self.krzdata.loadBank( name=name, 
                             remap_base=remapbase,
                             path=self.syn_data_base/filename)

    do_bank( "monksvox.kr1", "monksvox.kr1.krz")
    do_bank( "monksvox.kr2", "monksvox.kr2.krz")
    do_bank( "rain_1.krz", "rain_1.krz")
    do_bank( "moogbass.krz", "moogbass.krz")
    do_bank( "steinway.krz", "steinway.krz")
    do_bank( "trumpets.krz", "trumpets.krz")
    do_bank( "bothfrhn.krz", "bothfrhn.krz")
    do_bank( "b3.krz", "b3.krz")
    do_bank( "pipeorgn.krz", "pipeorgn.krz")
    do_bank( "lostring.krz", "lostring.krz")
    do_bank( "sledge.krz", "sledge.krz")
    do_bank( "alesisdr", "alesisdr.krz" )
    do_bank( "m1drums", "m1drums.krz")
    do_bank( "emusp12", "emusp12.krz")

    self.soundbank = self.krzdata.bankData
    self.krzprogs = self.soundbank.programsByName
    self.krzsamps = self.soundbank.multiSamplesByName
    self.krzkmaps = self.soundbank.keymapsByName

    #for item in self.krzsamps:
    #  print(item)
    
    ###################
    # enable looping on select samples
    ###################
    def enableLooping(sampname):
      s = self.krzsamps[sampname]
      ns = s.numSamples
      for i in range(0,ns):
        s = s.sampleByIndex(i)
        #print(s,dir(s),s.name)
        #print("start", s.start)
        #print("end", s.end)
        #print("loopstart", s.loopstart)
        #print("loopend", s.loopend)
        #print("loopmode", s.loopmode)
        s.loopstart = 0
        s.loopend = s.end
        s.loopmode = 1
   ###################
    enableLooping("EMAX_60P")
    enableLooping("Domini")
    enableLooping("Monk-Sustain")
    enableLooping("Tibetan_Monks")
    #assert(False)
    ################################################################################
