#!/usr/bin/env ork.python

################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication

tokens = CrcStringProxy()
################################################################################

class SdfImageTestApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.createEzApp(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    self.root = self.ezapp.topLayoutGroup

  ##############################################

  def onGpuInit(self, ctx):
    super().onGpuInit(ctx)

    self.uicontext = self.ezapp.uicontext

    # Create root layout group
    root = self.root

    ###################################
    # Create synthesized image using ImageRenderer
    ###################################

    img_width = 512
    img_height = 512
    renderer = lev2.ImageRenderer(img_width, img_height)

    # Clear to dark background
    renderer.clear(vec4(0.15, 0.15, 0.2, 1.0))

    # Create brushes and pens
    red_brush = lev2.ImageBrush(vec4(1.0, 0.2, 0.2, 1.0))
    blue_brush = lev2.ImageBrush(vec4(0.2, 0.4, 1.0, 1.0))
    yellow_brush = lev2.ImageBrush(vec4(1.0, 0.9, 0.2, 1.0))

    white_pen = lev2.ImagePen(vec4(1.0, 1.0, 1.0, 1.0), 3.0)
    cyan_pen = lev2.ImagePen(vec4(0.2, 1.0, 1.0, 1.0), 2.0)

    # Draw some shapes
    renderer.fillCircle(vec2(256, 256), 180, blue_brush)
    renderer.strokeCircle(vec2(256, 256), 180, white_pen)

    renderer.fillBox(vec2(150, 150), vec2(80, 80), red_brush, 10)
    renderer.strokeBox(vec2(150, 150), vec2(80, 80), white_pen, 10)

    renderer.fillBox(vec2(362, 150), vec2(80, 80), yellow_brush, 10)
    renderer.strokeBox(vec2(362, 150), vec2(80, 80), white_pen, 10)

    # Draw some lines
    renderer.strokeLine(vec2(100, 400), vec2(412, 400), cyan_pen)
    renderer.strokeLine(vec2(256, 300), vec2(256, 450), cyan_pen)

    ###################################
    # Create ImageView to display the rendered image
    ###################################

    img_view = root.makeChild(uiclass=lev2.ui.ImageView, args=["SdfImage", vec4(0)])
    img_view_widget = img_view.widget
    img_view_widget.generate_mipmaps = True
    img_view_widget.image = renderer.color_buffer
    img_view_widget.maintain_aspect_ratio = True
    img_view.layout.fill(root.layout)

################################################################################

def main():
  app = SdfImageTestApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()

if __name__ == "__main__":
  main()
