#!/usr/bin/env python3

from orkcore import *
from orklev2 import *

lev2appinit()

ctx = GfxEnv.ref.loadingContext()
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
ctx.currentMaterial = mtl

par_float = mtl.shader.param("Time")
par_vec2 = mtl.shader.param("InvViewportSize")
par_vec3 = mtl.shader.param("AmbientLevel")
par_vec4 = mtl.shader.param("ShadowParams")
par_mtx4 = mtl.shader.param("MVPC")

print(ctx.frameIndex)
FBI = ctx.FBI()
FBI.autoclear = True
FBI.clearcolor = vec4(1,0,0,1)
ctx.beginFrame()
ctx.debugMarker("yo")

mtl.bindTechnique(tek_envlight)
RCFD = ctx.topRCFD()
mtl.begin(RCFD)
mtl.bindParamFloat(par_float,1.0)
mtl.bindParamVec2(par_vec2,vec2())
mtl.bindParamVec3(par_vec3,vec3())
mtl.bindParamVec4(par_vec4,vec4())
mtl.bindParamMatrix(par_mtx4,mtx4())
mtl.end(RCFD)

print(FBI)
print(FBI.clearcolor)
print(ctx.FXI())
print(ctx.GBI())
print(ctx.TXI())
print(ctx.RSI())
print(ctx.topRCFD())

pfc = PixelFetchContext()
print(pfc.color(0))

ctx.endFrame()
FBI.capturePixel(vec4(0.0,0.0,0.0,0.0), pfc)
print(pfc.color(0))

print(ctx.frameIndex)
