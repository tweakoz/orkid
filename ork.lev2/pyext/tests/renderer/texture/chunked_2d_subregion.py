#!/usr/bin/env ork.python

################################################################################
# Chunked-upload TXI: 2D RGBA8 texture filled via 4 separate
# uploadTextureRegion calls — one per quadrant. Each call writes to a
# different (offset_x, offset_y) of the same mip 0.
#
# Visual proof: window shows 4 solid-color quadrants:
#   top-left   = red
#   top-right  = green
#   bot-left   = blue
#   bot-right  = yellow
# If any quadrant is wrong color or missing, the subregion path is broken.
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

DIM       = 256
HALF      = DIM // 2

QUADRANTS = [
    # (offset_x, offset_y, r, g, b)
    (   0,    0, 255,   0,   0),  # TL red
    (HALF,    0,   0, 255,   0),  # TR green
    (   0, HALF,   0,   0, 255),  # BL blue
    (HALF, HALF, 255, 255,   0),  # BR yellow
]

def buildSolidRGBA8(w, h, r, g, b):
  return bytes([r, g, b, 255]) * (w * h)

class App(object):
  def __init__(self):
    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    self.root = self.ezapp.topLayoutGroup

  def onGpuInit(self, ctx):
    txi = ctx.TXI

    self.tex = lev2.Texture("chunked_subregion")
    txi.reserveTexture(tex=self.tex, w=DIM, h=DIM, num_mips=1, fmt=tokens.RGBA8)

    for (ox, oy, r, g, b) in QUADRANTS:
      data = buildSolidRGBA8(HALF, HALF, r, g, b)
      txi.uploadTextureRegion(
          tex=self.tex,
          mip_level=0, array_layer=0,
          offset_x=ox, offset_y=oy, offset_z=0,
          extent_w=HALF, extent_h=HALF, extent_d=1,
          data=data)

    txi.finalizeUpload(tex=self.tex)

    img_view = self.root.makeChild(uiclass=lev2.ui.ImageView,
                                   args=["chunked_subregion", vec4(0, 0, 0, 1)])
    self.img_view_widget = img_view.widget
    self.img_view_widget.texture = self.tex
    self.img_view_widget.maintain_aspect_ratio = True
    img_view.layout.fill(self.root.layout)

App().ezapp.mainThreadLoop()
