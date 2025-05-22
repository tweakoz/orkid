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
fxconfig fxcfg_default { 
  glsl_version = "330";
  import "orkshader://sdftools.i";
  import "orkshader://misctools.i";
}
////////////////////////////////////////
uniform_set uset_vtx {
  mat4 mvp;
}
////////////////////////////////////////
uniform_set uset_frg {
  vec2 inverse_viewport_size;
  sampler2D rtgtex;
  float time;
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
    vec3 frg_pos;
  }
}
////////////////////////////////////////
fragment_interface iface_frg : uset_frg {
  inputs {
    vec3 frg_col;
    vec3 frg_pos;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
vertex_shader vs_imp1 : iface_vtx {
  frg_col = normalize(nrm);//+vec3(1))*0.5;
  frg_pos = pos.xyz;
  gl_Position = mvp * pos;
}
////////////////////////////////////////
libblock lib_X : lib_sdftools : lib_mmnoise : lib_cellnoise{
  float sampleNoise(vec3 p) {
    float n = 0.0;
    for (int o = 0; o < 4; o++) {
      float amp = float(4 - o) / 2.0;
      float frq = float(o + 1) / 8.0;    
      vec3 samplePos = (p * frq) + vec3(0, time * amp * 0.2, 0);
      n += cellnoise(samplePos) * amp; 
    } 
    return pow(n * 0.5, 2);
  }
  vec3 calculateNormal(vec3 p) {
    const float h = 0.001;
    const vec2 k = vec2(1, -1);
    return normalize(
      k.xyy * sampleNoise(p + k.xyy * h) +
      k.yyx * sampleNoise(p + k.yyx * h) +
      k.yxy * sampleNoise(p + k.yxy * h) +
      k.xxx * sampleNoise(p + k.xxx * h)
    );
  }
}
////////////////////////////////////////
fragment_shader ps_imp : iface_frg : lib_X {

  const int MAX_STEPS = 128;
  const float MAX_DIST = 20.0;
  const float SURFACE_DIST = 0.004;

  vec3 ro = vec3(0, 0, -5);  // Ray origin
  vec3 rd = normalize(frg_pos);  // Ray direction

  // Raymarch
  float totalDist = 0.0;
  float surfaceThreshold = 0.6;  // Isosurface threshold
  
  for (int i = 0; i < MAX_STEPS; i++) {
      vec3 p = ro + rd * totalDist;
      
      // Sample noise at current point
      float noise = sampleNoise(p);
      
      // Distance to isosurface
      float dist = abs(noise - surfaceThreshold);
      
      // Advance ray
      totalDist += dist;
      
      // Surface hit or max distance reached
      if (dist < SURFACE_DIST || totalDist > MAX_DIST) {
          break;
      }
  }
  
  // Check if we hit a surface
  if (totalDist < MAX_DIST) {
      vec3 hitPoint = ro + rd * totalDist;
      
      // Calculate surface normal for lighting
      vec3 normal = calculateNormal(hitPoint);
      
      // Basic lighting
      vec3 lightDir = normalize(vec3(1, 1, -1));
      float lighting = max(0.0, dot(normal, lightDir));
      
      // Sample noise for color variation
      float noiseVal = sampleNoise(hitPoint);
      
      // Color based on noise and lighting
      vec3 surfaceColor = ((normal+vec3(1))*0.5)*vec3(noiseVal);// * (0.5 + 0.5 * lighting);
      
      out_clr = vec4(surfaceColor, 1.0);
  } else {
      // Background color if no surface hit
      //out_clr = vec4(0.1, 0.1, 0.2, 1.0);
      discard;
  }
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
    pipeline.bindParam(mtl.param("time"), lambda: self.time)
    pipeline.sharedMaterial = mtl

    # rtgroup
    rtg_imp = lev2.RtGroup(ctx,256,256)
    rtb_imp_color = rtg_imp.createBuffer(tokens.RGBA8,tokens.NONE)
    # the imposter itself    
    self.imp_data = lev2.ImposterDrawableData()
    self.imp_data.shape = Sphere(vec3(0), 1.0)
    self.imp_data.detail = 3
    self.imp_data.imp_pass.pipeline = pipeline
    self.imp_data.imp_pass.rtgroup = rtg_imp

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
        self.imp_data.blit_pass.debug_viz = not self.imp_data.blit_pass.debug_viz
        return res
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return res

  ################################################

  def onUpdate(self,updinfo):
    #self.imp_data.debug_viz = (int(self.frame_index)>>6)&1
    self.time = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) 

  ################################################

  def onGpuUpdate(self,ctx):
    self.frame_index += 0.3
    y = 1.0+math.sin(self.frame_index*0.05)
    pos = vec3(0,y,0)
    self.imp_node.worldTransform.translation = pos
    pass 

###############################################################################

ImposterApp().ezapp.mainThreadLoop()
