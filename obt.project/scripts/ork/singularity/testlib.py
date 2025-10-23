import math 
import numpy as np
from orkengine.core import CrcStringProxy
from orkengine.lev2 import singularity as SINGUL
from ork.singularity import sampler

tokens = CrcStringProxy()

class WaveformsProgram:
  def __init__(self):
    prgname = "waveforms"
    new_soundbank = SINGUL.BankData()
    newprog = new_soundbank.newProgram(prgname)
    newlyr, SOSCIL, dspstg, ampstg = sampler.createLayer(newprog)

    #########################################
    # waveform data
    #########################################

    print("#####################")

    wavelength = 512
    samplerate = 16000
    root_key = 36
    orig_pitch = (samplerate/wavelength)
    #rootpitch = SINGUL.midiNoteToFrequency(root_key) # 466.1637615180899
    highestPitch = orig_pitch * 48000.0/samplerate
    highestPitchN = SINGUL.frequencyToMidiNote(highestPitch)
    highestPitchCents = int(highestPitchN*100.0)+1
    fratio   = 96000.0 / math.floor(samplerate);
    frqerc = SINGUL.linearFrequencyRatioToCents(fratio)
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
      for j in range(0,30):
        fj = (j/30.0)
        fn = fs*fj + (1.0-fj)
        waveform = waveform + (np.sin(2 * np.pi * (j + 1)  * t) / (j + 1))*fn
      waveform = waveform / np.max(np.abs(waveform))*0.1  # Normalize to -1 to 1
      final_waveform[i * wavelength:(i + 1) * wavelength] = waveform
      
    #############################

    the_sample = SINGUL.SampleData(
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
    multisample = SINGUL.MultiSampleData("MSAMPLE",[the_sample])

    #############################

    keymap = SINGUL.KeyMapData("KMAP")

    #############################

    R0 = keymap.addRegion(
      lokey=0,
      hikey=96,
      lovel=0,
      hivel=127,
      multisample=multisample,
      sample=the_sample)

    #############################

    pchlfo = newlyr.appendController("Lfo", "pchlfo")
    pchlfo.properties.shape = "Sine"
    pchlfo.properties.minRate = 0.3
    pchlfo.properties.maxRate = 0.4
    pchblock = dspstg.dspblockByName("pitch") # Pitch block
    #panblock = dspstg.appendDspBlock("AmpPanner","pan")
    pchblock.paramByName("pitch").coarse=0.0
    pchblock.paramByName("pitch").mods.src1 = pchlfo
    pchblock.paramByName("pitch").mods.src1scale = 1200.0 # cents

    #############################

    newlyr.keymap = keymap
    
    self.program = newprog
    self.soundbank = new_soundbank
    self.prgname = prgname
    self.layer = newlyr
    self.sample = the_sample
    self.multisample = multisample

    #############################
    
    
    
def bindSynthToApp(synth,app,initial_gain=0.0,main_fx=None):
  synth.masterGain = SINGUL.decibelsToLinear(initial_gain)
  app.mainbus = synth.outputBus("main")
  #app.synth.setEffect(app.mainbus,"Reverb:OilTank")
  if main_fx is not None:
    synth.setEffect(app.mainbus,main_fx)
  app.numaux = 9
  app.auxbusses = []
  app.auxbus_sources = []
  for i in range(0,app.numaux):
    app.auxbusses += [synth.createOutputBus("aux%d" % (i+1))]
    app.auxbus_sources += [app.auxbusses[i].createScopeSource()]
    synth.setEffect(app.auxbusses[i],"none")
  