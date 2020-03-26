#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import numpy, time
from orkcore import *
from orklev2 import *
from PIL import Image

################################################################################
# set up image dimensions, with antialiasing
################################################################################

ANTIALIASDIM = 4
WIDTH = 1080
HEIGHT = 640
AAWIDTH = WIDTH*ANTIALIASDIM
AAHEIGHT = HEIGHT*ANTIALIASDIM

################################################################################

shadertext = """
fxconfig fxcfg_default {
    glsl_version = "410";
    import "orkshader://mathtools.i"
}
///////////////////////////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 MatMVP;
  mat3 MatNormal;
}
///////////////////////////////////////////////////////////////
uniform_set ublock_frg {
  sampler2D ColorMap;
  sampler3D VolumeMap;
}
///////////////////////////////////////////////////////////////
vertex_interface iface_vdefault : ublock_vtx {
  inputs {
    vec4 position : POSITION;
    vec4 vtxcolor : COLOR0;
    vec3 normal : NORMAL;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec4 frg_clr;
    vec3 frg_pos;
    vec2 frg_uv0;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface iface_fdefault : ublock_frg {
  inputs {
    vec4 frg_clr;
    vec3 frg_pos;
    vec2 frg_uv0;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}
///////////////////////////////////////////////////////////////
state_block sb_default : default { CullTest = PASS_FRONT; }
state_block sb_additive : sb_default { BlendMode = ADDITIVE; }
///////////////////////////////////////////////////////////////
vertex_shader vs_lines : iface_vdefault {
  gl_Position = MatMVP * position;
  frg_pos = position.xyz;
  frg_clr     = vtxcolor;
  frg_uv0     = uv0;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_frustum : iface_vdefault : lib_math {
  gl_Position = MatMVP * position;
  frg_pos = position.xyz;
  float light = 3*saturateF(dot(MatNormal*normal,vec3(0,0,1)));
  frg_clr     = vec4(vtxcolor.xyz*light,1);
  frg_uv0     = uv0;
}
///////////////////////////////////////////////////////////////
fragment_shader ps_texvtxcolor_noalpha : iface_fdefault {
  vec4 texc = texture(ColorMap, frg_uv0);
  out_clr   = vec4(texc.xyz * frg_clr.xyz, 1.0);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_frustum : iface_fdefault {
  // octave noise with volume texture
  int numoctaves = 8;
  float val = 0;
  float freq = 0.1;
  float amp = 1.0;
  for( int i=0; i<numoctaves; i++ ){
    val += texture(VolumeMap, frg_pos*freq).x*amp;
    freq *= 2.1;
    amp *= 0.7;
  }
  val = pow(val,5.5)*.02;
  vec4 tex = vec4(val,val,val,0);
  out_clr = frg_clr-tex;
}
///////////////////////////////////////////////////////////////
fragment_shader ps_lines : iface_fdefault {
  out_clr = frg_clr;
}
///////////////////////////////////////////////////////////////
technique tek_frustum {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_frustum;
    fragment_shader = ps_frustum;
    state_block     = sb_additive;
  }
}
///////////////////////////////////////////////////////////////
technique tek_lines {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_lines;
    fragment_shader = ps_lines;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
"""
###################################
# setup context, shaders
###################################

lev2appinit()
gfxenv = GfxEnv.ref
ctx = gfxenv.loadingContext()
FBI = ctx.FBI()
GBI = ctx.GBI()
print(ctx)
ctx.makeCurrent()
FontManager.gpuInit(ctx)

###################################

mtl = FreestyleMaterial()
mtl.gpuInitFromShaderText(ctx,"frusprim",shadertext)
tek_frustum = mtl.shader.technique("tek_frustum")
tek_lines = mtl.shader.technique("tek_lines")

par_mvp = mtl.shader.param("MatMVP")
par_mnormal = mtl.shader.param("MatNormal")
par_tex = mtl.shader.param("ColorMap")
par_tex3d = mtl.shader.param("VolumeMap")

###################################
# setup primitive
###################################

fpmtx = ctx.perspective(45,1,0.1,3)
fvmtx = ctx.lookAt(vec3(0,0,-1),vec3(0,0,0),vec3(0,1,0))
frus = Frustum()
frus.set(fvmtx,fpmtx)


qsubmesh = meshutil.SubMesh()
qsubmesh.addQuad(frus.nearCorner(3), # near
                 frus.nearCorner(2),
                 frus.nearCorner(1),
                 frus.nearCorner(0),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(0.5,0.5,1.0,1))
qsubmesh.addQuad(frus.farCorner(0), # far
                 frus.farCorner(1),
                 frus.farCorner(2),
                 frus.farCorner(3),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(0.5,0.5,0.0,1))
qsubmesh.addQuad(frus.nearCorner(1), # top
                 frus.farCorner(1),
                 frus.farCorner(0),
                 frus.nearCorner(0),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(0.5,1.0,0.5,1))
qsubmesh.addQuad(frus.nearCorner(3), # bottom
                 frus.farCorner(3),
                 frus.farCorner(2),
                 frus.nearCorner(2),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(0.5,0.0,0.5,1))
qsubmesh.addQuad(frus.nearCorner(0), # left
                 frus.farCorner(0),
                 frus.farCorner(3),
                 frus.nearCorner(3),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(0.0,0.5,0.5,1))
qsubmesh.addQuad(frus.nearCorner(2), # right
                 frus.farCorner(2),
                 frus.farCorner(1),
                 frus.nearCorner(1),
                 vec2(0.0, 0.0),
                 vec2(1.0, 0.0),
                 vec2(1.0, 1.0),
                 vec2(0.0, 1.0),
                 vec4(1.0,0.5,0.5,1))
tsubmesh = meshutil.SubMesh()
meshutil.triangulate(qsubmesh,tsubmesh)
print(tsubmesh,ctx)
prim = meshutil.PrimitiveV12N12B12T8C4(tsubmesh,ctx)
###################################
# rtg setup
###################################

rtg = ctx.defaultRTG()
ctx.resize(AAWIDTH,AAHEIGHT)
capbuf = CaptureBuffer()

texture = Texture.load("lev2://textures/voltex_pn0")
print(texture)
lev2apppoll() # process opq

###################################
# setup camera
###################################

pmatrix = ctx.perspective(45,WIDTH/HEIGHT,0.01,100.0)
vmatrix = ctx.lookAt(vec3(-5,3,3),
                     vec3(0,0,0),
                     vec3(0,1,0))

rotmatrix = vmatrix.toRotMatrix3()
mvp_matrix = vmatrix*pmatrix

###################################

vtx_t = VtxV12N12B12T8C4
vbuf = vtx_t.staticBuffer(2)
vw = GBI.lock(vbuf,2)
vw.add(vtx_t(vec3(-.7,.78,0.5),vec3(),vec3(),vec2(),0xffffffff))
vw.add(vtx_t(vec3(0,0,0.5),vec3(),vec3(),vec2(),0xffffffff))
GBI.unlock(vw)

###################################
# render frame
###################################

ctx.beginFrame()
FBI.rtGroupPush(rtg)
FBI.clear(vec4(0.6,0.6,0.7,1),1.0)
ctx.debugMarker("yo")

RCFD = RenderContextFrameData(ctx)

mtl.bindTechnique(tek_frustum)
mtl.begin(RCFD)
mtl.bindParamMatrix4(par_mvp,mvp_matrix)
mtl.bindParamMatrix3(par_mnormal,rotmatrix)
#mtl.bindParamTexture(par_tex,texture)
mtl.bindParamTexture(par_tex3d,texture)

prim.draw(ctx)
mtl.end(RCFD)

mtl.bindTechnique(tek_lines)
mtl.begin(RCFD)
mtl.bindParamMatrix4(par_mvp,mtx4())
GBI.drawLines(vw)
mtl.end(RCFD)

FontManager.beginTextBlock(ctx,"i32",vec4(0,0,.1,1),WIDTH,HEIGHT,100)
FontManager.draw(ctx,0,0,"!!! YO !!!\nThis is a textured Frustum.")
FontManager.endTextBlock(ctx)

FBI.rtGroupPop()
ctx.endFrame()

###################################
# 1. capture framebuffer -> numpy array -> PIL image
# 2. Downsample
# 3. Flip image vertically
# 4. output png
###################################

ok = FBI.captureAsFormat(rtg,0,capbuf,0) # RGBA8
as_np = numpy.array(capbuf,dtype=numpy.uint8).reshape( AAHEIGHT, AAWIDTH, 4 )
img = Image.fromarray(as_np, 'RGBA')
img = img.resize((WIDTH,HEIGHT), Image.ANTIALIAS)
flipped = img.transpose(Image.FLIP_TOP_BOTTOM)
flipped.save("customprim.png")
