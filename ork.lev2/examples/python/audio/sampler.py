from orkengine.core import *
from orkengine.lev2 import *
from orkengine.lev2 import singularity as S

def createLayer(program):
  newlyr = program.newLayer()
  dspstg = newlyr.appendStage("DSP")
  ampstg = newlyr.appendStage("AMP")
  dspstg.ioconfig.inputs = [0,1]
  dspstg.ioconfig.outputs = [0,1]
  ampstg.ioconfig.inputs = [0,1]
  ampstg.ioconfig.outputs = [0,1]
  pchblock = dspstg.appendDspBlock("Pitch","pitch")
  newlyr.pitchBlock = pchblock
  newlyr.panmode = 2
  newlyr.pan = 7
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
  #
  #########################################
  # post amp
  #########################################

  ampblock = ampstg.appendDspBlock("AmpAdaptive","amp")
  ampblock.paramByName("gain").mods.src1 = ampenv
  ampblock.paramByName("gain").mods.src1scale = 1.0
  SOSCIL = dspstg.appendDspBlock("Sampler","soscil")
  return newlyr, SOSCIL

def createSampleLayer(program,
                      multisample,
                      lokey,
                      hikey,
                      lowpass,
                      tuning=0,
                      pan=7):

        
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

def createSampleLayerEX(program,filename,orig_pitch,lokey,hikey,lowpass):          
    newlyr, SOSCIL = createLayer(program)
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