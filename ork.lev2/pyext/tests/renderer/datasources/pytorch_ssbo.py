#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders data sourced from pytorch
#  (and modified by a compute shader)
# Copyright 1996-2025, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import torch
import math, sys, os, signal
from pathlib import Path
from orkengine.core import lev2pyexdir
from orkengine import lev2
from obt import template
sys.path.append(lev2pyexdir().normalized.as_string) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createFrustumPrim, createGridData
from lev2utils.scenegraph import createSceneGraph
from lev2utils.primitives import createPointsPrimSSBO, createGridData

DIM = 3072
NUMPOINTS = DIM*DIM
SIZEOF_FLOAT = 4
SIZEOF_VEC4F = 4*SIZEOF_FLOAT

COMPUTE_STORAGE = """
  storage {
    layout(std430, binding = 0) buffer {
      vec4 positions[$$$NUMPOINTS]; // avoid vec3 alignment issues
      vec4 colors[$$$NUMPOINTS];    // avoid vec3 alignment issues
      float time;
    } ssbo_tensor;
  }
"""

COMPUTE_STORAGE = template.template_string(COMPUTE_STORAGE,{"NUMPOINTS":NUMPOINTS})

SHADERTEXT = """
////////////////////////////////////////
fxconfig fxcfg_default {}
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
libblock typelib_ssbo {
  pragma_typelib;
  struct Vertex {
    vec3 pos;           // 12
    vec3 color;         // 16
  };
}
////////////////////////////////////////
vertex_interface iface_vtx_points 
  : ublock_vtx
  : typelib_ssbo {
  inputs {
    vec4 pos : POSITION;
    vec4 col : COLOR0;
  }
  $$$COMPUTE_STORAGE
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
vertex_shader vs_points 
  : iface_vtx_points {
  vec3 posv3   = positions[gl_VertexID].xyz;
  vec4 posv4   = vec4(posv3.xyz,1);
  frg_col      = colors[gl_VertexID].xyz;
  gl_PointSize = pointsize;
  gl_Position  = mvp * posv4;
}
////////////////////////////////////////
fragment_shader ps_points : iface_frg_points {
  out_clr = vec4(frg_col, 1);
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
////////////////////////////////////////
compute_interface iface_compute
 : typelib_ssbo {
 
  $$$COMPUTE_STORAGE

  inputs {
      layout(local_size_x = 1, local_size_y = 1, local_size_z = 1);
  }
}
////////////////////////////////////////
compute_shader compute_torch
    : extension(GL_NV_gpu_shader5)
    : iface_compute {

    int index = int(gl_WorkGroupID.x);
    
    vec3 inp_col  = colors[index].xyz;
    inp_col.x = 0.5+sin(inp_col.x*PI2*2.0)*0.5;
    inp_col.y = 0.5+sin(time+inp_col.y*PI2*8.0)*0.5;
    inp_col.z = 0.5+sin(inp_col.z*PI2*2.0)*0.5;
    colors[index].xyz = inp_col;
}
"""

SHADERTEXT = template.template_string(SHADERTEXT,{"COMPUTE_STORAGE":COMPUTE_STORAGE})

################################################################################

class MinimalSceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.materials = set()
    setupUiCamera( app=self, eye = vec3(10,10,10), constrainZ=True, up=vec3(0,1,0))
    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)
    self.frame = 0
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
    # pytorch test
    ###################################

    torch_device = "cpu"
    if torch.cuda.is_available():
      torch_device = "cuda"
      print("torch device: %s" % torch_device)
      torch.cuda.set_device(0)
    elif torch.mps.is_available():
      torch_device = "mps"
      print("torch device: %s" % torch_device)

    # create N dimensional tensor of NUMPOINTS points
    tensor_pos = torch.zeros([NUMPOINTS,4],dtype=torch.float32,device=torch_device)
    tensor_col = torch.zeros([NUMPOINTS,4],dtype=torch.float32,device=torch_device)
    self.l2tensor_pos = lev2.TorchTensor(tensor_pos)    
    self.l2tensor_col = lev2.TorchTensor(tensor_col)    
    self.ssbo = ctx.CI.createShaderStorageBufferWithLength(NUMPOINTS*4*6)

    tensor_pipeline = createPipeline( app = self,
                                      ctx = ctx,
                                      shadertext = SHADERTEXT,
                                      blending=tokens.OFF,
                                      depthtest=tokens.LEQUALS,
                                      techname = "tek_points_fwd",
                                      rendermodel = "FORWARD_PBR" )

    freestylemtl = tensor_pipeline.sharedMaterial

    param_pntsize = freestylemtl.param("pointsize")
    param_mvp = freestylemtl.param("mvp")
    
    tensor_pipeline.bindParam( param_pntsize, float(1.0) )                   # set pointsize
    tensor_pipeline.bindParam( param_mvp,     tokens.RCFD_Camera_MVP_Mono )   # matrix from left vr camera

    self.computeshader = freestylemtl.computeShader("compute_torch")
    print("computeshader<%s>" % self.computeshader)
    
    ##################
    # create points scene graph node
    ##################

    self.points_prim = createPointsPrimSSBO(ctx=ctx,numpoints=NUMPOINTS,ssbo=self.ssbo)
    self.primnode_points = self.points_prim.createNode("node1",self.layer1,tensor_pipeline)
    self.primnode_points.sortkey = 2;
    #self.points_prim.debug = True

  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ##############################################

  def onGpuUpdate(self,ctx):
    self.frame += 1    
    time = self.frame / 60.0
            
    # set the positions of the points to be a sphere

    r = math.sin(time) * 2.0
    
    # convert linear point index space to 2d-uv space via modulo
    
    lspace = torch.arange(0,NUMPOINTS,dtype=torch.float32,device=torch_device)
    x_coords = lspace % DIM
    y_coords = lspace // DIM

    # compute spherical coordinates
    
    theta = x_coords * 2.0 * math.pi / DIM
    phi = y_coords * math.pi / (DIM*0.5) - math.pi/2.0
    
    # spherical to cartesian

    x = torch.cos(theta) * torch.sin(phi)
    y = torch.sin(theta) * torch.sin(phi)
    z = torch.cos(phi)
    
    # set the positions of the points in the position tensor
    
    tensor_pos = self.l2tensor_pos.as_torch # 2d array of NUMPOINTS,4 floats

    tensor_pos[:,0] = (x*r).flatten()
    tensor_pos[:,1] = (y*r).flatten()
    tensor_pos[:,2] = (z*r).flatten()

    # set the colors of the points in the color tensor

    tensor_col = self.l2tensor_col.as_torch # 2d array of NUMPOINTS,4 floats

    tensor_col[:,0] = (x*0.5+0.5).flatten()
    tensor_col[:,1] = (y*0.5+0.5).flatten()
    tensor_col[:,2] = (z*0.5+0.5).flatten()
        
    # copy the tensors to the ssbo

    POS_OFFSET = 0
    COL_OFFSET = POS_OFFSET + NUMPOINTS*SIZEOF_VEC4F
    TIM_OFFSET = COL_OFFSET + NUMPOINTS*SIZEOF_VEC4F
    ctx.CI.copyTensorIntoShaderStorageBuffer(self.l2tensor_pos,self.ssbo,POS_OFFSET)
    ctx.CI.copyTensorIntoShaderStorageBuffer(self.l2tensor_col,self.ssbo,COL_OFFSET)
    ctx.CI.copyDataIntoShaderStorageBuffer(time*10.0,self.ssbo,TIM_OFFSET)
    
    ctx.CI.dispatch(self.computeshader,NUMPOINTS,1,1)
    
    
    
    
    

###############################################################################

MinimalSceneGraphApp().ezapp.mainThreadLoop()

