#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, signal, argparse
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append(str(thisdir()/".."/"particles")) 

parser = argparse.ArgumentParser(description='ptc harness')
parser.add_argument("-f", "--fullscreen", action='store_true', help='fullscreen')
args = vars(parser.parse_args())
fullscreen = args["fullscreen"]

################################################################################

lev2_pyexdir.addToSysPath()
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

from _ptc_harness import *

################################################################################

class StereoApp1(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,fullscreen=fullscreen)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.cameralut = CameraDataLut()
    setupUiCamera(app=self,eye=vec3(0,12,15))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    self.vrdev = orkidvr.novr_device()
    self.vrdev.camera = "vrcam"
    self.vrdev.width = 512
    self.vrdev.height = 512
    self.IVP = mtx4()

    params_dict = {
      "SkyboxIntensity" : float(1.5),
      "DiffuseIntensity" : float(6),
    }     

    createSceneGraph(app=self,rendermodel="FWDPBRVRDM",params_dict=params_dict)
    onode = self.outputnode # created by createSceneGraph
    onode.flipY = True

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createDrawableNodeFromData("grid",self.grid_data)
    self.grid_node.sortkey = 1

    createDefaultSpriteSystem(app=self)

  ################################################

  def onUpdate(self,updinfo):

    abstime = updinfo.absolutetime

    ########################################
    # stereo viewing setup  
    ########################################

    self.vrdev.FOVD = 90
    self.vrdev.IPD = 0.065
    self.vrdev.near = 0.1
    self.vrdev.far = 1e5
    
    x = math.sin(abstime*0.125)
    z = -math.cos(abstime*0.125)

    xf_hmd = mtx4.lookAt( vec3(x,0.1,z)*10.0,   # eye
                          vec3(0,0,0),     # tgt
                          vec3(0,1,0)      # up
                        )     
    
    self.vrdev.setPoseMatrix("hmd",xf_hmd)
    
    ########################################

    self.scene.updateScene(self.cameralut) 

  ##############################################

  def onGpuUpdate(self,ctx):
    # just need a mainthread python callback
    # so python can process ctrl-c signals...
    pass 

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
