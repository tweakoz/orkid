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
parser.add_argument('-r', '--rendermodel', type=str, default='forward', help='rendering model (deferred,forward)')
################################################################################

args = vars(parser.parse_args())

################################################################################

class RenderTestApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self,ssaa=2)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)

    setupUiCamera(app=self,eye=vec3(0,1,1)*25,tgt=vec3(0,3,0))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    self.frame_index = 0

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
    rendermodel = args["rendermodel"]

    if rendermodel == "deferred":
      rendermodel = "DeferredPBR"
    elif rendermodel == "forward":
      rendermodel="ForwardPBR"

    self.ball_model = lev2.XgmModel("data://tests/pbr_calib.glb")

    ##################
    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    normal = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
    for mesh in self.ball_model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.baseColor = vec4(1,1,1,1)
        copy.metallicFactor = 0.0
        copy.roughnessFactor = 0.0
        copy.assignImages(
          ctx,
          color = white,
          normal = normal,
          mtlruf = white,
          doConform=True
        )
        submesh.material = copy

    ##################
    # create model / sg node
    ##################

    createSceneGraph( app=self,
                      params_dict=params_dict,
                      rendermodel=rendermodel )

    self.model_drawable = self.ball_model.createDrawable()
    self.sgnode = self.scene.createDrawableNodeOnLayers(self.std_layers,"modelnode",self.model_drawable)
    self.modelinst = self.model_drawable.modelinst
    self.modelinst.enableAllMeshes()

    ###################################

    self.grid_data = createGridData()
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(2)
    self.grid_data.intensityA = 1.0
    self.grid_data.intensityB = 0.97
    self.grid_data.intensityC = 0.9
    self.grid_data.intensityD = 0.85
    self.grid_data.lineWidth = 0.1
    self.grid_node = self.layer_std.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    self.cookie1 = MyCookie("src://effect_textures/knob2.png")

    """

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
  """
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

    #for minst in self.modelinsts:
    #  minst.update(updinfo.deltatime)

    self.scene.updateScene(self.cameralut) 

  def onGpuUpdate(self,ctx):
    
    #self.spotlight1.update(self.lighttime)
    #self.spotlight2.update(self.lighttime)
    #self.spotlight3.update(self.lighttime)
    
    self.sgnode.worldTransform.translation = vec3(0,3,0)
    
    self.frame_index += 0.3
    pass 

###############################################################################

RenderTestApp().ezapp.mainThreadLoop()
