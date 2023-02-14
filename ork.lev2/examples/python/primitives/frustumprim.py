#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

#pip3 install numpi Pillow

import numpy, time
from orkengine.core import *
from orkengine.lev2 import *
from PIL import Image
tokens = CrcStringProxy()

WIDTH = 1280
HEIGHT = 720

###################################
# setup context, shaders
###################################

lev2appinit()
ctx = GfxEnv.loadingContext()
FBI = ctx.FBI()
GBI = ctx.GBI()
print(ctx)
ctx.makeCurrent()
FontManager.gpuInit(ctx)

###################################
# init material
###################################

mtl = FreestyleMaterial()
mtl.gpuInit(ctx,Path("orkshader://solid"))
tek = mtl.shader.technique("vtxcolor")
par_mvp = mtl.shader.param("MatMVP")

###################################
# create RCFD, RCID
###################################

RCFD = RenderContextFrameData(ctx)
RCFD.setRenderingModel("FORWARD_UNLIT")
RCID = RenderContextInstData(RCFD)
RCID.forceTechnique(tek)
RCID.genMatrix(lambda: mtx4())

print(RCID,RCFD)

###################################
# create simple compositor (needed for fxinst based rendering)
###################################

compdata = CompositingData()
compimpl = CompositingImpl(compdata)
CPD = CompositingPassData()
CPD.cameramatrices = CameraMatrices()
RCFD.cimpl = compimpl # bind compositor to RCFD

###################################
# create fx instance
###################################

fxinst = mtl.fxcache.findFxInst(RCID)
fxinst.bindParam(par_mvp,tokens.RCFD_Camera_MVP_Mono)

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
# render target (rtg) setup - for capturing to PNG
###################################

rtg = ctx.defaultRTG()
rtb = rtg.buffer(0) #rtg's MRT buffer 0
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

CPD.cameramatrices.setCustomView(vmatrix)
CPD.cameramatrices.setCustomProjection(pmatrix)

###################################
# render frame
###################################

ctx.beginFrame()
compimpl.pushCPD(CPD)
FBI.rtGroupPush(rtg)
FBI.clear(vec4(0.6,0.6,0.7,1),1.0)
ctx.debugMarker("yo")

###################################
# render frustum primitive
###################################

fxinst.wrappedDrawCall(RCID, lambda: prim.renderEML(ctx) )

###################################
# render overlay text
###################################

FontManager.beginTextBlock(ctx,"i48",vec4(.8,.8,1,1),WIDTH,HEIGHT,100)
FontManager.draw(ctx,0,0,"!!! YO !!!\nThis is a Frustum.")
FontManager.endTextBlock(ctx)

###################################
# end frame
###################################

FBI.rtGroupPop()
compimpl.popCPD()
ctx.endFrame()

###################################
# capture to bytes
###################################

ok = FBI.captureAsFormat(rtb,capbuf,"RGBA8")

###################################
# convert to numpy
###################################

as_np = numpy.array(capbuf,dtype=numpy.uint8).reshape( HEIGHT, WIDTH, 4 )

###################################
# convert to flipped PIL image
###################################

img = Image.fromarray(as_np, 'RGBA')
flipped = img.transpose(Image.FLIP_TOP_BOTTOM)

###################################
# save to PNG
###################################

flipped.save("frustumprim.png")
