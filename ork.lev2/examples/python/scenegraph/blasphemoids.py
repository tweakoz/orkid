#!/usr/bin/env python3
################################################################################
# lev2 scenegraph sample which allows the individual donning the VR HMD to
#  blast away some blasphemoids
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
from ork import host
tokens = CrcStringProxy()
################################################################################
#from pathlib import Path as plpath
#this_dir = plpath(os.path.dirname(os.path.abspath(__file__)))
#sys.path.append(str(this_dir))
#import _simsetup
thispath_to_syspath() # add file directory to syspath
################################################################################
parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--numinstances', metavar="numinstances", help='number of mesh instances' )
################################################################################
args = vars(parser.parse_args())
if args["numinstances"]==None:
  numinstances = 5000
else:
  numinstances = int(args["numinstances"])
################################################################################
class instance_set_class(_simsetup.InstanceSet):
  def __init__(self,model,layer):
    super().__init__(model,numinstances,layer)
    ####################################
    # opencl setup
    # create rot and trans temporary cl buffers
    ####################################
    self.clkernel = _simsetup.ClKernel()
    self.res_r = cl.Buffer(self.clkernel.ctx, mf.WRITE_ONLY, self.instancematrices.nbytes)
    self.res_t = cl.Buffer(self.clkernel.ctx, mf.WRITE_ONLY, self.instancematrices.nbytes)
  ########################################################
  # update matrices with OpenCL
  ########################################################
  def update(self,deltatime):
    self.clupdate()
################################################################################
class Blasphemoids(_simsetup.SimApp):
  ################################################
  def __init__(self):
    super().__init__(True,instance_set_class)
    self.pickray = None
    self.stereo_material_inst = None
    self.timeparam = None
    self.abstime = 0.0
  ################################################
  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    ####################################
    # laser line
    ####################################
    volumetexture = Texture.load("lev2://textures/voltex_pn3")
    material = FreestyleMaterial(ctx,Path("orkshader://noise"))
    param_volumetex = material.shader.param("VolumeMap")
    param_v4parref = material.shader.param("testvec4")
    self.v4parref = vec4()
    stereo_material_inst = material.createFxInstance()
    stereo_material_inst.technique = material.shader.technique("std_stereo")
    stereo_material_inst.param[material.param("mvpL")] = tokens.RCFD_Camera_MVP_Left
    stereo_material_inst.param[material.param("mvpR")] = tokens.RCFD_Camera_MVP_Right
    stereo_material_inst.param[param_v4parref] = self.v4parref
    stereo_material_inst.param[param_volumetex] = volumetexture
    self.stereo_material_inst = stereo_material_inst
    self.timeparam = material.param("time")
    self.laser_a = vec3(0,0,0)
    self.laser_b = vec3(0,0,-100)
    self.layer.createLineNode("laserline",
                              self.laser_a,
                              self.laser_b,
                              stereo_material_inst)
    ##############################################
    # create hand model node
    ##############################################
    model = Model("data://tests/pbr1/pbr1.glb")
    self.handnode = model.createNode("handnode",self.layer)
    ##############################################
    # input setup
    ##############################################
    self.inputmgr = InputManager.instance()
    self.hands = self.inputmgr.inputGroup("hands")
  ################################################
  def onUpdate(self,updinfo):
    super().onUpdate(updinfo)
    self.abstime = updinfo.absolutetime
    #left = self.hands.channel("left.matrix")
    right = self.hands.channel("right.matrix")
    iset = self.instanceset
    hand = right # todo fix controller assignment
    if hand!=None:
      pos = vec3()
      rot = quat()
      sca = float(0)
      hand.decompose(pos,rot,sca)
      #####################################
      self.laser_a.set(pos)
      self.laser_b.set(pos+hand.yNormal()*10000)
      #####################################
      # place model on hand
      #####################################
      self.handnode\
          .worldMatrix\
          .compose( pos, # pos
                    rot, # rot
                    0.05) # scale
      #####################################
      # check for trigger, and fire!
      #####################################
      button = self.hands.channel("right.trigger")
      if button and \
         self.pickray==None and \
         self.lastbutton==False:
        ray = ray3(pos,hand.yNormal())
        self.pickray = ray
      # todo - add event gating...
      self.lastbutton = button
  ################################################
  def onDraw(self,drwev):
    ##########################
    # animate laser pointer
    ##########################
    self.stereo_material_inst.param[self.timeparam] = self.abstime*10
    ##########################
    # render scenegraph
    ##########################
    self.scene.renderOnContext(drwev.context)
    ##########################
    # handle picking
    ##########################
    if self.pickray:
      # picking must occur on mainthread, atm...
      picked = self.scene.pickWithRay(self.pickray)
      self.pickray = None
      if picked!=0xffffffffffffffff:
         ##########################
         # we picked a blasphemoid
         #  alter it's course,
         #   and color...
         ##########################
         if picked<numinstances:
             color = vec4(
               random.uniform(0,1),
               random.uniform(0,1),
               random.uniform(0,1),
               1)
             iset = self.instanceset
             iset.instancecolors[picked] = color
             incraxis = vec3(random.uniform(-1,1),
                             random.uniform(-1,1),
                             random.uniform(-1,1)).normal()
             incrmagn = random.uniform(-0.25,0.25)
             rot = quat(incraxis,incrmagn)
             as_mtx4 = mtx4()
             trans = vec3(random.uniform(-1,1),
                          random.uniform(-1,1),
                          random.uniform(-1,1))*0.03
             tramtx = mtx4.transMatrix(trans)
             rotmtx = mtx4.rotMatrix(rot)
             iset.delta_rots[picked]=rotmtx # copy into numpy block
             iset.delta_tras[picked]=tramtx # copy into numpy block
  ################################################
app = Blasphemoids()
app.qtapp.mainThreadLoop()
