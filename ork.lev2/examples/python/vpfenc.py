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
import math

WIDTH = 1280
HEIGHT = 720

lev2appinit()
gfxenv = GfxEnv.ref
ctx = gfxenv.loadingContext()
FBI = ctx.FBI()
GBI = ctx.GBI()
ctx.makeCurrent()

###################################
# begin gfx init
###################################
ctx.debugPushGroup("init")
mtl = FreestyleMaterial()
mtl.gpuInit(ctx,Path("orkshader://solid"))
tek_vtxcolor = mtl.shader.technique("vtxcolor")

par_float = mtl.shader.param("Time")
par_invvpsize = mtl.shader.param("InvViewportSize")
par_vec3 = mtl.shader.param("AmbientLevel")
par_vec4 = mtl.shader.param("ShadowParams")
par_mvp = mtl.shader.param("MatMVP")

## vertex buffer init

vtx_t = VtxV12N12B12T8C4
vbuf = vtx_t.staticBuffer(3)
vw = GBI.lock(vbuf,3)
vw.add(vtx_t(vec3(-1,-1,0),vec3(),vec3(),vec2(),0xff0000ff))
vw.add(vtx_t(vec3(+1,-1,0),vec3(),vec3(),vec2(),0xffff0000))
vw.add(vtx_t(vec3(-1,+1,0),vec3(),vec3(),vec2(),0xff00ff00))
GBI.unlock(vw)
ctx.debugPopGroup()

# rtg setup

FBI.autoclear = True
rtg = ctx.defaultRTG()
ctx.resize(WIDTH,HEIGHT)

###################################
# end gfx init
###################################

capbufNV12 = CaptureBuffer()

gpuID = 0

encoder = PyNvEncoder(
    {'preset': 'hq',
     'codec': 'hevc',
     's': f"{WIDTH}x{HEIGHT}"}, gpuID)

print(encoder)
print(encoder.PixelFormat())

###################################
# frame loop
###################################

encoded_length = 0

h264file = open("vfpencout.hevc",  "wb")


for i in range(0,1200):

    phase = float(i)/60.0
    r = math.sin(phase)*0.5+0.5
    g = math.sin(phase*0.3)*0.5+0.5
    b = math.sin(phase*0.7)*0.5+0.5
    FBI.clearcolor = vec4(r,g,b,1)

    pmatrix = ctx.perspective(45,WIDTH/HEIGHT,0.01,100.0)
    vmatrix = ctx.lookAt(vec3(0,0,3),
                         vec3(0,0,0),
                         vec3(math.sin(phase),-math.cos(phase),0))

    mvp_matrix = vmatrix*pmatrix

    ###################
    # render to default buffer
    ###################
    ctx.debugPushGroup("frame%d"%i)
    ctx.beginFrame()

    mtl.bindTechnique(tek_vtxcolor)
    RCFD = ctx.topRCFD()


    mtl.begin(RCFD)
    mtl.bindParamMatrix4(par_mvp,mvp_matrix)
    GBI.drawTriangles(vw)
    mtl.end(RCFD)


    ctx.endFrame()

    #############################################
    # nv encode !
    #############################################

    FBI.captureAsFormat(rtg,0,capbufNV12,9) # NV12
    as_np = np.array(capbufNV12, copy=False)
    encFrame = encoder.EncodeSingleFrame(as_np)
    if(encFrame.size):
        encByteArray = bytearray(encFrame)
        h264file.write(encByteArray)
        encoded_length += len(encByteArray)
        print("fr<%d>: encoded_length<%d>"%(i,encoded_length))

    #############################################

    ctx.debugPopGroup()

##################################
# finish encoding
##################################

encFrames = encoder.Flush()
for encFrame in encFrames:
    if(encFrame.size):
        encByteArray = bytearray(encFrame)
        h264file.write(encByteArray)
        encoded_length += len(encByteArray)
        print("encoded_length<%d>"%encoded_length)
