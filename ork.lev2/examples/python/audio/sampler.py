from orkengine.core import *
from orkengine.lev2 import *
from orkengine.lev2 import singularity as S

################################################################################

def addResonator(layer,dspstg):

  rezblock = dspstg.appendDspBlock("Filter4PoleWithSep","reson")

  ###########################

  cutoff = rezblock.paramByName("cutoff")
  cutoff.coarse = 1500
  
  cutoff_lfo = layer.appendController("Lfo", "ResonLFO")
  cutoff_lfo.properties.minRate = 2.03

  cutoff.mods.src1 = cutoff_lfo
  cutoff.mods.src1bias = 0
  cutoff.mods.src1scale = 1000

  ###########################

  w = rezblock.paramByName("resonance")
  w.coarse = 6.05
  w_lfo = layer.appendController("Lfo", "WidthLFO")
  w_lfo.properties.minRate = 0.73
  w.mods.src1 = w_lfo
  w.mods.src1bias = .05
  w.mods.src1scale = .05

  sep = rezblock.paramByName("separation")
  sep.coarse = 1200

  ###########################

################################################################################

def createLayer( program,           # 
                 pitch_lfo=False,   #
                 resonator=False):  #
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
  #########################################
  # sampler..
  #########################################
  SOSCIL = dspstg.appendDspBlock("Sampler","soscil")
  #########################################
  # carrier
  #########################################
  ampenv = newlyr.appendController("RateLevelEnv", "AMPENV")
  ampenv.ampenv = True
  ampenv.bipolar = False
  ampenv.sustainSegment = 0
  ampenv.addSegment("seg0", 0, 1,0.4)
  ampenv.addSegment("seg1", 1, .5,4)
  ampenv.addSegment("seg2", 1, 0,2.0)
  #########################################
  # pitch LFO ?
  #########################################
  if pitch_lfo:
    pitch_lfo = newlyr.appendController("Lfo", "PitchLFO")
    pitch_lfo.properties.minRate = 0.03
    pchblock.paramByName("pitch").mods.src1 = pitch_lfo
    pchblock.paramByName("pitch").mods.src1scale = 1200.0
  #########################################
  # post amp
  #########################################
  ampblock = ampstg.appendDspBlock("AmpAdaptive","amp")
  ampblock.paramByName("gain").mods.src1 = ampenv
  ampblock.paramByName("gain").mods.src1scale = 1.0
  #########################################
  if resonator:
    addResonator(newlyr,dspstg)
  return newlyr, SOSCIL


################################################################################

def createLayerWithMultiSample( program,     #
                                multisample, #
                                lokey,       #
                                hikey,       #
                                lowpass,     #
                                tuning=0,    #
                                pan=7):      #

        
  newlyr, SOSCIL = createLayer(program)
  newlyr.pan = pan
  keymap = S.KeyMapData("KMAP")
  the_sample = multisample.sampleByIndex(0)
  R0 = keymap.addRegion(
    lokey=lokey,
    hikey=hikey,
    lovel=0,
    hivel=127,
    tuning=tuning,
    multisample=multisample,
    sample=the_sample)
  newlyr.keymap = keymap    
  SOSCIL.lowpassfreq = lowpass
  return newlyr, SOSCIL

################################################################################

def createSampleLayerEX( program,    #
                         filename,   #
                         orig_pitch, #
                         lokey,      #
                         hikey,      #
                         lowpass,    #
                         tuning=0,   #
                         pan=7):     #

    newlyr, SOSCIL = createLayer(program)
    newlyr.pan = pan
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
    newlyr.keymap = keymap    
    SOSCIL.lowpassfreq = lowpass
    return the_sample, newlyr, SOSCIL