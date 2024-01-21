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
    self.nextstep = 1
    self.step = 0
    self.clickcnt = 0
    self.basscnt = 0
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
    self.synth.setEffect(self.mainbus,"ShifterChorus")
    self.mainbus.gain = 30

    L0 = singularity.LayerData()
    S0 = L0.appendStage("PAN")
    S0.ioconfig.inputs = [0,1]
    S0.ioconfig.outputs = [0,1]
    self.aux0panblock = S0.appendDspBlock("AmpPanner2D","PANNER")
    self.auxbusses[0].gain = 0
    self.auxbusses[0].layer = L0

    L0 = singularity.LayerData()
    S0 = L0.appendStage("PAN")
    S0.ioconfig.inputs = [0,1]
    S0.ioconfig.outputs = [0,1]
    self.aux1panblock = S0.appendDspBlock("AmpPanner2D","PANNER")
    self.auxbusses[1].gain = -6
    self.auxbusses[1].layer = L0

    L0 = singularity.LayerData()
    S0 = L0.appendStage("PAN")
    S0.ioconfig.inputs = [0,1]
    S0.ioconfig.outputs = [0,1]
    self.aux2panblock = S0.appendDspBlock("AmpPanner2D","PANNER")
    self.auxbusses[2].gain = -6
    self.auxbusses[2].layer = L0

    L0 = singularity.LayerData()
    S0 = L0.appendStage("PAN")
    S0.ioconfig.inputs = [0,1]
    S0.ioconfig.outputs = [0,1]
    self.aux7panblock = S0.appendDspBlock("AmpPanner2D","PANNER")
    self.auxbusses[7].gain = 18
    self.auxbusses[7].layer = L0


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
    self.click = krz_bank.programByName("Click")
    self.hirez = krz_bank.programByName("Hi_Res_Sweeper")
    self.doomsday = krz_bank.programByName("Doomsday")
    self.bass = krz_bank.programByName("WonderSynth_Bass")
    ############################
    L0 = self.doomsday.layer(0)
    S0 = L0.stage("DSP")
    L1 = self.doomsday.layer(1)
    S1 = L1.stage("DSP")
    L2 = self.doomsday.layer(2)
    S2 = L2.stage("DSP")
    panblock = S2.replaceDspBlock("AmpPanner","AmpPanner2D","PANNER")
    self.gen_pan_block(L2,panblock,arate=1.7,drate=0.1)
    #S2.appendPitchChorus(L2,0.25,50.0,0.5)
    ############################
    L0.gain = -96 #-12
    L1.gain = -96
    L2.gain = -12
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
  def onUpdate(self,updev):
    self.time = updev.absolutetime
    return super().onUpdate(updev)
  ##############################################
  def onGpuUpdate(self, ctx):
    self.aux0panblock.paramByName("ANGLE").coarse = self.time*1.4
    self.aux0panblock.paramByName("DISTANCE").coarse = math.sin(self.time)+2
    self.aux1panblock.paramByName("ANGLE").coarse = self.time*-1.7
    self.aux1panblock.paramByName("DISTANCE").coarse = math.sin(self.time*3)+2
    ########################################
    # click
    ########################################
    if int(self.time*2)>self.clickcnt:
      self.clickcnt += 1
      x = random.randrange(-100,100)*0.01
      y = random.randrange(-100,100)*0.01
      self.aux7panblock.paramByName("ANGLE").coarse = self.clickcnt*0.5
      self.aux7panblock.paramByName("DISTANCE").coarse = 1

      if len(self.voices)>=10!=None:
        self.synth.keyOff(self.voices[2],48,127)
      kmod = singularity.KeyOnModifiers()
      kmod.outputbus = self.auxbusses[7]
      self.voices[10] = self.synth.keyOn(48,127,self.click,kmod)
    ########################################
    # bass
    ########################################
    if int(self.time*0.5) and self.basscnt==0:
      self.basscnt = 1
      x = random.randrange(-100,100)*0.01
      y = random.randrange(-100,100)*0.01
      self.aux2panblock.paramByName("ANGLE").coarse = self.basscnt*0.5
      self.aux2panblock.paramByName("DISTANCE").coarse = 1

      if len(self.voices)>=9!=None:
        self.synth.keyOff(self.voices[9],48,127)
      kmod = singularity.KeyOnModifiers()
      kmod.outputbus = self.auxbusses[2]
      self.voices[9] = self.synth.keyOn(36,127,self.bass,kmod)
    ########################################
    if self.step==0:
      if(self.time>2):
        self.prog = self.doomsday
        self.voices[0] = self.synth.keyOn(48,127,self.doomsday,None)
        self.step = 1
    elif self.step==1:
      if(self.time>4):
        kmod = singularity.KeyOnModifiers()
        kmod.outputbus = self.auxbusses[0]
        self.voices[1] = self.synth.keyOn(48,127,self.hirez,kmod)
        self.step = 2
    elif self.step==2:
      if(self.time>6):
        kmod = singularity.KeyOnModifiers()
        kmod.outputbus = self.auxbusses[1]
        self.voices[2] = self.synth.keyOn(84,127,self.hirez,kmod)
        self.step = 3
    elif self.step==3:
      if(self.time>13.5):
        self.voices[3] = self.synth.keyOn(72,127,self.doomsday,None)
        self.step = 4
    elif self.step==4:
      if(self.time>8):
        self.voices[4] = self.synth.keyOn(60,127,self.doomsday,None)
        self.step = 5
    elif self.step==5:
      if(self.time>13.5):
        self.voices[5] = self.synth.keyOn(72,127,self.doomsday,None)
        self.step = 6


    return super().onGpuUpdate(ctx)   
###############################################################################

AmbiApp().ezapp.mainThreadLoop()
