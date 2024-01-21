import sys, random

def makeT8XLayer(newprog,fi,offset):
  fibi = (fi-0.5)*2.0
  newlyr = newprog.newLayer()
  dspstg = newlyr.appendStage("DSP")
  ampstg = newlyr.appendStage("AMP")
  dspstg.ioconfig.inputs = [0]
  dspstg.ioconfig.outputs = [0]
  ampstg.ioconfig.inputs = [0]
  ampstg.ioconfig.outputs = [0]
  pchblock = dspstg.appendDspBlock("Pitch","pitch")
  sawblock = dspstg.appendDspBlock("OscilSaw","saw1")
  panblock = dspstg.appendDspBlock("AmpPanner","pan")
  ampblock = ampstg.appendDspBlock("AmpAdaptive","amp")
  #
  panblock.paramByName("POS").coarse=fibi
  #
  ampenv = newlyr.appendController("RateLevelEnv", "AMPENV")
  ampenv.ampenv = True
  ampenv.bipolar = False
  ampenv.sustainSegment = 0
  ampenv.addSegment("seg0", 0.5, 1,0.4)
  ampenv.addSegment("seg1", 0.5, .5,4)
  ampenv.addSegment("seg2", 0.5, 0,2.0)
  #
  ampblock.paramByName("gain").mods.src1 = ampenv
  ampblock.paramByName("gain").mods.src1scale = 1.0
  #
  r = random.randrange(-4,4)
  pchenv = newlyr.appendController("RateLevelEnv", "PITCHENV")
  pchenv.ampenv = False
  pchenv.bipolar = True
  pchenv.releaseSegment=1
  pchenv.addSegment("seg0", 0.1, 0,2)
  pchenv.addSegment("seg1", 16+r, 1,0.5)
  pchenv.addSegment("seg2", 10, 1,0.5)
  #
  coarse = offset
  fine = fi*7200 # -4500 ..4500
  finehz = 0
  r = random.randrange(0,3)
  newlyr.gain = -30
  if r>0:
    coarse = -12
  if r>1:
    coarse = +12
  print(coarse)
  newlyr.gain -= (60+(coarse*0.5))
  #
  sawblock.paramByName("pitch").coarse = coarse
  sawblock.paramByName("pitch").fine = fine
  sawblock.paramByName("pitch").fineHZ = finehz
  sawblock.paramByName("pitch").mods.src1 = pchenv
  sawblock.paramByName("pitch").mods.src1scale = -fine*0.997

  return newlyr