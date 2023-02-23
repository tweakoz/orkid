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
    self.NUMPOINTS = 65536

  def updatePoints(self,context, abstime):

    ##################
    # fill in points
    ##################

    paramA = 4+math.sin(abstime*2)*4
    paramB = 1+math.sin(abstime*2.3)*0.15
    paramC = 1+math.sin(abstime*2.7)*0.05
    paramD = 1+math.sin(abstime*2.9)*0.025

    data_ptr = self.points_prim.lock(context) # return V12C4 array view

    data_ptr['color'] = numpy.ones(self.NUMPOINTS, dtype=numpy.uint32)*0x00004040
    data_ptr['x'] = numpy.random.uniform(-1,1, self.NUMPOINTS).astype(numpy.float32)
    data_ptr['y'] = numpy.random.uniform(-1,1, self.NUMPOINTS).astype(numpy.float32)
    data_ptr['z'] = numpy.random.uniform(-1,1, self.NUMPOINTS).astype(numpy.float32)

    def do_axis(named):
      S = numpy.copy(numpy.sign(data_ptr[named]))
      A = numpy.copy(numpy.abs(data_ptr[named]))
      P = numpy.copy(numpy.power(A,paramA))
      P = numpy.copy(numpy.power(P,paramB))
      P = numpy.copy(numpy.power(P,paramC))
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

    createSceneGraph(app=self,rendermodel="ForwardPBR")

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
                               techname = "tek_points_fwd",
                               rendermodel = "ForwardPBR" )

    pointsize_param = pipeline.sharedMaterial.param("pointsize")
    pipeline.bindParam( pointsize_param, float(2.0) ) # set pointsize

    ##################
    # create points sg node
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

###############################################################################

PointsPrimApp().ezapp.mainThreadLoop()
