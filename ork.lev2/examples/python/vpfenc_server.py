#!/usr/bin/env python3
################################################################################
# lev2 sample which encodes a h264 VLC stream using nvidia vpf module
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

from orkengine.core import *
from orkengine.lev2 import *
from vpf import *
import numpy as np
import math
import http.server
from queue import Queue

import time, threading, socket

PORT = 8000

#################################################
# stream http server
#################################################

q = Queue(maxsize=500)

class Handler(http.server.BaseHTTPRequestHandler):

    def do_GET(self):
        if self.path != '/':
            self.send_error(404, "Object not found")
            return
        self.send_response(200)
        self.send_header('Content-type','video/h264')
        self.end_headers()

        # serve up an infinite stream
        i = 0
        while True:
            b = q.get(block=True,timeout=None)
            self.wfile.write(b)

# Create ONE socket.
addr = ('', PORT)
sock = socket.socket (socket.AF_INET, socket.SOCK_STREAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(addr)
sock.listen(1)

# Launch 100 listener threads.
class Thread(threading.Thread):
    def __init__(self, i):
        threading.Thread.__init__(self)
        self.i = i
        self.daemon = True
        self.start()
    def run(self):
        httpd = http.server.HTTPServer(addr, Handler, False)

        # Prevent the HTTP server from re-binding every handler.
        # https://stackoverflow.com/questions/46210672/
        httpd.socket = sock
        httpd.server_bind = self.server_close = lambda self: None

        httpd.serve_forever()

thr = Thread(0)

#################################################

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
     'codec': 'h264',
     's': f"{WIDTH}x{HEIGHT}"}, gpuID)

print(encoder)
print(encoder.PixelFormat())

###################################
# frame loop
###################################

encoded_length = 0

print( "video server running, try 'vlc http://<hostname>:%d/'"%PORT)

while True:
    i = ctx.frameIndex
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

    FBI.captureAsFormat(rtg,0,capbufNV12,10) # NV12
    as_np = np.array(capbufNV12, copy=False)
    encFrame = encoder.EncodeSingleFrame(as_np)
    if(encFrame.size):
        encByteArray = bytearray(encFrame)
        q.put(encByteArray,block=True,timeout=None)
        encoded_length += len(encByteArray)

    #############################################

    ctx.debugPopGroup()
