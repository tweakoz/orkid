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
parser.add_argument("--variant", type=int, default=1, help='grid shader variant (1-3)')
################################################################################

args = vars(parser.parse_args())
variant = args["variant"]
################################################################################

class StereoApp1(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
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

    ###################################

    self.grid_data = createGridData()
    if variant == 1:
      self.grid_data.shader_suffix = ""
    elif variant == 2:
      self.grid_data.shader_suffix = "_V2"
    elif variant == 3:
      self.grid_data.shader_suffix = "_V3"
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

    self.xf_hmd.lookAt( vec3(0,10,-10)*1.5 # eye
                      , vec3(0,0,0) # tgt
                      , vec3(0,1,0) # up
                      )
    
    self.vrdev.setPoseMatrix("hmd",self.xf_hmd.composed)
    
    ########################################

    self.scene.updateScene(self.cameralut) 

  def onGpuUpdate(self,ctx):
    # just need a mainthread python callback
    # so python can process ctrl-c signals...
    pass 

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
