#!/usr/bin/env python3
################################################################################
# lev2 sample which renders an instanced model, optionally in VR mode
#  the models are animated via a numba jitified python function
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import math, random, argparse, os, sys
from orkengine.core import *
from orkengine.lev2 import *
################################################################################
sys.path.append((thisdir()).normalized.as_string)
import _simsetup
################################################################################
parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--numinstances', metavar="numinstances", help='number of mesh instances' )
parser.add_argument('--vrmode', action="store_true", help='run in vr' )
################################################################################
args = vars(parser.parse_args())
vrmode = False #(args["vrmode"]==True)
if args["numinstances"]==None:
  numinstances = 10000
else:
  numinstances = int(args["numinstances"])
################################################################################
import numpy as np
from scipy import linalg as la
from numba import jit
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
    matrix_update(self.instancematrices,self.delta_rots)
    matrix_update(self.instancematrices,self.delta_tras)
    index = random.randint(0,numinstances-1)
    self.instancecolors[index] = color
################################################################################
class NumbaSimApp(_simsetup.SimApp):
  ################################################
  def __init__(self):
    super().__init__(vrmode,instance_set_class)
###############################################################################
NumbaSimApp().ezapp.mainThreadLoop()
