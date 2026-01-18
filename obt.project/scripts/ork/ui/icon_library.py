################################################################################
# Icon Library - SVG to Image factory
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import tempfile
import os
from obt import command
from orkengine import core
from orkengine import lev2

################################################################################

def text_icon(text, width=24, height=24, font_size=12, color="#E6E6E6"):
  """
  Create an image with centered text.

  Args:
    text: Text to render
    width: Output width in pixels
    height: Output height in pixels
    font_size: Font size in SVG units
    color: Text fill color (hex string)

  Returns:
    lev2.Image (image_ptr_t)
  """
  cx = int(width // 2)
  cy = int(height // 2)
  svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {int(width)} {int(height)}">
    <text x="{cx}" y="{cy}" text-anchor="middle" dominant-baseline="central" font-family="sans-serif" font-size="{font_size}" font-weight="bold" fill="{color}">{text}</text>
  </svg>'''
  return from_svg_string(svg, int(width), int(height))

################################################################################

def from_svg_string(svg_string, width, height):
  """
  Render an SVG string to an RGBA image.

  Args:
    svg_string: SVG markup as a string
    width: Output width in pixels
    height: Output height in pixels

  Returns:
    lev2.Image (image_ptr_t)
  """
  with tempfile.NamedTemporaryFile(suffix='.svg', delete=False) as svg_file:
    svg_file.write(svg_string.encode('utf-8'))
    svg_path = svg_file.name

  try:
    return from_svg_file(svg_path, width, height)
  finally:
    os.unlink(svg_path)

################################################################################

def from_svg_file(svg_path, width, height):
  """
  Render an SVG file to an RGBA image.

  Args:
    svg_path: Path to SVG file
    width: Output width in pixels
    height: Output height in pixels

  Returns:
    lev2.Image (image_ptr_t)
  """
  with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as png_file:
    png_path = png_file.name

  try:
    cmd = [
      'rsvg-convert',
      '-w', str(width),
      '-h', str(height),
      '-f', 'png',
      '-o', png_path,
      svg_path
    ]
    command.run(cmd, do_log=False)
    return lev2.Image.createFromFile(png_path)
  finally:
    if os.path.exists(png_path):
      os.unlink(png_path)

################################################################################

def crosshairs_icon(width=24, height=24, color="#E6E6E6", stroke_width=1.5):
  """
  Create a crosshairs/origin icon.

  Args:
    width: Output width in pixels
    height: Output height in pixels
    color: Stroke color (hex string)
    stroke_width: Line thickness

  Returns:
    lev2.Image (image_ptr_t)
  """
  cx = width / 2
  cy = height / 2
  # Leave margin for the lines
  margin = 3
  # Small circle radius at center
  circle_r = 2.5
  svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}">
    <!-- Horizontal line -->
    <line x1="{margin}" y1="{cy}" x2="{width - margin}" y2="{cy}" stroke="{color}" stroke-width="{stroke_width}" stroke-linecap="round"/>
    <!-- Vertical line -->
    <line x1="{cx}" y1="{margin}" x2="{cx}" y2="{height - margin}" stroke="{color}" stroke-width="{stroke_width}" stroke-linecap="round"/>
    <!-- Center circle -->
    <circle cx="{cx}" cy="{cy}" r="{circle_r}" fill="none" stroke="{color}" stroke-width="{stroke_width}"/>
  </svg>'''
  return from_svg_string(svg, int(width), int(height))

################################################################################

def provider_from_svg_string(svg_string, width, height):
  """
  Create an ImageProvider that lazily renders an SVG string.

  Args:
    svg_string: SVG markup as a string
    width: Output width in pixels
    height: Output height in pixels

  Returns:
    lev2.ImageProvider
  """
  return lev2.ImageProvider.createFromLambda(
    lambda: from_svg_string(svg_string, width, height)
  )

################################################################################

def provider_from_svg_file(svg_path, width, height):
  """
  Create an ImageProvider that lazily renders an SVG file.

  Args:
    svg_path: Path to SVG file
    width: Output width in pixels
    height: Output height in pixels

  Returns:
    lev2.ImageProvider
  """
  return lev2.ImageProvider.createFromLambda(
    lambda: from_svg_file(svg_path, width, height)
  )

################################################################################
