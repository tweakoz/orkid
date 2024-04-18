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
from common.lighting import MySpotLight, MyCookie

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
    self.ezapp = OrkEzApp.create(self,sssa=0)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    self.cameralut = CameraDataLut()
    self.vrcamera = CameraData()
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

    self.vrdev = orkidvr.novr_device()
    self.vrdev.camera = "vrcam"

    ###################################
    # create scenegraph
    ###################################

    params_dict = {
      "SkyboxTexPathStr": "src://envmaps/blender_studio.dds",
      "SkyboxIntensity": 1.5,
      "DiffuseIntensity": 1.0,
      "SpecularIntensity": 1.0,
      "AmbientLevel": vec3(.125),
      "DepthFogDistance": 10000.0,
    }
    if mono:
      params_dict["preset"] = "ForwardPBR"
    else:
      params_dict["preset"] = "FWDPBRVR"

    self.model = XgmModel("data://tests/chartest/char_mesh")
    self.anim = XgmAnim("data://tests/chartest/char_testanim1")

    self.anim_inst = XgmAnimInst(self.anim)
    self.anim_inst.mask.enableAll()
    self.anim_inst.use_temporal_lerp = True
    self.anim_inst.bindToSkeleton(self.model.skeleton)

    ##################
    # create model / sg node
    ##################

    createSceneGraph(app=self,params_dict=params_dict)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.layer1
    self.fwd_layers = [self.layer_fwd,self.layer_donly]
    #self.scenegraph = scenegraph.Scene(sg_params) << this does not work..
    #self.sgnode = self.model.createNode("modelnode",self.layer_fwd)
    self.model_drawable = self.model.createDrawable()
    self.sgnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"modelnode",self.model_drawable)
    self.modelinst = self.model_drawable.modelinst
    self.modelinst.enableSkinning()
    self.modelinst.enableAllMeshes()
    self.localpose = self.modelinst.localpose
    self.worldpose = self.modelinst.worldpose

    ###################################

    self.grid_data = createGridData()
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.texturepath = "src://effect_textures/white.dds"
    self.grid_data.modcolor = vec3(1)
    self.grid_node = self.layer_fwd.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    self.ball_model = XgmModel("data://tests/pbr_calib.glb")
    self.cookie1 = MyCookie("src://effect_textures/knob2.dds")

    shadow_size = 1024
    shadow_bias = 1e-3
    intens = 350
    self.spotlight1 = MySpotLight(app=self,
                                 model=self.ball_model,
                                 frq=0.3,
                                 color=vec3(intens,0,0),
                                 cookie=self.cookie1,
                                 radius=12,
                                 bias=shadow_bias,
                                 dim=shadow_size,
                                 fovamp=0,
                                 fovbase=65,
                                 voffset=16,
                                 vscale=12)

    self.spotlight2 = MySpotLight(app=self,
                                 model=self.ball_model,
                                 frq=0.7,
                                 color=vec3(0,intens,0),
                                 cookie=self.cookie1,
                                 radius=16,
                                 bias=shadow_bias,
                                 dim=shadow_size,
                                 fovamp=0,
                                 fovbase=85,
                                 voffset=17,
                                 vscale=10)

    self.spotlight3 = MySpotLight(app=self,
                                 model=self.ball_model,
                                 frq=0.9,
                                 color=vec3(0,0,intens),
                                 cookie=self.cookie1,
                                 radius=19,
                                 bias=shadow_bias,
                                 dim=shadow_size,
                                 fovamp=0,
                                 fovbase=95,
                                 voffset=20,
                                 vscale=10)

  ##############################################

  def onUiEvent(self,uievent):
    handled = False
    if mono:
      handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

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
    
    self.spotlight1.update(self.lighttime)
    self.spotlight2.update(self.lighttime)
    self.spotlight3.update(self.lighttime)

    self.localpose.bindPose()
    self.anim_inst.currentFrame = self.frame_index
    self.anim_inst.weight = 1.0
    self.anim_inst.applyToPose(self.localpose)
    self.localpose.blendPoses()
    self.localpose.concatenate()
    
    self.sgnode.worldTransform.translation = vec3(0,3,0)
    
    self.worldpose.fromLocalPose(self.localpose,mtx4())
    self.frame_index += 0.3
    pass 

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
