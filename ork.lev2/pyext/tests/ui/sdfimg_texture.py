#!/usr/bin/env ork.python

################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, argparse
from orkengine.core import vec2, vec3, vec4, CrcStringProxy, mtx3, coreappinit, coreappexit
from orkengine import lev2
from ork.app.application import ComponentizedApplication

tokens = CrcStringProxy()
################################################################################

def renderTexturedImage(img_width=512, img_height=512):
  """Render a textured image using ImageRenderer and return it"""

  ###################################
  # Create test texture (checkerboard pattern)
  ###################################

  tex_size = 128
  test_texture = lev2.Image()
  test_texture.initWithFormat(tex_size, tex_size, tokens.RGBA32F)

  # Fill with checkerboard pattern
  checker_size = 16
  for y in range(tex_size):
    for x in range(tex_size):
      checker_x = (x // checker_size) % 2
      checker_y = (y // checker_size) % 2
      is_white = (checker_x + checker_y) % 2 == 0

      pixel = test_texture.pixel32f(x, y)
      if is_white:
        pixel[0] = 1.0  # R
        pixel[1] = 0.8  # G
        pixel[2] = 0.6  # B
        pixel[3] = 1.0  # A
      else:
        pixel[0] = 0.2  # R
        pixel[1] = 0.4  # G
        pixel[2] = 0.8  # B
        pixel[3] = 1.0  # A

  ###################################
  # Create another texture (radial gradient)
  ###################################

  gradient_texture = lev2.Image()
  gradient_texture.initWithFormat(tex_size, tex_size, tokens.RGBA32F)

  center_x = tex_size / 2.0
  center_y = tex_size / 2.0
  max_radius = tex_size / 2.0

  for y in range(tex_size):
    for x in range(tex_size):
      dx = x - center_x
      dy = y - center_y
      dist = math.sqrt(dx*dx + dy*dy)
      t = min(dist / max_radius, 1.0)

      pixel = gradient_texture.pixel32f(x, y)
      pixel[0] = 1.0 - t  # R (bright in center)
      pixel[1] = 0.5      # G
      pixel[2] = t        # B (bright at edges)
      pixel[3] = 1.0      # A

  ###################################
  # Create synthesized image using ImageRenderer with textures
  # Using abstract coordinate space (0-512) that scales to actual resolution
  ###################################

  abstract_width = 512.0  # Virtual coordinate space
  abstract_height = 512.0

  renderer = lev2.ImageRenderer(img_width, img_height)

  # Clear to dark background first
  renderer.clear(vec4(0.15, 0.15, 0.2, 1.0))

  # Set up view transform to map abstract coordinates to actual pixel space
  # This makes the code resolution-independent
  view_mtx = mtx3()
  view_mtx.setScale(img_width / abstract_width, img_height / abstract_height, 1.0)
  renderer.pushTransform(view_mtx)

  # Create textured brushes
  checker_brush = lev2.ImageBrush()
  checker_brush.use_texture = True
  checker_brush.texture = test_texture

  # Create sampler
  sampler = lev2.ImageSampler()
  sampler.wrap_mode = tokens.REPEAT
  checker_brush.sampler = sampler

  # Set texture matrix to map abstract coordinates to texture coordinates
  scale = 0.01  # Scale world coords to texture coords
  tex_mtx = mtx3()
  tex_mtx.setScale(scale, scale, 1.0)
  checker_brush.texture_matrix = tex_mtx

  # Create gradient brush
  gradient_brush = lev2.ImageBrush()
  gradient_brush.use_texture = True
  gradient_brush.texture = gradient_texture
  gradient_brush.sampler = sampler

  # Scale gradient texture to fit shape
  grad_scale = 1.0 / 180.0  # Map 180-unit radius to 0-1 texture coords
  grad_mtx = mtx3()
  grad_mtx.setScale(grad_scale, grad_scale, 1.0)
  # Set translation in column 2 (for 2D affine transform)
  grad_mtx.setColumn(2, vec3(-256.0 * grad_scale, -256.0 * grad_scale, 1.0))  # Center at (256, 256)
  gradient_brush.texture_matrix = grad_mtx

  # Create solid brushes for comparison
  red_brush = lev2.ImageBrush(vec4(1.0, 0.2, 0.2, 1.0))

  # Create pens (widths in abstract units)
  white_pen = lev2.ImagePen(vec4(1.0, 1.0, 1.0, 1.0), 3.0)
  cyan_pen = lev2.ImagePen(vec4(0.2, 1.0, 1.0, 1.0), 2.0)

  # Draw in abstract coordinate space (same as original pixel coordinates)
  # The view transform will scale these to the actual output resolution

  # Draw textured circle with gradient
  renderer.fillCircle(vec2(256, 256), 180, gradient_brush)
  renderer.strokeCircle(vec2(256, 256), 180, white_pen)

  # Draw textured boxes with checkerboard
  renderer.fillBox(vec2(150, 150), vec2(80, 80), checker_brush, 10)
  renderer.strokeBox(vec2(150, 150), vec2(80, 80), white_pen, 10)

  renderer.fillBox(vec2(362, 150), vec2(80, 80), checker_brush, 10)
  renderer.strokeBox(vec2(362, 150), vec2(80, 80), white_pen, 10)

  # Draw solid colored shapes for contrast
  renderer.fillBox(vec2(150, 362), vec2(60, 60), red_brush, 8)
  renderer.strokeBox(vec2(150, 362), vec2(60, 60), cyan_pen, 8)

  # Draw some stroked lines
  renderer.strokeLine(vec2(100, 400), vec2(412, 400), cyan_pen)

  # Return the rendered image
  return renderer.color_buffer

################################################################################

class SdfImageTestApp(ComponentizedApplication):
  def __init__(self,w,h):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    self.root = self.ezapp.topLayoutGroup
    self.img_width = w
    self.img_height = h

  def onGpuInit(self, ctx):
    super().onGpuInit(ctx)
    self.uicontext = self.ezapp.uicontext
    root = self.root

    # Render the image
    rendered_image = renderTexturedImage(img_width=self.img_width, img_height=self.img_height)

    # Create ImageView to display the rendered image
    img_view = root.makeChild(uiclass=lev2.ui.ImageView, args=["SdfTexturedImage", vec4(0)])
    img_view_widget = img_view.widget
    img_view_widget.generate_mipmaps = True
    img_view_widget.image = rendered_image
    img_view_widget.maintain_aspect_ratio = True
    img_view.layout.fill(root.layout)

################################################################################

def main():
  parser = argparse.ArgumentParser(description='SDF Image Renderer Texture Test')
  parser.add_argument('-o', '--output', type=str, default=None,
                      help='Output PNG file path (headless mode - render and save without UI)')
  parser.add_argument('-d', '--dim', type=int, default=512,
                      help='Output dimensions (width and height, default: 512)')
  args = parser.parse_args()

  if args.output:
    coreappinit()
    # Headless mode - just render and save
    print(f"Rendering {args.dim}x{args.dim} image in headless mode...")
    rendered_image = renderTexturedImage(args.dim, args.dim)
    rendered_image.writeToFile(args.output)
    print(f"Rendered image written to: {args.output}")
    coreappexit()
  else:
    # Interactive mode - show UI
    SdfImageTestApp(args.dim, args.dim).ezapp.mainThreadLoop()

if __name__ == "__main__":
  main()
