#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import time, math, os, sys
from orkengine.core import *
from orkengine import lev2
from pathlib import Path
this_dir = Path(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(str(this_dir))
from lev2utils.shaders import Shader
constants = mathconstants()

################################################################################

SHADERTEXT = """
fxconfig fxcfg_default {
  glsl_version = "330";
}
///////////////////////////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 MatMVP;
}
///////////////////////////////////////////////////////////////
vertex_interface vif_PC {
  inputs {
    vec3 position : POSITION;
  }
  outputs {
    vec4 frg_color;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface fif_PC : vif_PC {
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
///////////////////////////////////////////////////////////////
vertex_shader vs_x : vif_PC : ublock_vtx {
  gl_Position = MatMVP * vec4(position, 1);
  frg_color   = vec4(position,1);
}
///////////////////////////////////////////////////////////////
fragment_shader fs_x : fif_PC {
  out_clr   = mod(abs(frg_color*10.0),1);
}
///////////////////////////////////////////////////////////////
state_block sb_default : default {
  DepthTest = LEQUALS;
  DepthMask = ON;
}
///////////////////////////////////////////////////////////////
technique tek_x {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_x;
    fragment_shader = fs_x;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
"""

################################################################################

class MyApp(object):
  ###########################
  def __init__(self):
    super().__init__()
    self.qtapp = lev2.OrkEzApp.create(self)
    self.qtapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.counter = 0
  ###########################
  def onGpuInit(self,ctx):
    FBI = ctx.FBI
    GBI = ctx.GBI
    self.mtl = lev2.FreestyleMaterial()
    self.mtl.gpuInitFromShaderText(ctx,"shader",SHADERTEXT)
    self.tek = self.mtl.technique("tek_x")
    self.par_mvp = self.mtl.param("MatMVP")
    self.volumetexture = lev2.Texture.load("lev2://textures/voltex_pn3")
    ###################################
    fpmtx = ctx.perspective(45,1,0.1,3)
    fvmtx = ctx.lookAt(vec3(0,0,-1),vec3(0,0,0),vec3(0,1,0))
    frust = dfrustum()
    frust.set(fmtx4_to_dmtx4(fvmtx),fmtx4_to_dmtx4(fpmtx))
    self.prim = lev2.primitives.FrustumPrimitive()
    self.prim.topColor = dvec4(0.5,1.0,0.5,1)
    self.prim.bottomColor = dvec4(0.5,0.0,0.5,1)
    self.prim.leftColor = dvec4(0.0,0.5,0.5,1)
    self.prim.rightColor = dvec4(1.0,0.5,0.5,1)
    self.prim.nearColor = dvec4(0.5,0.5,1.0,1)
    self.prim.farColor = dvec4(0.5,0.5,0.0,1)
    self.prim.frustum = frust
    self.prim.gpuInit(ctx)
  ###########################
  def onDraw(self,drawevent):
    ctx = drawevent.context
    WIDTH = ctx.mainSurfaceWidth()
    HEIGHT = ctx.mainSurfaceHeight()
  ###########################
    self.counter += 1
    fi = float(self.counter) * 0.001
    θ = fi * constants.PI2
    x = math.sin(θ)*5.0
    z = -math.cos(θ)*5.0
  ###########################
    pmatrix = ctx.perspective(100*constants.DTOR,WIDTH/HEIGHT,0.1,1000.0)
    vmatrix = ctx.lookAt(vec3(x,0.5,z)*20.0,
                         vec3(0,0,0),
                         vec3(0,1,0))
    mvp_matrix = pmatrix*vmatrix
  ###########################
    RCFD = lev2.RenderContextFrameData(ctx)
    self.mtl.begin(self.tek,RCFD)
    self.mtl.bindParamMatrix4(self.par_mvp,mvp_matrix)
    self.prim.renderEML(ctx)
    self.mtl.end(RCFD)
##############################################
myapp = MyApp()
myapp.qtapp.mainThreadLoop()
