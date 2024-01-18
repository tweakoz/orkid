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
  
  #########################################
  # PAN2D
  #########################################
  def gen_pan_block(self,newlyr,panblock,arate=None,drate=None):
    ANGLE = panblock.paramByName("ANGLE")
    DISTANCE = panblock.paramByName("DISTANCE")
    #########################################
    # angle will just keep going up...
    #########################################
    angle_gradient = newlyr.appendController("Gradient", "angle_gradient")
    angle_gradient.properties.initial = 0.0
    angle_gradient.properties.slope = arate
    #
    ANGLE.coarse=0
    ANGLE.mods.src1 = angle_gradient
    ANGLE.mods.src1scale = 1.0
    ANGLE.mods.src1bias = -1.0
    #########################################
    # distance will follow a sine wave
    #########################################
    dist_lfo = newlyr.appendController("Lfo", "distanceLFO")
    dist_lfo.properties.shape = "Sine"
    dist_lfo.properties.minRate = drate
    dist_lfo.properties.maxRate = drate
    DISTANCE.coarse=0
    DISTANCE.mods.src1 = dist_lfo
    DISTANCE.mods.src1scale = 2.0
    DISTANCE.mods.src1bias = 3.0

  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    self.krzdata = singularity.KrzSynthData()
    krz_bank = self.krzdata.bankData
    self.synth.setEffect(self.mainbus,"none")
    self.mainbus.gain = 36
    self.octave = 2
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
    if False:
      newprog = krz_bank.programByName("Doomsday")
      L0 = newprog.layer(0)
      S0 = L0.stage("DSP")
      L1 = newprog.layer(1)
      S1 = L1.stage("DSP")
      L2 = newprog.layer(2)
      S2 = L2.stage("DSP")
      L0.gain = -96 #-12
      L1.gain = -96
      L2.gain = -12
      x = S2.replaceDspBlock("FilterNotch","AmpAdaptive","X")
      panblock = S2.replaceDspBlock("AmpPanner","AmpPanner2D","PANNER")
      self.gen_pan_block(L2,panblock,arate=1.7,drate=0.1)
    else:
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
        modblock.paramByName("pitch").coarse = 1.0
        modblock.paramByName("amp").coarse = 0.0
        modblock.paramByName("amp").fine = 0.0
        modblock.paramByName("amp").mods.src1 = modenv
        modblock.paramByName("amp").mods.src1scale = 2
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
        #########################################
        # post amp
        #########################################
        #
        panblock = dspstg.appendDspBlock("AmpPanner2D","pan")
        self.gen_pan_block(newlyr,panblock,arate=1.0,drate=4.0)
        #
        ampblock = ampstg.appendDspBlock("AmpAdaptive","amp")
        ampblock.paramByName("gain").mods.src1 = ampenv
        ampblock.paramByName("gain").mods.src1scale = 1.0
        return newlyr
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
