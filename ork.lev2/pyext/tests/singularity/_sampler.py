from orkengine.core import *
from orkengine.lev2 import *
from orkengine.lev2 import singularity as S

def createLayer(program):
  newlyr = program.newLayer()
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
                      lowpass):

        
  newlyr, SOSCIL = createLayer(program)
  keymap = S.KeyMapData("KMAP")
  the_sample = multisample.sampleByIndex(0)
  R0 = keymap.addRegion(
    lokey=lokey,
    hikey=hikey,
    lovel=0,
    hivel=127,
    multisample=multisample,
    sample=the_sample)
  newlyr.keymap = keymap    
  SOSCIL.lowpassfreq = lowpass
  return newlyr, SOSCIL