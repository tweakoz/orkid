#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, sys, os, random, numpy
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createFrustumPrim, createGridData
from common.scenegraph import createSceneGraph

################################################################################

SHADERTEXT = """
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 m;
  mat4 mvp;
}
////////////////////////////////////////
uniform_set ublock_frg {
  vec4 modcolor;
}
////////////////////////////////////////
vertex_interface iface_vtx : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec3 nrm : NORMAL;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec3 world_nrm;
    vec3 world_pos;
    vec2 frg_uv;
  }
}
////////////////////////////////////////
fragment_interface iface_frg : ublock_frg {
  inputs {
    vec3 world_nrm;
    vec3 world_pos;
    vec2 frg_uv;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
vertex_shader vs_pseudowire : iface_vtx {
  gl_Position =  mvp * pos;
  world_pos = (m * pos).xyz;
  world_nrm = (m * vec4(nrm,0)).xyz;
  frg_uv = uv0;
}
////////////////////////////////////////
fragment_shader ps_pseudowire : iface_frg {
  
    float intens = 0.0;
    float width = 4.0;

    vec2 dx = dFdx(frg_uv);
    vec2 dy = dFdy(frg_uv);
    vec2 df = fwidth(frg_uv);
    float dd = min(df.x,df.y);
    
    width *= dd;

    if(frg_uv.x<width)
        intens += 1;
    if(frg_uv.y<width)
        intens += 1;
    if((1-frg_uv.x)<width)
        intens += 1;
    if((1-frg_uv.y)<width)
        intens += 1;
    if(intens>0)
        intens = 1;
    else
        intens = 0;

    out_clr = vec4(intens,intens,intens,1);
}

////////////////////////////////////////
technique tek_pseudowire {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_pseudowire;
    fragment_shader = ps_pseudowire;
    state_block     = default;
  }
}
"""

################################################################################

class PointsPrimApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera( app=self, eye = vec3(6,6,6), constrainZ=True, up=vec3(0,1,0))

  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################

    createSceneGraph(app=self,rendermodel="ForwardPBR")

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ##################
    # create frustum primitive
    ##################

    vmatrix = ctx.lookAt( vec3(0,0,-1),
                          vec3(0,0,0),
                          vec3(0,1,0) )

    pmatrix = ctx.perspective(45,1,0.1,3)

    frustum_prim = createFrustumPrim(ctx=ctx,vmatrix=vmatrix,pmatrix=pmatrix)

    ##################
    # create shading pipeline
    ##################

    pipeline = createPipeline( app = self,
                               ctx = ctx,
                               shadertext = SHADERTEXT,
                               blending=tokens.ADDITIVE,
                               culltest=tokens.OFF,
                               depthtest=tokens.OFF,
                               techname = "tek_pseudowire",
                               rendermodel = "ForwardPBR" )

    param_world = pipeline.sharedMaterial.param("m")
    pipeline.bindParam( param_world, tokens.RCFD_M )

    ##################
    # create sg node
    ##################

    self.primnode = frustum_prim.createNode("node1",self.layer1,pipeline)
    self.primnode.sortkey = 2;


  ################################################

  def onUpdate(self,updinfo):

    def nodesetxf(node=None,trans=None,orient=None,scale=None):
      node.worldTransform.translation = trans 
      node.worldTransform.orientation = orient 
      node.worldTransform.scale = scale

    ###################################

    Y = 3

    trans = vec3(0,Y,0)
    orient = quat()
    scale = 1+(1+math.sin(updinfo.absolutetime*2))

    ###################################

    nodesetxf(node=self.primnode,trans=trans,orient=orient,scale=scale)

    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

###############################################################################

PointsPrimApp().ezapp.mainThreadLoop()
