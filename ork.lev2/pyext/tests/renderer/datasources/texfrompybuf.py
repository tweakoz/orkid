#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
import numpy as np
from orkengine.core import vec3, vec4, quat, mtx4, Transform
from orkengine.core import CrcStringProxy, lev2_pyexdir
from orkengine import lev2
        
tokens = CrcStringProxy()
        
################################################################################

lev2_pyexdir.addToSysPath()

from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--stereo', action='store_true', help='stereo mode')
################################################################################

args = vars(parser.parse_args())

stereo = args["stereo"]
mono = not stereo

################################################################################

class StereoApp1(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self,ssaa=2)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.cameralut = lev2.CameraDataLut()
    self.vrcamera = lev2.CameraData()
    self.cameralut.addCamera("vrcam",self.vrcamera)
    self.xf_hmd = Transform()

    if mono:
      setupUiCamera(app=self,eye=vec3(0,1,1)*25,tgt=vec3(0,10,0))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    self.frame_index = 0

    self.vrdev = lev2.orkidvr.novr_device()
    self.vrdev.camera = "vrcam"

    ###################################
    # create scenegraph
    ###################################

    params_dict = {
      "SkyboxTexPathStr": "src://envmaps/blender_studio.dds",
      "SkyboxIntensity": 1.5,
      "DiffuseIntensity": 1.0,
      "SpecularIntensity": 1.0,
      "AmbientLevel": vec3(0),
      "DepthFogDistance": 10000.0,
    }
    if mono:
      params_dict["preset"] = "ForwardPBR"
    else:
      params_dict["preset"] = "FWDPBRVR"

    ##################
    # create model / sg node
    ##################

    createSceneGraph(app=self,params_dict=params_dict)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.layer1
    self.fwd_layers = [self.layer_fwd,self.layer_donly]

    ###################################
    
    img_dim = 1024
    np_img = np.random.randint(0, 255, size=(img_dim, img_dim, 3), dtype = np.uint8)
    print(np_img)
    
    ###################################

    self.grid_data = createGridData()
    self.grid_data.colorImage = lev2.Image.createFromBuffer( img_dim,
                                                             img_dim,
                                                             tokens.RGB8,
                                                             np_img)

    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(2)
    self.grid_data.intensityA = 1.0
    self.grid_data.intensityB = 0.97
    self.grid_data.intensityC = 0.9
    self.grid_data.intensityD = 0.85
    self.grid_data.lineWidth = 0.1
    self.grid_node = self.layer_fwd.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):
    self.lighttime = updinfo.absolutetime

    ########################################
    # stereo viewing setup  
    ########################################

    self.vrdev.FOV = 90
    self.vrdev.IPD = 0.065
    self.vrdev.near = 0.1
    self.vrdev.far = 1e5
    
    #self.vrcamera.perspective(.1,1e5,90)
    #self.vrcamera.lookAt( 
    #  vec3(0,10,-1), # eye 
    #  vec3(0,10,0), # tgt
    #  vec3(0,1,0) # up
    #)
    mtx_hmd = mtx4()
    mtx_hmd.setColumn(3,vec4(0,5,10,1))
    self.vrdev.setPoseMatrix("hmd",mtx_hmd.inverse)
    
    ########################################

    #for minst in self.modelinsts:
    #  minst.update(updinfo.deltatime)

    self.scene.updateScene(self.cameralut) 

  def onGpuUpdate(self,ctx):
    self.frame_index += 0.3
    pass 

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
