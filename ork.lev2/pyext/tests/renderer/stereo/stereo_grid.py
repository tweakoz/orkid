#!/usr/bin/env ork.python

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
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.misc import *
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("--variant", type=int, default=0, help='grid shader variant (1-3)')
################################################################################

args = vars(parser.parse_args())
variant = args["variant"]
################################################################################

class StereoApp1(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,fullscreen=False)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    self.cameralut = CameraDataLut()
    self.xf_hmd = Transform()
    setupUiCamera(app=self,eye=vec3(0,12,15))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    self.vrdev = orkidvr.novr_device()
    self.vrdev.camera = "vrcam"
    self.vrdev.width = 1024
    self.vrdev.height = 256
    self.IVP = mtx4()
    
    params_dict = {
      "SkyboxIntensity" : float(1.5),
      "DiffuseIntensity" : float(6),
    }     
    createSceneGraph(app=self,rendermodel="FWDPBRVRDM",params_dict=params_dict)    
    onode = self.outputnode # created by createSceneGraph
    def onCameraChange(cdd):
      eyeindex = cdd.rendererProperty(tokens.eyeindex)
      viewdata = cdd.viewdata
      self.IVP = viewdata.IVPM # mono IVP
      print(f"eyeindex: {eyeindex} IVP {self.IVP}")
    onode.onCameraChange(lambda cdd: onCameraChange(cdd))
    onode.flipY = False
    
    ###################################

    self.grid_data = createGridData()
    if variant == 1:
      self.grid_data.shader_suffix = ""
    elif variant == 2:
      self.grid_data.shader_suffix = "_V2"
    elif variant == 3:
      self.grid_data.shader_suffix = "_V3"
    elif variant == 4:
      self.grid_data.shader_suffix = "_V4"
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):

    abstime = updinfo.absolutetime
    
    ########################################
    # stereo viewing setup  
    ########################################

    # projection matrix
    self.vrdev.FOV = 90    # degrees
    self.vrdev.IPD = 0.065 # meters
    self.vrdev.near = 0.1  # meters
    self.vrdev.far = 1e5   # meters

    x = math.sin(abstime*0.5)
    z = -math.cos(abstime*0.5)
    self.xf_hmd.lookAt( vec3(x,1,z)*-5,   # eye
                        vec3(0,0,0),     # tgt
                        vec3(0,1,0)      # up
                      )     

    self.vrdev.setPoseMatrix("hmd",self.xf_hmd.composed)
    
    ########################################

    self.scene.updateScene(self.cameralut) 
    
  ################################################

  def onGpuUpdate(self,ctx):
    # just need a mainthread python callback
    # so python can process ctrl-c signals...
    pass 

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
