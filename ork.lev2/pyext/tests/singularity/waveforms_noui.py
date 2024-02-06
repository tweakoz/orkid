#!/usr/bin/env python3

################################################################################
# singularity test for editing layers in a soundbank
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import numpy as np
import sys, random, time
from orkengine.core import *
from orkengine.lev2 import *
from orkengine.lev2 import singularity as S

################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
sys.path.append((lev2exdir()/"python").normalized.as_string) # add orkid lev2 python extensions to path
from _boilerplate import *
from singularity._harness import SingulTestApp, find_index
from audio import nodata
tokens = CrcStringProxy()
################################################################################

lev2appinit()
audiodevice = singularity.device.instance()
synth = singularity.synth.instance()
synth.system_tempo = 120.0
sequencer = synth.sequencer

mainbus = synth.outputBus("main")
synth.setEffect(mainbus,"Reverb:GuyWire")
synth.masterGain = singularity.decibelsToLinear(12.0)
#synth.setEffect(mainbus,"none")

audiodata = nodata.NoDataSynthProgram(do_lfo=True,do_rez=True)
ok_list = [
  audiodata.prgname
]
############################
sorted_progs = sorted(ok_list)
prog_index = find_index(sorted_progs, audiodata.prgname)
synth.programbus.uiprogram = audiodata.patch
print(prog_index)
voice = synth.keyOn(36,127,audiodata.patch,None)
##############################################

###############################################################################

while True:
  synth.mainThreadHandler()
  time.sleep(0)
  
