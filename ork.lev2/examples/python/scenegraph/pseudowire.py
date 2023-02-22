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
    vec3 uvQ : BINORMAL;
  }
  outputs {
    vec3 world_nrm;
    vec3 world_pos;
    vec3 frg_uvq;
  }
}
////////////////////////////////////////
fragment_interface iface_frg : ublock_frg {
  inputs {
    vec3 world_nrm;
    vec3 world_pos;
    vec3 frg_uvq;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
vertex_shader vs_pseudowire : iface_vtx {
  gl_Position =  mvp * pos;
  world_pos = (m * pos).xyz;
  world_nrm = (m * vec4(nrm,0)).xyz;
  frg_uvq = uvQ;
}
////////////////////////////////////////
fragment_shader ps_pseudowire : iface_frg {
  
    float intens = 0.0;
    float width = 12.0;

    // https://www.reedbeta.com/blog/quadrilateral-interpolation-part-1/
    vec2 UV = frg_uvq.xy / frg_uvq.z;

    //vec2 param_space = mod(UV*10,1);
    vec2 param_space = UV;

    vec2 df = fwidth(param_space);
    float dd = min(df.x,df.y);
    
    width *= dd;

    if(param_space.x<width)
        intens += 1;
    if(param_space.y<width)
        intens += 1;
    if((1-param_space.x)<width)
        intens += 1;
    if((1-param_space.y)<width)
        intens += 1;
    if(intens>0)
        intens = 1;
    else
        intens = 0;


    out_clr = vec4(modcolor.xyz*intens,1);
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
    param_modcolor = pipeline.sharedMaterial.param("modcolor")
    pipeline.bindParam( param_world, tokens.RCFD_M )
    pipeline.bindParam( param_modcolor, tokens.RCFD_MODCOLOR )

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

    r = 0.5+math.sin(updinfo.absolutetime*1.2)*0.5
    g = 0.5+math.sin(updinfo.absolutetime*1.7)*0.5
    b = 0.5+math.sin(updinfo.absolutetime*1.9)*0.5

    self.primnode.modcolor = vec4(r,g,b,1)


    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

###############################################################################

PointsPrimApp().ezapp.mainThreadLoop()
