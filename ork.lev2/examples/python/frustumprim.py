#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import numpy, time
from orkengine.core import *
from orkengine.lev2 import *
from PIL import Image

WIDTH = 1280
HEIGHT = 720

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
mtl.gpuInit(ctx,Path("orkshader://solid"))
tek = mtl.shader.technique("vtxcolor")
par_mvp = mtl.shader.param("MatMVP")

###################################
# setup primitive
###################################

fpmtx = ctx.perspective(45,1,0.1,3)
fvmtx = ctx.lookAt(vec3(0,0,-1),vec3(0,0,0),vec3(0,1,0))
frust = Frustum()
frust.set(fvmtx,fpmtx)
prim = primitives.FrustumPrimitive()
prim.frustum = frust
prim.topColor = vec4(0.5,1.0,0.5,1)
prim.bottomColor = vec4(0.5,0.0,0.5,1)
prim.leftColor = vec4(0.0,0.5,0.5,1)
prim.rightColor = vec4(1.0,0.5,0.5,1)
prim.frontColor = vec4(0.5,0.5,1.0,1)
prim.backColor = vec4(0.5,0.5,0.0,1)
prim.gpuInit(ctx)

###################################
# rtg setup
###################################

rtg = ctx.defaultRTG()
ctx.resize(WIDTH,HEIGHT)
capbuf = CaptureBuffer()

lev2apppoll() # process opq

###################################
# setup camera
###################################

pmatrix = ctx.perspective(45,WIDTH/HEIGHT,0.01,100.0)
vmatrix = ctx.lookAt(vec3(-5,3,3),
                     vec3(0,0,0),
                     vec3(0,1,0))

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

mtl.bindTechnique(tek)
mtl.begin(RCFD)
mtl.bindParamMatrix4(par_mvp,mvp_matrix)
prim.draw(ctx)
mtl.end(RCFD)

mtl.bindTechnique(tek)
mtl.begin(RCFD)
mtl.bindParamMatrix4(par_mvp,mtx4())
GBI.drawLines(vw)
mtl.end(RCFD)

FontManager.beginTextBlock(ctx,"i48",vec4(.8,.8,1,1),WIDTH,HEIGHT,100)
FontManager.draw(ctx,0,0,"!!! YO !!!\nThis is a Frustum.")
FontManager.endTextBlock(ctx)

FBI.rtGroupPop()
ctx.endFrame()

###################################

print(rtg.texture(0))
ok = FBI.captureAsFormat(rtg,0,capbuf,"RGBA8")
as_np = numpy.array(capbuf,dtype=numpy.uint8).reshape( HEIGHT, WIDTH, 4 )
img = Image.fromarray(as_np, 'RGBA')
flipped = img.transpose(Image.FLIP_TOP_BOTTOM)
flipped.save("frustumprim.png")
