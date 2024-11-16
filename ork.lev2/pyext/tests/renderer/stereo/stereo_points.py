#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random, numpy
from numba import jit, prange
from pathlib import Path
from ork import path as ork_path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append(str(ork_path.py_examples))
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createPointsPrimV12C4, createGridData
from lev2utils.scenegraph import createSceneGraph

################################################################################

SHADERTEXT = """
////////////////////////////////////////
fxconfig fxcfg_default {}
////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 mvp;
  mat4 mvp_l;
  mat4 mvp_r;
  float pointsize;
}
////////////////////////////////////////
uniform_set ublock_frg {
  vec4 modcolor;
}
////////////////////////////////////////
vertex_interface iface_vtx_points 
  : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec4 col : COLOR0;
  }
  outputs {
    layout(secondary_view_offset = 1) int gl_Layer;
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
vertex_shader vs_points 
  : iface_vtx_points
  : extension(GL_NV_stereo_view_rendering)
  : extension(GL_NV_viewport_array2) {
  frg_col = col.xyz;
  vec4 posv4 = vec4(pos.x,pos.y,pos.z,1);
  gl_PointSize = pointsize;
  gl_Position                   = mvp_l * posv4;
  gl_SecondaryPositionNV        = mvp_r * posv4;
  gl_Layer                      = 0;
  gl_ViewportMask[0]            = 1;
  gl_SecondaryViewportMaskNV[0] = 2;
}
////////////////////////////////////////
fragment_shader ps_points : iface_frg_points {
  out_clr = vec4(frg_col.xyz, 1);
}

////////////////////////////////////////
technique tek_points_fwd_stereo {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_points;
    fragment_shader = ps_points;
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
    self.NUMPOINTS = 262144

  def updatePoints(self,context, abstime):

    ##################
    # fill in points
    ##################

    paramA = 4+math.sin(abstime*2)*4
    paramB = 1+math.sin(abstime*2.3)*0.15
    paramC = 1+math.sin(abstime*2.7)*0.05
    paramD = 1+math.sin(abstime*2.9)*0.025

    data_ptr = self.points_prim.lock(context) # return V12C4 array view

    color_abgr = 0x00004040
    data_ptr['color'] = numpy.ones(self.NUMPOINTS, dtype=numpy.uint32)*color_abgr
    data_ptr['x'] = numpy.random.uniform(-2,2, self.NUMPOINTS).astype(numpy.float32)
    data_ptr['y'] = numpy.random.uniform(-1,1, self.NUMPOINTS).astype(numpy.float32)
    data_ptr['z'] = numpy.random.uniform(-2,2, self.NUMPOINTS).astype(numpy.float32)

    def do_axis(named):
      S = numpy.sign(data_ptr[named])
      A = numpy.abs(data_ptr[named])
      P = numpy.power(A,paramA)
      P = numpy.power(P,paramB)
      P = numpy.power(P,paramC)
      data_ptr[named] = P*S
      if named == 'y':
        data_ptr['y'] += numpy.ones(self.NUMPOINTS, dtype=numpy.float32)

    do_axis('x')
    do_axis('y')
    do_axis('z')

    self.points_prim.unlock(context) # unlock array view (writes to GPU)
  
  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################

    createSceneGraph(app=self,rendermodel="FWDPBRVR")

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ###################################
    # create points primitive 
    ###################################

    self.points_prim = createPointsPrimV12C4(ctx=ctx,numpoints=self.NUMPOINTS)

    self.updatePoints(ctx,0.1 )

    ##################
    # create shading pipeline
    ##################

    pipeline = createPipeline( app = self,
                               ctx = ctx,
                               shadertext = SHADERTEXT,
                               blending=tokens.ADDITIVE,
                               depthtest=tokens.LEQUALS,
                               techname = "tek_points_fwd_stereo",
                               rendermodel = "FORWARD_PBR" )
    
    param_pntsize = pipeline.sharedMaterial.param("pointsize")
    param_mvpL = pipeline.sharedMaterial.param("mvp_l")
    param_mvpR = pipeline.sharedMaterial.param("mvp_r")
    
    pipeline.bindParam( param_pntsize, float(2.0) )                   # set pointsize
    pipeline.bindParam( param_mvpL,    tokens.RCFD_Camera_MVP_Left )  # matrix from left vr camera
    pipeline.bindParam( param_mvpR,    tokens.RCFD_Camera_MVP_Right ) # matrix from right vr camera

    ##################
    # create points scene graph node
    ##################

    self.primnode = self.points_prim.createNode("node1",self.layer1,pipeline)
    self.primnode.sortkey = 2;


  ################################################

  def onUpdate(self,updinfo):
    self.abstime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ################################################

  def onDraw(self,drawevent):
    context = drawevent.context
    self.ezapp.processMainSerialQueue()
    self.updatePoints(context,self.abstime )
    self.scene.renderOnContext(context);

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

###############################################################################

PointsPrimApp().ezapp.mainThreadLoop()
