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
mtl = FreestyleMaterial()
mtl.gpuInit(ctx,Path("orkshader://solid"))
tek = mtl.shader.technique("texvtxcolor_noalpha")

par_mvp = mtl.shader.param("MatMVP")
par_tex = mtl.shader.param("ColorMap")

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

texture = Texture.load("data://effect_textures/spinner")

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
# render frame
###################################

ctx.beginFrame()
FBI.rtGroupPush(rtg)
FBI.clear(vec4(0.6,0.6,0.7,1),1.0)
ctx.debugMarker("yo")

mtl.bindTechnique(tek)
RCFD = ctx.topRCFD()

mtl.begin(RCFD)
mtl.bindParamMatrix(par_mvp,mvp_matrix)
mtl.bindParamTexture(par_tex,texture)

prim.draw(ctx)
mtl.end(RCFD)

FontManager.beginTextBlock(ctx,"i48",vec4(0.2,0.2,0.5,1),WIDTH,HEIGHT,100)
FontManager.draw(ctx,0,0,"!!! YO !!!\nThis is a textured Frustum.")
FontManager.endTextBlock(ctx)

FBI.rtGroupPop()
ctx.endFrame()

###################################

print(rtg.texture(0))
ok = FBI.captureAsFormat(rtg,0,capbuf,0) # RGBA8
as_np = numpy.array(capbuf,dtype=numpy.uint8).reshape( HEIGHT, WIDTH, 4 )
img = Image.fromarray(as_np, 'RGBA')
flipped = img.transpose(Image.FLIP_TOP_BOTTOM)
flipped.save("frustumprim.png")
