#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import vec3, vec4, quat, mtx4, lev2_pyexdir, Transform, CrcStringProxy
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
    self.ezapp = lev2.OrkEzApp.create(self,ssaa=1)
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
    self.grid_data.modcolor = vec3(1)
    self.grid_data.intensityA = 1.0
    self.grid_data.intensityB = 0.97
    self.grid_data.intensityC = 0.9
    self.grid_data.intensityD = 0.85
    self.grid_data.lineWidth = 0.1
    self.grid_node = self.layer_fwd.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    self.ball_model = lev2.XgmModel("data://tests/pbr_calib.glb")

    COLOR_LAYERS = [self.layer_fwd]
    NUM_SPOTS = 16

    lmgr = self.scene.lightingmanager
    color_cookies = lmgr.spot_cookies_color
    depth_cookies = lmgr.spot_cookies_depth
    color_cookies.needsIrradianceCache = True
    color_cookies.resize(1024,1024,NUM_SPOTS,tokens.RGB8,True)
    depth_cookies.resize(1024,1024,NUM_SPOTS,tokens.Z32F,True)

    texset = ["knob2", "L0D"]
    cookie_paths = []
    for i in range(NUM_SPOTS):
      val = random.randint(0,10)
      index = (val<3)
      cookie_paths.append("src://effect_textures/%s.png"%texset[index])
    ccooks = [color_cookies.load(path) for path in cookie_paths]
    dcooks = [depth_cookies.slice(i) for i in range(NUM_SPOTS)]
    colors = [vec3.fromHsv(i/NUM_SPOTS,1.0,250.0) for i in range(NUM_SPOTS)] 
    indices = [i for i in range(NUM_SPOTS)]
    frqs = [random.uniform(-0.4,0.4) for i in range(NUM_SPOTS)]
    fovbases = [random.uniform(25,45) for i in range(NUM_SPOTS)]
    fovamps = [random.uniform(0,10) for i in range(NUM_SPOTS)]
    voffsets = [random.uniform(30,40) for i in range(NUM_SPOTS)]
    vscales = [random.uniform(0,10) for i in range(NUM_SPOTS)]
    radii = [random.uniform(10,25) for i in range(NUM_SPOTS)]
    if True:
      shadow_size = 1024
      shadow_bias = 1e-3
      kwargs = {
        "app":self,
        "model":self.ball_model,
        "bias":shadow_bias,
        "dim":shadow_size,
        "layers":COLOR_LAYERS,
      }
      self.spotlights = [] 
      for i in range(NUM_SPOTS):
        s = MySpotLight( **kwargs, 
                         index=indices[i],
                         frq=frqs[i],
                         color=colors[i],
                         cookie=ccooks[i],
                         depth_cookie=dcooks[i],
                         fovbase=fovbases[i],
                         fovamp=fovamps[i],
                         voffset=voffsets[i],
                         vscale=vscales[i],
                         radius=radii[i])
        self.spotlights.append(s)

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
    
    if hasattr(self,'spotlights'):
      for s in self.spotlights:
        s.update(self.lighttime)

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
