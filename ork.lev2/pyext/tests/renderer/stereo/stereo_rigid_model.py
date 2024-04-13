#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

lev2_pyexdir.addToSysPath()
from common.cameras import *
from common.shaders import *
from common.misc import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')

################################################################################

args = vars(parser.parse_args())
numinstances = 500

################################################################################

class StereoApp1(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    self.cameralut = CameraDataLut()
    self.vrcamera = CameraData()
    self.cameralut.addCamera("vrcam",self.vrcamera)
    self.modelinsts=[]
    self.xf_hmd = Transform()

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    self.vrdev = orkidvr.novr_device()
    self.vrdev.camera = "vrcam"

    createSceneGraph(app=self,rendermodel="FWDPBRVR")

    model = XgmModel("data://tests/pbr_calib.glb")

    ###################################
    
    for i in range(numinstances):
      self.modelinsts += [modelinst(model,self.layer1,i)]

    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    return ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):

    ########################################
    # stereo viewing setup  
    ########################################

    self.vrdev.FOV = 90
    self.vrdev.IPD = 0.065
    self.vrdev.near = 0.1
    self.vrdev.far = 1e5
    
    self.vrcamera.perspective(.1,1e5,90)
    self.vrcamera.lookAt( 
      vec3(0,10,-1), # eye 
      vec3(0,10,0), # tgt
      vec3(0,1,0) # up
    )
    mtx_hmd = mtx4()
    mtx_hmd.setColumn(3,vec4(0,10,0,1))
    self.vrdev.setPoseMatrix("hmd",mtx_hmd.inverse)
    
    ########################################

    for minst in self.modelinsts:
      minst.update(updinfo.deltatime)

    self.scene.updateScene(self.cameralut) 

  def onGpuUpdate(self,ctx):
    # just need a mainthread python callback
    # so python can process ctrl-c signals...
    pass 

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
