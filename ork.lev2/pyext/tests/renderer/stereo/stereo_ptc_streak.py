#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, signal
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append(str(thisdir()/".."/"particles")) 

################################################################################

lev2_pyexdir.addToSysPath()
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

from _ptc_harness import *

################################################################################

class StereoApp1(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.cameralut = CameraDataLut()
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

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    createDefaultStreakSystem(app=self)

  ##############################################

  def onGpuUpdate(self,ctx):
    # just need a mainthread python callback
    # so python can process ctrl-c signals...
    pass 

  ################################################

  def onUpdate(self,updinfo):

    ########################################
    # stereo viewing setup  
    ########################################

    self.vrdev.FOV = math.radians(120)
    self.vrdev.IPD = 0.065
    self.vrdev.near = 0.1
    self.vrdev.far = 1e5
    
    self.xf_hmd.lookAt( vec3(0,10,-10)*1.5 # eye
                      , vec3(0,0,0) # tgt
                      , vec3(0,1,0) # up
                      )
    
    self.vrdev.setPoseMatrix("hmd",self.xf_hmd.composed)
    
    ########################################

    self.scene.updateScene(self.cameralut) 

  ##############################################

  def onUiEvent(self,uievent):
    return ui.HandlerResult()

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
