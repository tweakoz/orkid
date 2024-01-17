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

################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
from singularity._harness import SingulTestApp, find_index
################################################################################

class AmbiApp(SingulTestApp):

  def __init__(self):
    super().__init__()
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    self.krzdata = singularity.KrzSynthData()
    krz_bank = self.krzdata.bankData
    self.synth.setEffect(self.mainbus,"none")
    self.mainbus.gain = 48
    self.octave = 4
    ############################
    # create a new hybrid patch
    #  mixing different synth architectures
    ############################
    self.new_soundbank = singularity.BankData()
    ############################
    newprog = self.new_soundbank.newProgram("PMX")
    newprog.monophonic = True
    newprog.portamentoRate = 36000 # cents per second
    ############################
    def makePMXLayer(mod_semis,car_semis):
      newlyr = newprog.newLayer()
      newlyr.gain = -18
      dspstg = newlyr.appendStage("DSP")
      ampstg = newlyr.appendStage("AMP")
      dspstg.ioconfig.inputs = []
      dspstg.ioconfig.outputs = [0,1]
      ampstg.ioconfig.inputs = [0,1]
      ampstg.ioconfig.outputs = [0,1]
      pchblock = dspstg.appendDspBlock("Pitch","pitch")
      #########################################
      # modulator
      #########################################
      modblock = dspstg.appendDspBlock("OscPMX","pmx")
      modenv = newlyr.appendController("RateLevelEnv", "MODENV")
      modenv.ampenv = False
      modenv.bipolar = False
      modenv.sustainSegment = 0
      modenv.addSegment("seg0", 0.0, 4,0.4)
      modenv.addSegment("seg1", 0.2, .5,4)
      modenv.addSegment("seg2", 0.2, 0,2.0)
      #
      modblock.properties.InputChannel = 0
      modblock.properties.PmInputChannels = [0,1,2,3]
      modblock.paramByName("amp").coarse = 0.0
      modblock.paramByName("amp").fine = 0.0
      modblock.paramByName("amp").mods.src1 = modenv
      modblock.paramByName("amp").mods.src1depth = 2
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
      carblock = dspstg.appendDspBlock("OscPMX","pmx2")
      carblock.properties.InputChannel = 0
      carblock.properties.PmInputChannels = [0,1,2,3]
      carblock.paramByName("amp").coarse = 1.0
      carblock.paramByName("pitch").coarse = car_semis
      #
      panlfo = newlyr.appendController("Lfo", "PANLFO")
      panlfo.properties.shape = "Saw"
      panlfo.properties.minRate = 0.125
      panlfo.properties.maxRate = 0.125
      panblock = dspstg.appendDspBlock("AmpPanner2D","pan")
      panblock.paramByName("ANGLE").coarse=-1
      panblock.paramByName("ANGLE").mods.src1 = panlfo
      panblock.paramByName("ANGLE").mods.src1depth = 1
      #########################################
      # post amp
      #########################################
      #
      ampblock = ampstg.appendDspBlock("AmpAdaptive","amp")
      ampblock.paramByName("gain").mods.src1 = ampenv
      ampblock.paramByName("gain").mods.src1depth = 1.0
      return newlyr

    if False:
      newprog = krz_bank.programByName("Doomsday")
      L0 = newprog.layer(0)
      L0.gain = -36 # dB
      S0 = L0.stage("DSP")
      para = S0.replaceDspBlock("EqParaBass","FilterHighPass","PANNER")
      panblock = S0.replaceDspBlock("AmpPanner","AmpPanner2D","PANNER")
      newprog.layer(1).gain = -96 #dB
      newprog.layer(2).gain = -96 #dB
      panlfo = L0.appendController("Lfo", "PANLFO")
      panlfo.properties.shape = "Saw"
      panlfo.properties.minRate = 0.125
      panlfo.properties.maxRate = 0.125
      panblock.paramByName("ANGLE").coarse=-1
      panblock.paramByName("ANGLE").mods.src1 = panlfo
      panblock.paramByName("ANGLE").mods.src1depth = 1
    else:
      makePMXLayer(0,0)
      makePMXLayer(12,0)
    #assert(False)
    ############################
    self.soundbank = self.new_soundbank
    ############################
    ok_list = [
      "PMX"
    ]
    ############################
    self.sorted_progs = sorted(ok_list)
    self.prog_index = find_index(self.sorted_progs, "PMX")
    self.synth.programbus.uiprogram = newprog
    print(self.prog_index)
    if self.pgmview:
      self.pgmview.setProgram(newprog)
  ##############################################

###############################################################################

AmbiApp().ezapp.mainThreadLoop()
