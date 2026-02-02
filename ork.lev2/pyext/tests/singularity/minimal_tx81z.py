#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, random, numpy, obt.path, time, bisect, argparse
import plotly.graph_objects as go
from collections import defaultdict
import re
from orkengine.core import *
from orkengine.lev2 import *

################################################################################
# Parse arguments
################################################################################
parser = argparse.ArgumentParser(description='TX81Z Minimal Test')
parser.add_argument('--newui', action='store_true', help='Use new DAW-style mixer UI')
args = parser.parse_args()

################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
################################################################################

if args.newui:
    ############################################################################
    # NEW UI PATH
    ############################################################################
    from singularity._daw_mixerview import SingulTestAppNewUI

    class Tx81zApp(SingulTestAppNewUI):

        def __init__(self):
            super().__init__()

        def onGpuInit(self, ctx):
            super().onGpuInit(ctx)
            self._load_tx81z_data()

        def _load_tx81z_data(self):
            """Load TX81Z soundbank and set up programs."""
            self.syn_data_base = singularity.baseDataPath()/"tx81z"
            self.txdata = singularity.Tx81zSynthData()
            self.txdata.loadBank("bank1", self.syn_data_base/"tx81z_1.syx")
            self.txdata.loadBank("bank2", self.syn_data_base/"tx81z_2.syx")
            self.txdata.loadBank("bank3", self.syn_data_base/"tx81z_3.syx")
            self.txdata.loadBank("bank4", self.syn_data_base/"tx81z_4.syx")
            self.soundbank = self.txdata.bankData
            self.txprogs = self.soundbank.programsByName
            self.sorted_progs = sorted(self.txprogs.keys())
            print("txprogs<%s>" % self.txprogs)
            self.prog_index = 0

            # Set program list in mixer view
            self.mixer_view.set_programs(self.sorted_progs)

            # Set initial program if available
            if self.sorted_progs:
                self.prog = self.soundbank.programByName(self.sorted_progs[0])
                self.setUiProgram(self.prog)
                main = self.synth.outputBus("main")
                self.setBusProgram(main, self.prog)

else:
    ############################################################################
    # OLD UI PATH (unchanged - no regression)
    ############################################################################
    from singularity._harness import SingulTestApp, find_index

    class Tx81zApp(SingulTestApp):

        def __init__(self):
            super().__init__()

        def onGpuInit(self, ctx):
            super().onGpuInit(ctx)
            self.syn_data_base = singularity.baseDataPath()/"tx81z"
            self.txdata = singularity.Tx81zSynthData()
            self.txdata.loadBank("bank1", self.syn_data_base/"tx81z_1.syx")
            self.txdata.loadBank("bank2", self.syn_data_base/"tx81z_2.syx")
            self.txdata.loadBank("bank3", self.syn_data_base/"tx81z_3.syx")
            self.txdata.loadBank("bank4", self.syn_data_base/"tx81z_4.syx")
            self.soundbank = self.txdata.bankData
            self.txprogs = self.soundbank.programsByName
            self.sorted_progs = sorted(self.txprogs.keys())
            print("txprogs<%s>" % self.txprogs)
            self.prog_index = 0

###############################################################################

app = Tx81zApp()
app.ezapp.mainThreadLoop()
