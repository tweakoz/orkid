#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

from orkcore import *
from orklev2 import *

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
mtl = FreestyleMaterial()
mtl.gpuInit(ctx,Path("orkshader://deferred"))
print(mtl)
print(mtl.shader)
print(mtl.shader.params)
print(mtl.shader.techniques)
tek_envlight = mtl.shader.technique("environmentlighting")
print(tek_envlight)

par_float = mtl.shader.param("Time")
par_vec2 = mtl.shader.param("InvViewportSize")
par_vec3 = mtl.shader.param("AmbientLevel")
par_vec4 = mtl.shader.param("ShadowParams")
par_mtx4 = mtl.shader.param("MVPC")

###################################

vtx_t = VtxV12N12B12T8C4
vbuf = vtx_t.staticBuffer(3)
vw = GBI.lock(vbuf,3)
vtx = vtx_t(vec3(),vec3(),vec3(),vec2(),0xffffffff)
vw.add(vtx)
vw.add(vtx)
vw.add(vtx)
GBI.unlock(vw)

###################################

print(ctx.frameIndex)
FBI.autoclear = True
FBI.clearcolor = vec4(1,0,0,1)
rtg = ctx.defaultRTG()
print( "RTG<%s>"%rtg)
ctx.beginFrame()
ctx.debugMarker("yo")

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

print(FBI)
print(FBI.clearcolor)
print(ctx.FXI())
print(ctx.GBI())
print(ctx.TXI())
print(ctx.RSI())
print(ctx.topRCFD())

pfc = PixelFetchContext(rtg,1)
print(pfc.color(0))

FBI.capturePixel(vec4(0,1,0,0), pfc)
print(pfc.color(0))
ctx.endFrame()

print(ctx.frameIndex)
