#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
import numpy as np
from orkengine.core import vec3, vec4, quat, mtx4, Sphere, Transform
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

IMP_SHADERTEXT = """
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set uset_vtx {
  mat4 mvp;
}
////////////////////////////////////////
uniform_set uset_frg {
  vec2 inverse_viewport_size;
  sampler2D rtgtex;
}
////////////////////////////////////////
vertex_interface iface_vtx : uset_vtx {
  inputs {
    vec4 pos : POSITION;
    vec3 nrm : NORMAL;
    vec3 bin : BINORMAL;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec3 frg_col;
  }
}
////////////////////////////////////////
fragment_interface iface_frg : uset_frg {
  inputs {
    vec3 frg_col;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
vertex_shader vs_imp1 : iface_vtx {
  frg_col = normalize(nrm);//+vec3(1))*0.5;
  gl_Position = mvp * pos;
}
////////////////////////////////////////
fragment_shader ps_imp : iface_frg {
  out_clr = vec4(frg_col,1);
}

////////////////////////////////////////
technique tek_imp1 {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_imp1;
    fragment_shader = ps_imp;
    state_block     = default;
  }
}
"""

################################################################################

class ImposterApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self,ssaa=0)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.frame_index = 0

    setupUiCamera(app=self,eye=vec3(0,1,1)*25,tgt=vec3(0,0,0))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    RENDERMODEL = "ForwardPBR"
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
    params_dict["preset"] = RENDERMODEL

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
    self.grid_data.majorTileDim = 1.0
    self.grid_node = self.layer_fwd.createDrawableNodeFromData("grid",self.grid_data)
    self.grid_node.sortkey = 1

    self.ball_model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.cookie1 = MyCookie("src://effect_textures/knob2.png")

  ##############################################
  # create imposter
  ##############################################

    # imposter material
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx,"IMPX",IMP_SHADERTEXT)
    mtl.rasterstate.setBlendingMacro(tokens.OFF)
    mtl.rasterstate.culltest = tokens.PASS_FRONT
    mtl.rasterstate.depthtest = tokens.LEQUALS

    # imposter pipeline permutation
    permu = lev2.FxPipelinePermutation()
    permu.technique = mtl.shader.technique("tek_imp1")

    # imposter pipeline
    pipeline = mtl.fxcache.findPipeline(permu)
    pipeline.name = "imppipe"
    pipeline.bindParam(mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
    pipeline.sharedMaterial = mtl

    # rtgroup
    rtg = lev2.RtGroup(ctx,128,128)
    rtb_c = rtg.createBuffer(tokens.RGB8,tokens.NONE)
    # the imposter itself    
    self.imp_data = lev2.ImposterDrawableData()
    self.imp_data.shape = Sphere(vec3(0), 1.0)
    self.imp_data.detail = 3
    self.imp_data.pipeline = pipeline
    self.imp_data.rtgroup = rtg

    # imposter scenegraph node
    self.imp_node = self.layer_fwd.createDrawableNodeFromData("imp1",self.imp_data)
    self.imp_node.worldTransform.scale = 1
    self.imp_node.worldTransform.translation = vec3(0,0,0)

  ##############################################

  def onUiEvent(self,uievent):
    res = lev2.ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      ######################
      if uievent.keycode == ord("D"):
        self.imp_data.debug_viz = not self.imp_data.debug_viz
        return res
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return res

  ################################################

  def onUpdate(self,updinfo):
    self.time = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) 

  def onGpuUpdate(self,ctx):
    self.frame_index += 0.3
    y = 1.0+math.sin(self.frame_index*0.05)
    pos = vec3(0,1,0)
    self.imp_node.worldTransform.translation = pos
    pass 

###############################################################################

ImposterApp().ezapp.mainThreadLoop()
