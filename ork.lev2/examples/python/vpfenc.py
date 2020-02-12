#!/usr/bin/env python3
################################################################################
# lev2 sample which encodes a h264 file using nvidia vpf module
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

from orkcore import *
from orklev2 import *
from vpf import *
import numpy as np

lev2appinit()
gfxenv = GfxEnv.ref
ctx = gfxenv.loadingContext()
FBI = ctx.FBI()
GBI = ctx.GBI()
print(ctx)
w = ctx.mainSurfaceWidth()
h = ctx.mainSurfaceHeight()
print(w,h)
ctx.makeCurrent()

###################################
# begin gfx init
###################################
ctx.debugPushGroup("init")
mtl = FreestyleMaterial()
mtl.gpuInit(ctx,Path("orkshader://deferred"))
tek_envlight = mtl.shader.technique("environmentlighting")

par_float = mtl.shader.param("Time")
par_vec2 = mtl.shader.param("InvViewportSize")
par_vec3 = mtl.shader.param("AmbientLevel")
par_vec4 = mtl.shader.param("ShadowParams")
par_mtx4 = mtl.shader.param("MVPC")

## vertex buffer init

vtx_t = VtxV12N12B12T8C4
vbuf = vtx_t.staticBuffer(3)
vw = GBI.lock(vbuf,3)
vtx = vtx_t(vec3(),vec3(),vec3(),vec2(),0xffffffff)
vw.add(vtx)
vw.add(vtx)
vw.add(vtx)
GBI.unlock(vw)
ctx.debugPopGroup()

# rtg setup

print(ctx.frameIndex)
FBI.autoclear = True
FBI.clearcolor = vec4(1,0,0,1)
rtg = ctx.defaultRTG()
ctx.resize(1024,1024)

###################################
# end gfx init
###################################

gpuID = 0
convsrcfmt = PixelFormat.RGB
convdstfmt = PixelFormat.NV12

colorconv = PySurfaceConverter(1024,1024, convdstfmt, convsrcfmt, gpuID)
gpu_uploader = PyFrameUploader(1024,1024, convsrcfmt, gpuID)

print(colorconv)

encoder = PyNvEncoder(
    {'preset': 'hq',
     'codec': 'h264',
     's': '1024x1024'}, gpuID)

framesizeRGB = 1024 * 1024 * 3

print(encoder)

rawBitmapRGB = np.zeros((framesizeRGB,),dtype = np.uint8)

###################################
# frame loop
###################################

for i in range(1,2):
    ###################
    # render to default buffer
    ###################
    print("frame<%d>"%ctx.frameIndex)
    ctx.debugPushGroup("frame-%d"%i)
    ctx.beginFrame()

    mtl.bindTechnique(tek_envlight)
    RCFD = ctx.topRCFD()
    mtl.begin(RCFD)
    mtl.bindParamFloat(par_float,1.0)
    mtl.bindParamVec2(par_vec2,vec2(2,3))
    mtl.bindParamVec3(par_vec3,vec3(4,5,6))
    mtl.bindParamVec4(par_vec4,vec4(7,8,9,10))
    mtl.bindParamMatrix(par_mtx4,mtx4())

    GBI.drawTriangles(vw)
    mtl.end(RCFD)

    pfc = PixelFetchContext(rtg,1)
    FBI.capturePixel(vec4(0,1,0,0), pfc)
    print(pfc.color(0))
    ctx.endFrame()
    ctx.debugPopGroup()

    #rawsurfaceRGB = gpu_uploader.UploadSingleFrame(rawBitmapRGB)
    #print(rawsurfaceRGB)
    #rawSurfaceNV12 = colorconv.Execute(rawsurfaceRGB)
