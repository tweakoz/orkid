#!/usr/bin/env python3
################################################################################
# lev2 sample which renders an instanced model, optionally in VR mode
#  the models are animated via a OpenCL kernel
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import math, random, argparse, os, sys
#import numpy as np
#from scipy import linalg as la
from orkengine.core import *
from orkengine.lev2 import *
################################################################################
sys.path.append((thisdir()).normalized.as_string)
import _simsetup
################################################################################
parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--numinstances', metavar="numinstances", help='number of mesh instances' )
parser.add_argument('--vrmode', action="store_true", help='run in vr' )
parser.add_argument('--cldev', metavar="cldev", help='OpenCL device #' )
################################################################################
args = vars(parser.parse_args())
vrmode = (args["vrmode"]==True)
if args["numinstances"]==None:
  numinstances = 10000
else:
  numinstances = int(args["numinstances"])
if args["cldev"]==None:
  os.environ["PYOPENCL_CTX"]='0'
else:
  os.environ["PYOPENCL_CTX"]=args["cldev"]
################################################################################
import pyopencl as cl
mf = cl.mem_flags
################################################################################
class instance_set_class(_simsetup.InstanceSet):
  def __init__(self,model,layer):
    super().__init__(model,numinstances,layer)
    self.clkernel = _simsetup.ClKernel()
    # opencl setup
    self.res_r = cl.Buffer(self.clkernel.ctx, mf.WRITE_ONLY, self.instancematrices.nbytes)
    self.res_t = cl.Buffer(self.clkernel.ctx, mf.WRITE_ONLY, self.instancematrices.nbytes)
  ########################################################
  # update matrices with OpenCL
  ########################################################
  def update(self,deltatime):
    self.clupdate(dt=deltatime)
    index = random.randint(0,numinstances-1)
    color = vec4(random.uniform(0,1),
                 random.uniform(0,1),
                 random.uniform(0,1),
                 1)
    self.instancecolors[index] = color
################################################################################
class OpenClSimApp(_simsetup.SimApp):
  ################################################
  def __init__(self):
    super().__init__(vrmode,instance_set_class)
################################################
OpenClSimApp().ezapp.mainThreadLoop()
