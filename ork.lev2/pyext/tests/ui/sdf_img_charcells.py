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

def renderCharCellsImage(img_width=2048, img_height=2048):
  """Render a 16x16 grid of ASCII characters (0-255)

  Args:
    img_width: Output image width
    img_height: Output image height
  """

  ###################################
  # Create synthesized image using ImageRenderer
  ###################################

  renderer = lev2.ImageRenderer(img_width, img_height)
  renderer.enable_bbox_optimization = True  # Enable bbox culling per character

  # Clear to dark gray background
  renderer.clear(vec4(0.2, 0.2, 0.2, 1.0))

  # Create white brush for glyph fill
  white_brush = lev2.ImageBrush(vec4(1.0, 1.0, 1.0, 1.0))

  # Create black pen for outline
  black_pen = lev2.ImagePen(vec4(0.0, 0.0, 0.0, 1.0), 0.5)

  # Grid parameters
  grid_cols = 16
  grid_rows = 16
  cell_width = img_width / grid_cols
  cell_height = img_height / grid_rows

  # Font configuration
  font_path = "/System/Library/Fonts/Supplemental/Georgia.ttf"

  # Scale factor to fit glyphs nicely in cells (adjust as needed)
  glyph_scale = cell_height * 0.00025  # Empirical scaling factor

  # Helper function to draw filled+outlined polygons from extracted font glyphs
  def draw_glyph_filled_outlined(contours, brush, pen):
    """Draw filled and outlined polygon from glyph contours"""
    if not contours:
      return
    # Convert to list of lists of vec2
    vec_contours = [[vec2(p[0], p[1]) for p in contour] for contour in contours]
    # Fill first
    renderer.fillPolygon(vec_contours, brush)
    # Draw outline on top
    renderer.strokePolygon(vec_contours, pen)

  # Render each character in the grid
  for char_code in range(256):
    # Calculate grid position
    col = char_code % grid_cols
    row = char_code // grid_cols

    # Calculate cell center position
    cell_center_x = (col + 0.5) * cell_width
    cell_center_y = (row + 0.5) * cell_height

    # Get the character (handle non-printable characters gracefully)
    try:
      char = chr(char_code)

      # Extract glyph contours from font
      contours = extract_glyph_contours_freetype(
        font_path,
        char,
        scale=glyph_scale,
        center_x=cell_center_x,
        center_y=cell_center_y
      )

      # Draw the glyph with white fill and black outline
      draw_glyph_filled_outlined(contours, white_brush, black_pen)

    except Exception as e:
      # Skip characters that can't be rendered
      pass

  # Return the rendered image
  return renderer.color_buffer

################################################################################

class CharCellsImageApp(ComponentizedApplication):
  def __init__(self, w, h):
    super().__init__()
    self.img_width = w
    self.img_height = h
    self.createEzApp(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    self.root = self.ezapp.topLayoutGroup

  def onGpuInit(self, ctx):
    super().onGpuInit(ctx)
    self.uicontext = self.ezapp.uicontext
    root = self.root

    # Render the character grid image
    rendered_image = renderCharCellsImage(img_width=self.img_width, img_height=self.img_height)

    # Create ImageView to display the rendered image
    img_view = root.makeChild(uiclass=lev2.ui.ImageView, args=["CharCellsImage", vec4(0)])
    self.img_view_widget = img_view.widget
    self.img_view_widget.generate_mipmaps = True
    self.img_view_widget.image = rendered_image
    self.img_view_widget.maintain_aspect_ratio = True
    img_view.layout.fill(root.layout)

################################################################################

def main():
  parser = argparse.ArgumentParser(description='Character Cells Image Renderer')
  parser.add_argument('-o', '--output', type=str, default=None,
                      help='Output PNG file path (headless mode - render and save without UI)')
  parser.add_argument('-d', '--dim', type=int, default=2048,
                      help='Output dimensions (width and height, default: 2048)')
  args = parser.parse_args()

  if args.output:
    coreappinit()
    # Headless mode - just render and save
    print(f"Rendering {args.dim}x{args.dim} character grid image in headless mode...")
    rendered_image = renderCharCellsImage(args.dim, args.dim)
    rendered_image.writeToFile(args.output)
    print(f"Rendered image written to: {args.output}")
    coreappexit()
  else:
    # Interactive mode - show UI
    app = CharCellsImageApp(args.dim, args.dim)
    app.ezapp.mainThreadLoop()
    app.ezapp.shutdown()

if __name__ == "__main__":
  main()
