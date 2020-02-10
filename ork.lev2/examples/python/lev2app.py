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
ctx.currentMaterial = mtl
print(ctx.frameIndex)
FBI = ctx.FBI()
FBI.autoclear = True
FBI.clearcolor = vec4(1,0,0,1)
ctx.beginFrame()
ctx.debugMarker("yo")
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
