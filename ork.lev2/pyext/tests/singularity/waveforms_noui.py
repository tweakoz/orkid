#!/usr/bin/env python3

################################################################################
# singularity minimal sound playback with no UI
# Copyright 1996-2024, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import sys, time
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((lev2exdir()/"python").normalized.as_string) # add orkid lev2 python extensions to path
from audio import nodata
################################################################################
lev2appinit()
audiodevice = singularity.device.instance()
synth = singularity.synth.instance()
mainbus = synth.outputBus("main")
synth.setEffect(mainbus,"Reverb:GuyWire") # none
synth.masterGain = singularity.decibelsToLinear(18.0)
audiodata = nodata.NoDataSynthProgram(do_lfo=True,do_rez=True)
synth.programbus.uiprogram = audiodata.patch
voice = synth.keyOn(36,127,audiodata.patch,None)
###############################################################################
# main loop
###############################################################################
while True:
  synth.mainThreadHandler()
  time.sleep(0)
  
