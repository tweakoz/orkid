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
