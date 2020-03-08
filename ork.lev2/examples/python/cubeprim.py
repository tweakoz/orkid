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

lev2appinit()
gfxenv = GfxEnv.ref
ctx = gfxenv.loadingContext()
FBI = ctx.FBI()
GBI = ctx.GBI()
print(ctx)
ctx.makeCurrent()
mtl = FreestyleMaterial()
mtl.gpuInit(ctx,Path("orkshader://solid"))
print(mtl)
print(mtl.shader)
print(mtl.shader.params)
print(mtl.shader.techniques)
tek = mtl.shader.technique("vtxcolor")
print(tek)

par_mvp = mtl.shader.param("MatMVP")

###################################

cubeprim = primitives.CubePrimitive()
cubeprim.size = 1
cubeprim.topColor = vec4(0.5,1.0,0.5,1)
cubeprim.bottomColor = vec4(0.5,0.0,0.5,1)
cubeprim.leftColor = vec4(0.0,0.5,0.5,1)
cubeprim.rightColor = vec4(1.0,0.5,0.5,1)
cubeprim.frontColor = vec4(0.5,0.5,1.0,1)
cubeprim.backColor = vec4(0.5,0.5,0.0,1)
cubeprim.gpuInit(ctx)

###################################
# rtg setup
###################################

FBI.autoclear = True
FBI.autoclear = True
FBI.clearcolor = vec4(1,0,0,1)
rtg = ctx.defaultRTG()
ctx.resize(WIDTH,HEIGHT)
capbuf = CaptureBuffer()

###################################
pmatrix = ctx.perspective(70,WIDTH/HEIGHT,0.01,100.0)
vmatrix = ctx.lookAt(vec3(1,0.8,1),
                     vec3(0,0,0),
                     vec3(0,1,0))

mvp_matrix = vmatrix*pmatrix

print(ctx.frameIndex)
print( "RTG<%s>"%rtg)

ctx.beginFrame()
FBI.rtGroupPush(rtg)
FBI.clear(vec4(0.6,0.6,0.7,1),1.0)
ctx.debugMarker("yo")

mtl.bindTechnique(tek)
RCFD = ctx.topRCFD()

mtl.begin(RCFD)
mtl.bindParamMatrix(par_mvp,mvp_matrix)
cubeprim.draw(ctx)
mtl.end(RCFD)
FBI.rtGroupPop()
ctx.endFrame()

ok = FBI.captureAsFormat(rtg,0,capbuf,0) # RGBA8
assert(ok)
as_np = numpy.array(capbuf,dtype=numpy.uint8).reshape( HEIGHT, WIDTH, 4 )
img = Image.fromarray(as_np, 'RGBA')
flipped = img.transpose(Image.FLIP_TOP_BOTTOM)
flipped.save("cubeprim.png")
