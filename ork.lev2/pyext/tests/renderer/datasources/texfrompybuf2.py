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
    num_circles = 5
    
    self.images = []
    
    # generate a overlapping circles pattern (circular) with numpy
    for i in range(30):
      scale = float(29-i) / 30.0
      height, width = img_dim, img_dim
      center_y, center_x = img_dim // 2, img_dim // 2
      max_radius = img_dim

      # Create a grid of (x, y) coordinates
      y, x = np.ogrid[:height, :width]

      # Calculate the distance of each pixel from the center
      distance_from_center = np.sqrt((x - center_x)**2 + (y - center_y)**2)

      # Normalize distances to the range [0, 1]
      normalized_distance = scale*distance_from_center / max_radius

      # Calculate the pattern for concentric circles
      circle_pattern = (normalized_distance * num_circles) % 1.0

      # Create the buffer and map the pattern to a grayscale image
      np_img = (circle_pattern * 255).astype(np.uint8)
          
      print(f"np_img.shape:{np_img.shape}")

      # convert to rgb
      np_img = np.stack((np_img,)*3, axis=-1)
      self.images.append(np_img)

    ###################################

    self.grid_data = createGridData()
    self.grid_data.colorImage = lev2.Image.createFromBuffer( img_dim,
                                                             img_dim,
                                                             tokens.RGB8,
                                                             self.images[0])
    self.grid_data.mtlrufImage = lev2.Image.createRGB8FromColor( img_dim,img_dim,vec3(1,0,1) )

    self.grid_data.shader_suffix = "_V5"
    self.grid_data.modcolor = vec3(1,0.5,1)*3
    self.grid_data.majorTileDim = 20.0
    self.grid_node = self.layer_fwd.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    self.ball_model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.cookie1 = MyCookie("src://effect_textures/knob2.png")

    shadow_size = 4096
    shadow_bias = 1e-3
    intens = 100
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
    self.spotlight1.update(self.lighttime)
    self.spotlight2.update(self.lighttime)
    self.spotlight3.update(self.lighttime)
    self.frame_index += 0.3
    pass 

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
