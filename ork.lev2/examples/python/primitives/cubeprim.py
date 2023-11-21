#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

#pip3 install numpi Pillow

import numpy, time, sys
from PIL import Image
from orkengine.core import *
from orkengine.lev2 import *
tokens = CrcStringProxy()

l2exdir = (lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path

from common.primitives import createCubePrim

WIDTH = 2560
HEIGHT = 1440

class MyApp:
  def __init__(self):
    pass
  def onGpuInit(self):
    assert(False)


appi = MyApp()

ezapp = OrkEzApp.create(appi)
gfxenv = GfxEnv.ref
ctx = gfxenv.loadingContext()
FBI = ctx.FBI()
GBI = ctx.GBI()
print(ctx)
ctx.makeCurrent()
material = FreestyleMaterial()
material.gpuInit(ctx,Path("orkshader://solid"))
print(material)
print(material.shader)
print(material.shader.params)
print(material.shader.techniques)

par_mvp = material.shader.param("MatMVP")
par_tex = material.shader.param("ColorMap")

###################################
# load texture
###################################

texture = Texture.load("data://effect_textures/noise01")
lev2apppoll() # process opq

###################################
# create fx instance
###################################

pipeline = material.createFxInstance()
pipeline.technique = material.shader.technique("texvtxcolor_noalpha")
pipeline.param[par_mvp] = tokens.RCFD_Camera_MVP_Mono
#pipeline.param[param_v4parref] = self.v4parref
pipeline.param[par_tex] = texture

###################################

cubeprim = createCubePrim(ctx=ctx,size=1.0)
#pipeline_cube = createPipeline( app = self, ctx = ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd" )

###################################
# rtg setup
###################################

FBI.autoclear = True
FBI.autoclear = True
FBI.clearcolor = vec4(1,0,0,1)
rtg = ctx.defaultRTG()
rtb = rtg.buffer(0)
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

# todo - rework using pipeline
#material.bindTechnique(tek)
#RCFD = RenderContextFrameData(ctx)

#material.begin(RCFD)
#material.bindParamMatrix4(par_mvp,mvp_matrix)
#material.bindParamTexture(par_tex,texture)
#cubeprim.renderEML(ctx)
#material.end(RCFD)
#FBI.rtGroupPop()
#ctx.endFrame()

ok = FBI.captureAsFormat(rtb,capbuf,"RGBA8")
assert(ok)
as_np = numpy.array(capbuf,dtype=numpy.uint8).reshape( HEIGHT, WIDTH, 4 )
img = Image.fromarray(as_np, 'RGBA')
flipped = img.transpose(Image.FLIP_TOP_BOTTOM)
flipped.save("cubeprim.png")
