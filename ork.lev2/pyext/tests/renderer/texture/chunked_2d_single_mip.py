#!/usr/bin/env ork.python

################################################################################
# Chunked-upload TXI: 2D RGBA8 texture, 1 mip, single uploadTextureRegion
# covering the whole image. Visual proof: window shows the uploaded
# diagonal-gradient pattern.
#
# - top-left  -> red
# - top-right -> green
# - bot-left  -> blue
# - bot-right -> white
# Anything other than that bilinear color blend means the upload was wrong.
################################################################################

import math
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

WIDTH, HEIGHT = 256, 256

def buildGradientRGBA8(w, h):
  out = bytearray(w * h * 4)
  for y in range(h):
    fy = y / max(1, h - 1)
    for x in range(w):
      fx = x / max(1, w - 1)
      r = int((1.0 - fx) * (1.0 - fy) * 255 + fx * fy * 255)  # red→white diag
      g = int(fx * (1.0 - fy) * 255 + fx * fy * 255)          # green→white
      b = int((1.0 - fx) * fy * 255 + fx * fy * 255)          # blue→white
      i = (y * w + x) * 4
      out[i + 0] = r
      out[i + 1] = g
      out[i + 2] = b
      out[i + 3] = 255
  return bytes(out)

class App(object):
  def __init__(self):
    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    self.root = self.ezapp.topLayoutGroup

  def onGpuInit(self, ctx):
    txi = ctx.TXI

    self.tex = lev2.Texture("chunked_single_mip")
    txi.reserveTexture(tex=self.tex, w=WIDTH, h=HEIGHT, num_mips=1, fmt=tokens.RGBA8)

    data = buildGradientRGBA8(WIDTH, HEIGHT)
    txi.uploadTextureRegion(
        tex=self.tex,
        mip_level=0, array_layer=0,
        offset_x=0, offset_y=0, offset_z=0,
        extent_w=WIDTH, extent_h=HEIGHT, extent_d=1,
        data=data)
    txi.finalizeUpload(tex=self.tex)

    img_view = self.root.makeChild(uiclass=lev2.ui.ImageView,
                                   args=["chunked_single_mip", vec4(0, 0, 0, 1)])
    self.img_view_widget = img_view.widget
    self.img_view_widget.texture = self.tex
    self.img_view_widget.maintain_aspect_ratio = True
    img_view.layout.fill(self.root.layout)

App().ezapp.mainThreadLoop()
