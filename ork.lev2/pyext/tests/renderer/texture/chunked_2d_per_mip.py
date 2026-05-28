#!/usr/bin/env ork.python

################################################################################
# Chunked-upload TXI: 2D RGBA8 texture with full mip pyramid. Each mip is
# uploaded as its own region with a distinct tint, so when you clamp the
# sampler to a specific mip you can see the color change.
#
# Press keys 0..8 to clamp the sampler to a single mip via minLod=maxLod=N.
# A label in the top-left shows the current mip and the legend.
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

BASE_DIM = 256

MIP_TINTS = [
    (255,   0,   0),  # mip0 red
    (255, 128,   0),  # mip1 orange
    (255, 255,   0),  # mip2 yellow
    (  0, 255,   0),  # mip3 green
    (  0, 255, 255),  # mip4 cyan
    (  0,   0, 255),  # mip5 blue
    (128,   0, 255),  # mip6 violet
    (255,   0, 255),  # mip7 magenta
    (255, 255, 255),  # mip8 white
]

def numMipsFor(dim):
  m = 1
  d = dim
  while d > 1:
    d = d >> 1
    m += 1
  return m

def buildMipRGBA8(w, h, tint):
  # checker in the tint color so the inter-mip distinction is unmistakable
  r, g, b = tint
  out = bytearray(w * h * 4)
  cell = max(1, w // 8)
  for y in range(h):
    for x in range(w):
      on = ((x // cell) ^ (y // cell)) & 1
      mr = r if on else r // 3
      mg = g if on else g // 3
      mb = b if on else b // 3
      i = (y * w + x) * 4
      out[i + 0] = mr
      out[i + 1] = mg
      out[i + 2] = mb
      out[i + 3] = 255
  return bytes(out)

class App(object):
  def __init__(self):
    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    self.root = self.ezapp.topLayoutGroup
    self.current_mip = 0
    self.num_mips = numMipsFor(BASE_DIM)
    self.ctx = None

  def _legendText(self):
    return (f"chunked_2d_per_mip — press 1..9,0 to clamp sampler  "
            f"(1=mip0, 9=mip8, 0=mip9; current: mip {self.current_mip})")

  def _applyMip(self, mip):
    if mip < 0 or mip >= self.num_mips:
      return
    self.current_mip = mip
    self.tex.setMipRange(mip, mip)
    self.ctx.TXI.applySamplingMode(self.tex)
    self.legend.text = self._legendText()
    print(self._legendText())

  def onGpuInit(self, ctx):
    self.ctx = ctx
    txi = ctx.TXI

    self.tex = lev2.Texture("chunked_per_mip")
    txi.reserveTexture(tex=self.tex, w=BASE_DIM, h=BASE_DIM,
                       num_mips=self.num_mips, fmt=tokens.RGBA8)

    for level in range(self.num_mips):
      mw = max(1, BASE_DIM >> level)
      mh = max(1, BASE_DIM >> level)
      tint = MIP_TINTS[level % len(MIP_TINTS)]
      data = buildMipRGBA8(mw, mh, tint)
      txi.uploadTextureRegion(
          tex=self.tex,
          mip_level=level, array_layer=0,
          offset_x=0, offset_y=0, offset_z=0,
          extent_w=mw, extent_h=mh, extent_d=1,
          data=data)

    txi.finalizeUpload(tex=self.tex)

    # Image viewer fills the window
    img_view = self.root.makeChild(uiclass=lev2.ui.ImageView,
                                   args=["chunked_per_mip", vec4(0, 0, 0, 1)])
    self.img_view_widget = img_view.widget
    self.img_view_widget.texture = self.tex
    self.img_view_widget.maintain_aspect_ratio = True
    img_view.layout.fill(self.root.layout)

    # Top-left legend overlay
    legend_li = self.root.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["legend", vec4(0, 0, 0, 0.7), self._legendText()])
    self.legend = legend_li.widget
    legend_li.layout.setFixedRect(self.root.layout, 8, 8, 560, 28)

    # Establish the initial sampler-clamp to mip 0
    self._applyMip(0)

  def onUiEvent(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed:
      kc = uievent.keycode
      if kc >= ord('0') and kc <= ord('9'):
        # Rotate: '1'->mip0, '2'->mip1, ..., '9'->mip8, '0'->mip9
        mip = (kc - ord('0') + 9) % 10
        self._applyMip(mip)
        return lev2.ui.HandlerResult()
    return lev2.ui.HandlerResult()

App().ezapp.mainThreadLoop()
