#!/usr/bin/env ork.python

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
################################################################################

args = vars(parser.parse_args())

################################################################################

class ImposterApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self,ssaa=0)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    #self.cameralut = lev2.CameraDataLut()
    #self.cameralut.addCamera("vrcam",self.vrcamera)
    self.xf_hmd = Transform()
    self.frame_index = 0

    setupUiCamera(app=self,eye=vec3(0,1,1)*25,tgt=vec3(0,10,0))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):


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
    params_dict["preset"] = "ForwardPBR"

    ##################
    # create model / sg node
    ##################

    createSceneGraph(app=self,params_dict=params_dict)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.layer1
    self.fwd_layers = [self.layer_fwd,self.layer_donly]

    ###################################

    self.grid_data = createGridData()

    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(1)*3
    self.grid_data.majorTileDim = 8.0
    self.grid_node = self.layer_fwd.createDrawableNodeFromData("grid",self.grid_data)
    self.grid_node.sortkey = 1

    self.ball_model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.cookie1 = MyCookie("src://effect_textures/knob2.png")

  ##############################################

    self.imp_data = lev2.ImposterDrawableData()
    self.grid_node = self.layer_fwd.createDrawableNodeFromData("imp1",self.imp_data)

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):
    self.time = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) 

  def onGpuUpdate(self,ctx):
    self.frame_index += 0.3
    pass 

###############################################################################

ImposterApp().ezapp.mainThreadLoop()
