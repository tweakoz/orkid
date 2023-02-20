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
from common.primitives import createPointsPrimV12C4, createGridData
from common.scenegraph import createSceneGraph

################################################################################

SHADERTEXT = """
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 mvp;
  float pointsize;
}
////////////////////////////////////////
uniform_set ublock_frg {
  vec4 modcolor;
}
////////////////////////////////////////
vertex_interface iface_vtx_points : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec4 col : COLOR0;
  }
  outputs {
    vec3 frg_col;
  }
}
////////////////////////////////////////
fragment_interface iface_frg_points : ublock_frg {
  inputs {
    vec3 frg_col;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
state_block sb_points : default {
 CullTest  = OFF;  
 DepthTest  = LEQUALS;  
 DepthMask  = ON;  
}
////////////////////////////////////////
vertex_shader vs_points : iface_vtx_points {
  frg_col = col.xyz;
  gl_Position = mvp * vec4(pos.x,pos.y,pos.z,1);
  gl_PointSize = pointsize;
}
////////////////////////////////////////
fragment_shader ps_points : iface_frg_points {
  out_clr = vec4(frg_col.xyz, 1);
}

////////////////////////////////////////
technique tek_points_fwd {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_points;
    fragment_shader = ps_points;
    state_block     = sb_points;
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
    # create points primitive 
    ###################################

    NUMPOINTS = 262144

    points_prim = createPointsPrimV12C4(ctx=ctx,numpoints=NUMPOINTS)

    ##################
    # fill in points
    ##################

    data_ptr = numpy.array(points_prim.lock(ctx), copy=False)
    for i in range(NUMPOINTS):
      VTX = data_ptr[i] # V12, C4

      x = random.uniform(-1,1)
      y = random.uniform(-1,1)
      z = random.uniform(-1,1)

      x = numpy.sign(x)*x*x
      y = numpy.sign(y)*y*y
      z = numpy.sign(z)*z*z

      VTX[0] = x*2    # float x
      VTX[1] = 2+y*2  # float y 
      VTX[2] = z*2    # float z 


      VTX[3] = 0x0000ffff # uint32_t color (abgr)

    points_prim.unlock(ctx)

    ##################
    # create shading pipeline
    ##################

    pipeline = createPipeline( app = self,
                               ctx = ctx,
                               shadertext = SHADERTEXT,
                               techname = "tek_points_fwd",
                               rendermodel = "ForwardPBR" )

    pointsize_param = pipeline.sharedMaterial.param("pointsize")
    pipeline.bindParam( pointsize_param, float(1.5) ) # set pointsize

    ##################
    # create points sg node
    ##################

    self.primnode = points_prim.createNode("node1",self.layer1,pipeline)

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

###############################################################################

PointsPrimApp().ezapp.mainThreadLoop()
