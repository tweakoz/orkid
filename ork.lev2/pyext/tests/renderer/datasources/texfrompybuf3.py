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
from matplotlib import pyplot as plt
from skimage import color # see https://scikit-image.org/docs/dev/api/skimage.color.html
import cv2
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
parser.add_argument('-r', '--rendermodel', type=str, default='forward', help='rendering model (deferred,forward)')

################################################################################

args = vars(parser.parse_args())

stereo = args["stereo"]
mono = not stereo
NUM_IMAGES = 15

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

  def alphaBlendWithMaskImage( self, rgb_npimg_a, rgb_npimg_b, mask_npimg ):
    # convert to float
    rgb_npimg_a = rgb_npimg_a.astype(np.float32)
    rgb_npimg_b = rgb_npimg_b.astype(np.float32)
    mask_npimg = mask_npimg.astype(np.float32)
    # normalize mask
    mask_npimg = mask_npimg / 255.0
    # blend
    rgb_npimg = rgb_npimg_a * (1.0 - mask_npimg) + rgb_npimg_b * mask_npimg
    # convert to uint8
    rgb_npimg = rgb_npimg.astype(np.uint8)
    return rgb_npimg

  ##############################################

  def genMask(self, width, height):
     # make rgb float mask image that is a gradient from 0 to 255
    mask_npimg = np.zeros((height, width, 3), dtype=np.float32)
    for y in range(height):
      mask_npimg[y,:,0] = np.linspace(0, 255, width)
    mask_npimg = mask_npimg.astype(np.uint8)
    return mask_npimg

  ##############################################

  def genNewImages(self, input_image):
    
    input_width = input_image.width
    input_height = input_image.height
    input_data = input_image.data.bytes
    
    print(f"input_image.width:{input_width}")
    print(f"input_image.height:{input_height}")
    print(f"input_image.data:{input_data}")
    
    # convert datablock to numpy array
    np_input_data = np.frombuffer(input_data, dtype=np.uint8)
    np_input_data = np_input_data.reshape((input_height, input_width, 3))
    
    ###################################
    # roll image vertically (like vertical hold)
    ###################################
        
    mask = self.genMask(input_width, input_height)
    
    self.images = []
    #fig = plt.figure(figsize=(10, 7))

    for i in range(NUM_IMAGES):
      #fig.add_subplot(6, 5, i+1) 
      fiter = float(i) / float(NUM_IMAGES)
      np_img = np_input_data.copy()
      #np_img = np.roll(np_img, int(fiter * input_height), axis=0)
      orig_np_img = np_img.astype(np.float32)
      np_img = color.convert_colorspace(orig_np_img,"RGB","HSV")
      # shift hue
      np_img[:,:,0] = np_img[:,:,0] + fiter
      # fully saturate
      np_img[:,:,1] = 1.0
      # back to rgb
      np_img = color.convert_colorspace(np_img, "HSV","RGB")
      # blend with mask
      
      np_img = self.alphaBlendWithMaskImage(orig_np_img, np_img, mask)

      np_img = np_img.astype(np.uint8)
      #plt.imshow(np_img)
      image = lev2.Image.createFromBuffer( input_width,
                                           input_height,
                                           tokens.RGB8,
                                           np_img)
      self.images.append(image)    
    #plt.show()

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

    ##################
    # create scenegraph
    ##################

    rendermodel = args["rendermodel"]
    if rendermodel == "deferred":
      rendermodel = "DeferredPBR"
    elif rendermodel == "forward":
      rendermodel="ForwardPBR"

    createSceneGraph( app=self,
                     rendermodel=rendermodel,
                     params_dict=params_dict
                    )

    ###################################

    self.grid_data = createGridData()
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(1)*3
    self.grid_data.majorTileDim = 8.0
    self.grid_node = self.layer_std.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    self.ball_model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.cookie1 = MyCookie("src://effect_textures/knob2.png")

    self.model = lev2.XgmModel("data://tests/chartest/char_mesh")
    self.anim = lev2.XgmAnim("data://tests/chartest/char_testanim1")

    self.anim_inst = lev2.XgmAnimInst(self.anim)
    self.anim_inst.mask.enableAll()
    self.anim_inst.use_temporal_lerp = True
    self.anim_inst.bindToSkeleton(self.model.skeleton)

    self.model_materials = []
    texset = set()
    
    ##################
    for mesh in self.model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.baseColor = vec4(2,2,2,1)
        copy.metallicFactor = 0.0
        copy.roughnessFactor = 1.0
        submesh.material = copy
        cnmrea = copy.texArrayCNMREA
        texset.add(cnmrea)
        self.model_materials.append(copy)

    assert(len(texset)==1)
    
    self.model_texture = texset.pop()
    self.model_colorimage = self.model_texture.subimage(0)

    self.genNewImages(self.model_colorimage)

    ##################
    # create model / sg node
    ##################

    self.model_drawable = self.model.createDrawable()
    self.sgnode = self.scene.createDrawableNodeOnLayers(self.std_layers,"modelnode",self.model_drawable)
    self.modelinst = self.model_drawable.modelinst
    self.modelinst.enableSkinning()
    self.modelinst.enableAllMeshes()
    self.localpose = self.modelinst.localpose
    self.worldpose = self.modelinst.worldpose

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):
    self.lighttime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) 

  ################################################

  def onGpuUpdate(self,ctx):

    ###########################
    # animate model
    ###########################

    self.localpose.bindPose()
    self.anim_inst.currentFrame = self.frame_index
    self.anim_inst.weight = 1.0
    self.anim_inst.applyToPose(self.localpose)
    self.localpose.blendPoses()
    self.localpose.concatenate()
    
    self.sgnode.worldTransform.translation = vec3(0,3,0)

    ###########################
    # texture replacement
    ###########################

    next_image = self.images[int(self.frame_index)%NUM_IMAGES]
    for mtl in self.model_materials:
      mtl.setColorImage(ctx,next_image)
    self.frame_index += 0.3

    ###########################

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
