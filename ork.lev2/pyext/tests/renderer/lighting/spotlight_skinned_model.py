#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import vec3, vec4, quat, mtx4, lev2_pyexdir, Transform
from orkengine import lev2

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

    #self.vrdev = lev2.orkidvr.novr_device()
    #self.vrdev.camera = "vrcam"

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

    self.model = lev2.XgmModel("data://tests/chartest/char_mesh")
    self.anim = lev2.XgmAnim("data://tests/chartest/char_testanim1")

    self.anim_inst = lev2.XgmAnimInst(self.anim)
    self.anim_inst.mask.enableAll()
    self.anim_inst.use_temporal_lerp = True
    self.anim_inst.bindToSkeleton(self.model.skeleton)

    ##################
    for mesh in self.model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.baseColor = vec4(1,.5,1,1)
        copy.metallicFactor = 0.0
        copy.roughnessFactor = 1.0
        copy.assignImages(
          ctx,
          doConform=True
        )
        submesh.material = copy

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
    self.grid_data.modcolor = vec3(2)
    self.grid_data.intensityA = 1.0
    self.grid_data.intensityB = 0.97
    self.grid_data.intensityC = 0.9
    self.grid_data.intensityD = 0.85
    self.grid_data.lineWidth = 0.1
    self.grid_node = self.layer_fwd.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    self.ball_model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.cookie1 = MyCookie("src://effect_textures/knob2.png")

    shadow_size = 4096
    shadow_bias = 1e-3
    intens = 450
    self.spotlight1 = MySpotLight(app=self,
                                 model=self.ball_model,
                                 frq=0.3,
                                 color=vec3(intens,0,0),
                                 cookie=self.cookie1,
                                 radius=12,
                                 bias=shadow_bias,
                                 dim=shadow_size,
                                 fovamp=0,
                                 fovbase=45,
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
                                 fovbase=65,
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
                                 fovbase=75,
                                 voffset=20,
                                 vscale=10)

  ##############################################

  def onUiEvent(self,uievent):
    handled = False
    if mono:
      handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):
    self.lighttime = updinfo.absolutetime
    
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
