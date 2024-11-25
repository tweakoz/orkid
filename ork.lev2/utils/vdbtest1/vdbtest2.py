#!/usr/bin/env ork.python

import math, sys
#import openvdb as vdb
from obt import path as obt_path 
from ork import path as ork_path
from orkengine.core import vec2,vec3
from orkengine.lev2 import vdb as ork_vdb, OrkEzApp, RefreshFastest, ui, primitives
sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path
from cameras import *
from shaders import *
from primitives import createPointsPrimV12C4, createGridData
from scenegraph import createSceneGraph

radius = 50.0 
desired_num_points = 1000000
voxel_size = radius / math.cbrt(desired_num_points);
sphere = ork_vdb.FloatGrid.createLevelSetSphere(radius, vec3(0,0,0), voxel_size, 3.0)
#sphere['radius'] = radius
#sphere.transform = ork_vdb.createLinearTransform(voxelSize=0.5)
#sphere.name = 'sphere'
#############################
outside = sphere.background
width = 1.1 * outside
#for iter in sphere.onValueSequence:
#  print(iter)
#  dist = iter.value
#  iter.value = (outside - dist) / width
#for iter in sphere.iterOffValues():
#  if iter.value < 0.0:
#    iter.value = 1.0
#    iter.active = False
#sphere.background = 0.0
#sphere.gridClass = ork_vdb.GridClass.FOG_VOLUME
#############################

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
    self.phi = 0.0
    
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
    
    self.points_prim = primitives.PointsPrimitiveV12C4.createFromVdbFloatGrid(sphere,ctx)

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

    def _pointsize():
      val = float(float(2.0+math.sin(self.phi*2.0)*2.0))
      return 1.0

    pointsize_param = pipeline.sharedMaterial.param("pointsize")
    pipeline.bindParam( pointsize_param, lambda : _pointsize() ) # set pointsize

    ##################
    # create points sg node
    ##################

    self.primnode = self.points_prim.createNode("node1",self.layer1,pipeline)
    self.primnode.sortkey = 2;


  ################################################

  def onUpdate(self,updinfo):
    self.abstime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.phi = self.abstime
    
  ################################################

  def onDraw(self,drawevent):
    context = drawevent.context
    self.ezapp.processMainSerialQueue()
    self.scene.renderOnContext(context);

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()
###############################################################################

PointsPrimApp().ezapp.mainThreadLoop()
