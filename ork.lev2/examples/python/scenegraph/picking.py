#!/usr/bin/env python3
################################################################################
# lev2 sample which renders an instanced model, optionally in VR mode
#  the models are animated via a OpenCL kernel
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import math, random, argparse, os, sys
import numpy as np
from scipy import linalg as la
import pyopencl as cl
mf = cl.mem_flags
from orkengine.core import *
from orkengine.lev2 import *
from obt import host
################################################################################
from pathlib import Path
this_dir = Path(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(str(this_dir))
import _simsetup
################################################################################
parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--numinstances', metavar="numinstances", help='number of mesh instances' )
################################################################################
args = vars(parser.parse_args())
if args["numinstances"]==None:
  numinstances = 50000
else:
  numinstances = int(args["numinstances"])
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
    self.clupdate()
    ############################################
    #assert(False)
################################################################################
class PickingApp(_simsetup.SimApp):
  ################################################
  def __init__(self):
    super().__init__(False,instance_set_class)
  def onUiEvent(self,event):
    #print("x<%d> y<%d> code<%d>"%(event.x,event.y,event.code))
    #print("shift<%d> alt<%d> ctrl<%d>"%(event.shift,event.alt,event.ctrl))
    #print("left<%d> middle<%d> right<%d>"%(event.left,event.middle,event.right))
    if True: #event.code==3:
      def pick_callback(pixel_fetch_context):
        obj = pixel_fetch_context.value(0)
        picked = 0
        if picked!=0xffffffffffffffff:
          #print("%s"%(hex(picked)))
          assert(picked<=numinstances);
          color = vec4(random.uniform(0,1),
                      random.uniform(0,1),
                      random.uniform(0,1),
                      1)
          iset = self.instanceset
          iset.instancecolors[picked] = color
      self.scene.pickWithScreenCoord(self.camera,vec2(event.x,event.y),pick_callback)
    return ui.HandlerResult()
  ################################################
app = PickingApp()
app.ezapp.mainThreadLoop()
