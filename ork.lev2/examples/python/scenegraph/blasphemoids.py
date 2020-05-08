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
################################################################################
args = vars(parser.parse_args())
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
class Blasphemoids(_simsetup.SimApp):
  ################################################
  def __init__(self):
    super().__init__(True,instance_set_class)
    self.pickray = None
  ################################################
  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    model = Model("data://tests/pbr1/pbr1.glb")
    self.handnode = model.createNode("handnode",self.layer)
  ################################################
  def onUpdate(self,updinfo):
    super().onUpdate(updinfo)
    inputmgr = InputManager.instance()
    hands = inputmgr.inputGroup("hands")
    left = hands.channel("left.matrix")
    right = hands.channel("right.matrix")
    iset = self.instanceset
    hand = right
    if hand!=None:
      pos = vec3()
      rot = quat()
      sca = float(0)
      hand.decompose(pos,rot,sca)
      #####################################
      # put model on hand
      #####################################
      self.handnode\
          .worldMatrix\
          .compose( pos, # pos
                    rot, # rot
                    0.05) # scale
      #####################################
      # check for trigger, and fire!
      #####################################
      button = hands.channel("right.trigger")
      if button and \
         self.pickray==None and \
         self.lastbutton==False:
        ray = ray3(pos,hand.yNormal())
        self.pickray = ray
      # todo - add event gating...
      self.lastbutton = button
  ################################################
  def onDraw(self,drwev):
    self.scene.renderOnContext(drwev.context)
    if self.pickray:
      # picking must occur on mainthread, atm...
      picked = self.scene.pickWithRay(self.pickray)
      self.pickray = None
      if picked!=0xffffffffffffffff:
         print("%s"%(hex(picked)))
         assert(picked<=numinstances);
         color = vec4(random.uniform(0,1),
                      random.uniform(0,1),
                      random.uniform(0,1),
                      1)
         iset = self.instanceset
         iset.instancecolors[picked] = color
  ################################################
app = Blasphemoids()
app.qtapp.exec()
