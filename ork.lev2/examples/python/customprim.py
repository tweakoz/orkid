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
import _shaders

################################################################################
# set up image dimensions, with antialiasing
################################################################################

ANTIALIASDIM = 4
WIDTH = 1080
HEIGHT = 640
AAWIDTH = WIDTH*ANTIALIASDIM
AAHEIGHT = HEIGHT*ANTIALIASDIM

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

sh = _shaders.Shader(ctx)
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

texture = Texture.load("lev2://textures/voltex_pn3")
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

sh._mtl.bindTechnique(sh._tek_frustum)
sh.beginNoise(RCFD,0.0)
sh.bindMvpMatrix(mvp_matrix)
sh.bindRotMatrix(rotmatrix)
sh.bindVolumeTex(texture)

prim.draw(ctx)
sh.end(RCFD)

sh._mtl.bindTechnique(sh._tek_lines)
sh.beginLines(RCFD)
sh._mtl.bindParamMatrix4(sh._par_mvp,mtx4())
GBI.drawLines(vw)
sh.end(RCFD)

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

ok = FBI.captureAsFormat(rtg,0,capbuf,"RGBA8")
as_np = numpy.array(capbuf,dtype=numpy.uint8).reshape( AAHEIGHT, AAWIDTH, 4 )
img = Image.fromarray(as_np, 'RGBA')
img = img.resize((WIDTH,HEIGHT), Image.ANTIALIAS)
flipped = img.transpose(Image.FLIP_TOP_BOTTOM)
flipped.save("customprim.png")
