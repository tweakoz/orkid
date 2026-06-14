#!/usr/bin/env ork.python

################################################################################
# Chunked-upload TXI: verify the on_complete callbacks fire after each
# uploadTextureRegion's GPU work finishes, AND after finalizeUpload.
#
# Builds a 2D RGBA8 texture in NxN tile chunks (one uploadTextureRegion per
# tile). Each region's on_complete callback bumps a counter. finalizeUpload's
# on_complete flips a final state.
#
# Visual:
#   - Window shows the texture (a colored tile grid once uploads complete).
#   - Top-left label updates each frame with the live counter, e.g.:
#       "chunked_completion_sema — 64/64 region cbs fired  final: fired"
#
# Pass: counter reaches TOTAL_TILES and "final: fired" appears.
# Fail: counter sticks below TOTAL_TILES or "final: pending" never flips.
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

GRID         = 8                      # tiles per row/column
TILE         = 32                     # tile pixels (chunked region size)
DIM          = GRID * TILE            # texture dimension (256)
TOTAL_TILES  = GRID * GRID            # number of region uploads

def buildTileRGBA8(w, h, r, g, b):
  return bytes([r, g, b, 255]) * (w * h)

def hsv2rgb(h, s, v):
  i = int(h * 6.0) % 6
  f = h * 6.0 - int(h * 6.0)
  p = v * (1.0 - s)
  q = v * (1.0 - f * s)
  t = v * (1.0 - (1.0 - f) * s)
  r, g, b = [
      (v, t, p), (q, v, p), (p, v, t),
      (p, q, v), (t, p, v), (v, p, q)
  ][i]
  return int(r * 255), int(g * 255), int(b * 255)

class App(object):
  def __init__(self):
    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    self.root = self.ezapp.topLayoutGroup

    # Callback state — written from the GPU completion thread, read each
    # frame by the legend updater. Python int writes are atomic enough
    # for a counter.
    self.cb_count = 0
    self.final_fired = False
    self.legend = None

  def _makeCb(self, tile_index):
    def _cb():
      self.cb_count += 1
    return _cb

  def _finalCb(self):
    self.final_fired = True

  def _legendText(self):
    state = "fired" if self.final_fired else "pending"
    return (f"chunked_completion_sema — {self.cb_count}/{TOTAL_TILES} "
            f"region cbs fired   final: {state}")

  def onGpuInit(self, ctx):
    txi = ctx.TXI

    self.tex = lev2.Texture("chunked_sema")
    txi.reserveTexture(tex=self.tex, w=DIM, h=DIM, num_mips=1, fmt=tokens.RGBA8)

    for ty in range(GRID):
      for tx in range(GRID):
        idx = ty * GRID + tx
        hue = idx / float(TOTAL_TILES)
        r, g, b = hsv2rgb(hue, 0.85, 1.0)
        data = buildTileRGBA8(TILE, TILE, r, g, b)
        txi.uploadTextureRegion(
            tex=self.tex,
            mip_level=0, array_layer=0,
            offset_x=tx * TILE, offset_y=ty * TILE, offset_z=0,
            extent_w=TILE, extent_h=TILE, extent_d=1,
            data=data,
            on_complete=self._makeCb(idx))

    txi.finalizeUpload(tex=self.tex, on_complete=self._finalCb)

    img_view = self.root.makeChild(uiclass=lev2.ui.ImageView,
                                   args=["chunked_sema", vec4(0, 0, 0, 1)])
    self.img_view_widget = img_view.widget
    self.img_view_widget.texture = self.tex
    self.img_view_widget.maintain_aspect_ratio = True
    img_view.layout.fill(self.root.layout)

    legend_li = self.root.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["legend", vec4(0, 0, 0, 0.7), self._legendText()])
    self.legend = legend_li.widget
    legend_li.layout.setFixedRect(self.root.layout, 8, 8, 560, 28)

  def onUpdate(self, updinfo):
    if self.legend is not None:
      self.legend.text = self._legendText()

App().ezapp.mainThreadLoop()
