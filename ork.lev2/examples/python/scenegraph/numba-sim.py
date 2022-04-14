#!/usr/bin/env python3
################################################################################
# lev2 sample which renders an instanced model, optionally in VR mode
#  the models are animated via a numba jitified python function
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math, random, argparse, os, sys
import numpy as np
from scipy import linalg as la
from numba import jit
from orkengine.core import *
from orkengine.lev2 import *
################################################################################
from pathlib import Path
this_dir = Path(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(str(this_dir))
import _simsetup
################################################################################
parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--numinstances', metavar="numinstances", help='number of mesh instances' )
parser.add_argument('--vrmode', action="store_true", help='run in vr' )
################################################################################
args = vars(parser.parse_args())
vrmode = (args["vrmode"]==True)
if args["numinstances"]==None:
  numinstances = 10000
else:
  numinstances = int(args["numinstances"])
################################################################################
#@vectorize(['float32(float32, float32)'], target='cuda')
@jit(nopython=True,parallel=True)
def matrix_update(curmatrices, deltas):
  for i in range(0,numinstances):
    a = curmatrices[i]
    b = deltas[i]
    curmatrices[i]=b.dot(a)
################################################################################
class instance_set_class(_simsetup.InstanceSet):
  ########################################################
  def __init__(self,model,layer):
    super().__init__(model,numinstances,layer)
  ########################################################
  def update(self,deltatime):
    matrix_update(self.instancematrices,self.deltas)
################################################################################
class NumbaSimApp(_simsetup.SimApp):
  ################################################
  def __init__(self):
    super().__init__(vrmode,instance_set_class)
################################################
app = NumbaSimApp()
app.qtapp.mainThreadLoop()
