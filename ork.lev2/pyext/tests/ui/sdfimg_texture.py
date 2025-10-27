#!/usr/bin/env ork.python

################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, argparse
from orkengine.core import vec2, vec3, vec4, quat, CrcStringProxy, mtx3, coreappinit, coreappexit
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.font.tools_freetype import extract_glyph_contours_freetype

tokens = CrcStringProxy()
################################################################################

def renderTexturedImage(img_width=512, img_height=512, time=0.0):
  """Render a textured image using ImageRenderer and return it

  Args:
    img_width: Output image width
    img_height: Output image height
    rotation: Rotation angle in radians for checkerboard patterns
  """

  ###################################
  # Create test texture (checkerboard pattern)
  ###################################

  tex_size = 512
  test_texture = lev2.Image()
  test_texture.initWithFormat(tex_size, tex_size, tokens.RGBA32F)

  # Fill with checkerboard pattern
  checker_size = 64
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
      pixel[0] = 0.0           # R (no red)
      pixel[1] = 0.0           # G (no green)
      pixel[2] = 0.3 + t * 0.5 # B (dark blue in center, brighter at edges)
      pixel[3] = 1.0           # A

  ###################################
  # Create synthesized image using ImageRenderer with textures
  # Using abstract coordinate space (0-512) that scales to actual resolution
  ###################################

  abstract_width = 512.0  # Virtual coordinate space
  abstract_height = 512.0

  renderer = lev2.ImageRenderer(img_width, img_height)
  renderer.enable_bbox_optimization = False  # Disable bbox optimization for testing

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
  # Rotate around center of coordinate space (256, 256)
  scale = 0.006  # Scale world coords to texture coords
  center_x = 256.0
  center_y = 256.0

  # Translate to center
  t1 = mtx3()
  t1.translation = vec2(-center_x, -center_y)

  # Rotate
  r = mtx3()
  q = quat(vec3(0,0,1), time*4.0)
  r.fromQuaternion(q)

  # Scale
  s = mtx3()
  s.setScale(scale, scale, 1.0)

  # Compose: Scale * Rotate * Translate
  checker_brush.texture_matrix = s * r * t1

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
  navy_brush = lev2.ImageBrush(vec4(0.0, 0.0, 0.5, 1.0))

  # Create pens (widths in abstract units)
  white_pen = lev2.ImagePen(vec4(1.0, 1.0, 1.0, 1.0), 3.0)
  cyan_pen = lev2.ImagePen(vec4(0.2, 1.0, 1.0, 1.0), 2.0)
  hotpink_pen = lev2.ImagePen(vec4(1.0, 0.41, 0.71, 1.0), 2.0)

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

  renderer.fillBox(vec2(362, 362), vec2(60, 60), navy_brush, 8)
  renderer.strokeBox(vec2(362, 362), vec2(60, 60), hotpink_pen, 8)

  # Draw some stroked lines
  renderer.strokeLine(vec2(100, 256), vec2(412, 256), cyan_pen)

  ###################################
  # Create dark checkerboard texture for glyph fill
  ###################################

  glyph_tex_size = 64
  glyph_texture = lev2.Image()
  glyph_texture.initWithFormat(glyph_tex_size, glyph_tex_size, tokens.RGBA32F)

  # Fill with dark checkerboard pattern (rotated 45 degrees via sampling)
  checker_size = 42  # 30% bigger than 32
  for y in range(glyph_tex_size):
    for x in range(glyph_tex_size):
      # Rotate coordinates by 45 degrees for diagonal checkerboard
      rx = x * 0.707 + y * 0.707
      ry = -x * 0.707 + y * 0.707
      checker_x = (int(rx) // checker_size) % 2
      checker_y = (int(ry) // checker_size) % 2
      is_black = (checker_x + checker_y) % 2 == 0

      pixel = glyph_texture.pixel32f(x, y)
      if is_black:
        pixel[0] = 0.05  # R (almost black)
        pixel[1] = 0.05  # G
        pixel[2] = 0.05  # B
        pixel[3] = 1.0   # A
      else:
        pixel[0] = 0.3   # R (grey - 50% brighter)
        pixel[1] = 0.3   # G
        pixel[2] = 0.3   # B
        pixel[3] = 1.0   # A

  ###################################
  # Draw fancy glyphs from font
  ###################################

  # Helper function to draw filled+outlined polygons from extracted font glyphs with glow
  def draw_glyph_filled_outlined(contours, brush, pen):
    """Draw filled and outlined polygon from glyph contours with glow effect using separable convolution"""
    # Convert to list of lists of vec2
    vec_contours = [[vec2(p[0], p[1]) for p in contour] for contour in contours]

    # Fill first
    renderer.fillPolygon(vec_contours, brush)
    # Finally draw the sharp bright outline on top
    renderer.strokePolygon(vec_contours, pen)

  # Create checkerboard brush for glyph fill
  glyph_brush = lev2.ImageBrush()
  glyph_brush.use_texture = True
  glyph_brush.texture = glyph_texture
  glyph_brush.sampler = sampler

  # Set texture matrix to map glyph coordinates to texture coordinates
  # Use smaller scale for tighter checkerboard pattern with rotation around center
  glyph_scale = 0.05

  # Translate to center
  t1_glyph = mtx3()
  t1_glyph.translation = vec2(-center_x, -center_y)
  t1_glyph2 = mtx3()
  t1_glyph2.translation = vec2(center_x, center_y)

  # Rotate
  r_glyph = mtx3()
  q = quat(vec3(0,0,1), time*3.0)
  r_glyph.fromQuaternion(q)

  # Scale
  s_glyph = mtx3()
  s_glyph.setScale(glyph_scale*0.3, glyph_scale*0.3, 1.0)

  # Compose: Scale * Rotate * Translate
  glyph_brush.texture_matrix = s_glyph * t1_glyph2 * r_glyph * t1_glyph

  # Create brightest yellow pen for outline
  yellow_pen = lev2.ImagePen(vec4(1.1, 1.1, 0.0, 1.0), 1.5)

  # Extract omega (Ω) from Georgia font
  # Centered horizontally, positioned below center
  omega_cx = 256
  omega_cy = 330
  omega_scale = 0.04  # Scale factor to get nice size from font units

  font_path = "/System/Library/Fonts/Supplemental/Georgia.ttf"
  omega_contours = extract_glyph_contours_freetype(font_path, 'Ω', scale=omega_scale, center_x=omega_cx, center_y=omega_cy)
  draw_glyph_filled_outlined(omega_contours, glyph_brush, yellow_pen)

  # Extract phi (Φ) from Georgia font
  # Centered horizontally, positioned above center
  phi_cx = 256
  phi_cy = 180
  phi_scale = 0.04  # Scale factor to get nice size from font units

  phi_contours = extract_glyph_contours_freetype(font_path, 'Φ', scale=phi_scale, center_x=phi_cx, center_y=phi_cy)
  draw_glyph_filled_outlined(phi_contours, glyph_brush, yellow_pen)


  kernel_size = 15
  sigma = 3.0  # Standard deviation
  falloff_power = 1.0  # NEW: Controls nonlinearity (1.0 = normal Gaussian, >1.0 = sharper, <1.0 = softer)

  kernel = []
  for i in range(kernel_size):
    x = i - kernel_size // 2
    # Generate Gaussian weight
    weight = math.exp(-abs(x) / sigma)

    # Apply falloff power for nonlinear control
    #weight = weight ** falloff_power

    kernel.append(weight)

  # Normalize kernel
  #kernel_sum = sum(kernel)
  #kernel = [k / kernel_sum for k in kernel]
  
  # Apply separable convolution to blur the whole buffer
  # Use threshold to only blur pixels with significant alpha (ignore RGB)
  #threshold = vec4(0.75, 0.75, 0, 0)  # Only check alpha > 0.01
  #blurred = renderer.color_buffer.separableConvolve(kernel, threshold)

  # Replace buffer with blurred version
  #renderer.color_buffer = blurred

  # Return the rendered image
  return renderer.color_buffer

################################################################################

class SdfImageTestApp(ComponentizedApplication):
  def __init__(self,w,h,animate=False):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    self.root = self.ezapp.topLayoutGroup
    self.img_width = w
    self.img_height = h
    self.animate = animate
    self.rotation_angle = 0.0

  def onGpuInit(self, ctx):
    super().onGpuInit(ctx)
    self.uicontext = self.ezapp.uicontext
    root = self.root

    # Render the image
    rendered_image = renderTexturedImage(img_width=self.img_width, img_height=self.img_height, time=0.0)

    # Create ImageView to display the rendered image
    img_view = root.makeChild(uiclass=lev2.ui.ImageView, args=["SdfTexturedImage", vec4(0)])
    self.img_view_widget = img_view.widget
    self.img_view_widget.generate_mipmaps = True
    self.img_view_widget.image = rendered_image
    self.img_view_widget.maintain_aspect_ratio = True
    img_view.layout.fill(root.layout)

  def onUpdate(self, updinfo):
    super().onUpdate(updinfo)
    if self.animate:
      # Increment rotation angle
      self.rotation_angle += updinfo.deltatime * 0.5  # 0.5 radians per second

      # Re-render with new rotation
      rendered_image = renderTexturedImage(img_width=self.img_width, img_height=self.img_height, time=updinfo.absolutetime)
      self.img_view_widget.image = rendered_image

################################################################################

def main():
  parser = argparse.ArgumentParser(description='SDF Image Renderer Texture Test')
  parser.add_argument('-o', '--output', type=str, default=None,
                      help='Output PNG file path (headless mode - render and save without UI)')
  parser.add_argument('-d', '--dim', type=int, default=512,
                      help='Output dimensions (width and height, default: 512)')
  parser.add_argument('-a', '--animate', action='store_true',
                      help='Enable animation in headed mode (rotates checkerboards)')
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
    SdfImageTestApp(args.dim, args.dim, args.animate).ezapp.mainThreadLoop()

if __name__ == "__main__":
  main()
