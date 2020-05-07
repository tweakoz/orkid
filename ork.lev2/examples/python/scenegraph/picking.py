#!/usr/bin/env python3
################################################################################
# lev2 sample which renders an instanced model, optionally in VR mode
#  the models are animated via a OpenCL kernel
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math, random, argparse, os, sys
import numpy as np
from scipy import linalg as la
import pyopencl as cl
mf = cl.mem_flags
from orkengine.core import *
from orkengine.lev2 import *
from ork import host
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
class instance_set_class(_simsetup.InstanceSet):
  def __init__(self,model,layer):
    super().__init__(model,numinstances,layer)
    self.clkernel = _simsetup.ClKernel()
    # opencl setup
    self.res_g = cl.Buffer(self.clkernel.ctx, mf.WRITE_ONLY, self.instancematrices.nbytes)
  ########################################################
  # update matrices with OpenCL
  ########################################################
  def update(self,deltatime):
    current = cl.Buffer(self.clkernel.ctx, mf.READ_ONLY | mf.COPY_HOST_PTR, hostbuf=self.instancematrices)
    delta = cl.Buffer(self.clkernel.ctx, mf.READ_ONLY | mf.COPY_HOST_PTR, hostbuf=self.deltas)
    globalsize = (numinstances,1,1)
    localsize = None
    self.clkernel.prg.cl_concatenate_mtx4(self.clkernel.queue, globalsize, localsize, current, delta, self.res_g)
    cl.enqueue_copy(self.clkernel.queue, self.instancematrices, self.res_g)
################################################################################
class PickingApp(_simsetup.SimApp):
  ################################################
  def __init__(self):
    super().__init__(vrmode,instance_set_class)
  def onUiEvent(self,event):
    #print("x<%d> y<%d> code<%d>"%(event.x,event.y,event.code))
    #print("shift<%d> alt<%d> ctrl<%d>"%(event.shift,event.alt,event.ctrl))
    #print("left<%d> middle<%d> right<%d>"%(event.left,event.middle,event.right))
    #assert(self.scene)
    ray = ray3(vec3(0,0,0),vec3(0,0,-1))
    picked = self.scene.pickWithRay(ray)
    print("%s"%hex(picked))
  ################################################
app = PickingApp()
app.qtapp.exec()
