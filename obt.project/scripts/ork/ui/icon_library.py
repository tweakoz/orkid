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

_svg_cache = {}

# A PNG always opens with this 8-byte signature. rsvg-convert either writes a
# complete PNG or (when absent/failing — legitimate on a headless node without
# librsvg) leaves the pre-created temp file 0 bytes. An empty/truncated file fed
# to Image.createFromFile reads a short datablock and trips a FATAL engine assert
# (force-segfault). So every raster result is signature-checked before it is
# loaded or persisted, and a missing rasterizer degrades to a placeholder icon.
_PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

################################################################################

def _is_valid_png_bytes(data):
  return len(data) >= len(_PNG_SIGNATURE) and bytes(data[:len(_PNG_SIGNATURE)]) == _PNG_SIGNATURE

def _is_valid_png_file(path):
  try:
    with open(path, 'rb') as f:
      return _is_valid_png_bytes(f.read(len(_PNG_SIGNATURE)))
  except OSError:
    return False

def _placeholder_icon(width, height):
  """Transparent stand-in used when rsvg-convert cannot produce a glyph (e.g. a
  headless node without librsvg). The toolbar button still lays out and responds
  — it just shows no icon — a graceful degrade instead of a fatal empty-image load."""
  return lev2.Image.createRGBA8FromColor(int(width), int(height), core.vec4(0, 0, 0, 0))

def _render_svg_to_png(svg_path, width, height, png_path):
  """Rasterize an SVG file to png_path via rsvg-convert. Returns True iff a valid
  PNG landed. rsvg-convert being absent or failing is a legitimate, non-fatal
  outcome (the caller falls back to a placeholder icon)."""
  cmd = ['rsvg-convert', '-w', str(width), '-h', str(height), '-f', 'png', '-o', png_path, svg_path]
  try:
    command.run(cmd, do_log=False)
  except Exception as e:
    print(f"[icon_library] rsvg-convert unavailable ({e}); using placeholder icon", flush=True)
    return False
  if not _is_valid_png_file(png_path):
    print("[icon_library] rsvg-convert produced no/invalid PNG; using placeholder icon", flush=True)
    return False
  return True

################################################################################

def _dblock_key(svg_string, width, height):
  """Compute a persistent cache key for an SVG string at a given size."""
  crc = core.Crc64Context()
  crc.accum("svg-icon:1")
  crc.accum(svg_string)
  crc.accum(f"{width}x{height}")
  crc.finish()
  return crc.result

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

def from_svg_string_auto(svg_string, width):
  """
  Render an SVG string to an RGBA image, preserving aspect ratio.
  Width is fixed; height is derived from the SVG viewBox.
  Falls back to square if viewBox is absent or square.

  Args:
    svg_string: SVG markup as a string
    width: Output width in pixels

  Returns:
    lev2.Image (image_ptr_t)
  """
  height = width  # default: square
  vb_start = svg_string.find('viewBox="')
  if vb_start >= 0:
    vb_start += len('viewBox="')
    vb_end = svg_string.find('"', vb_start)
    parts = svg_string[vb_start:vb_end].split()
    if len(parts) == 4:
      vb_w = float(parts[2])
      vb_h = float(parts[3])
      if vb_w > 0:
        height = int(width * vb_h / vb_w)
  return from_svg_string(svg_string, width, height)

################################################################################

def from_svg_string(svg_string, width, height):
  """
  Render an SVG string to an RGBA image.
  Uses DataBlockCache for persistent on-disk caching (avoids rsvg-convert on repeat runs)
  and an in-memory dict for within-session reuse.

  Args:
    svg_string: SVG markup as a string
    width: Output width in pixels
    height: Output height in pixels

  Returns:
    lev2.Image (image_ptr_t)
  """
  dblock_key = _dblock_key(svg_string, width, height)

  # Level 1: in-memory cache (same session)
  cached = _svg_cache.get(dblock_key)
  if cached is not None:
    return cached

  # Level 2: persistent DataBlockCache (across sessions)
  cached_dblock = core.DataBlockCache.findDataBlock(dblock_key)
  if cached_dblock is not None:
    png_bytes = bytes(cached_dblock.bytes)
    if _is_valid_png_bytes(png_bytes):
      with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as tmp:
        tmp.write(png_bytes)
        tmp_path = tmp.name
      try:
        result = lev2.Image.createFromFile(tmp_path)
      finally:
        os.unlink(tmp_path)
      _svg_cache[dblock_key] = result
      return result
    # A prior run whose rsvg-convert was absent/failing can persist an EMPTY entry
    # here; loading it would trip the fatal short-datablock assert. Evict the
    # poisoned entry and fall through to regenerate (self-heals the cache).
    core.DataBlockCache.removeDataBlock(dblock_key)

  # Level 3: generate via rsvg-convert (single invocation)
  with tempfile.NamedTemporaryFile(suffix='.svg', delete=False) as svg_file:
    svg_file.write(svg_string.encode('utf-8'))
    svg_path = svg_file.name
  with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as png_file:
    png_path = png_file.name

  try:
    if not _render_svg_to_png(svg_path, width, height, png_path):
      # No rasterizer — degrade to a placeholder rather than crash or poison the
      # persistent cache with empty bytes.
      result = _placeholder_icon(width, height)
      _svg_cache[dblock_key] = result
      return result
    # Cache the valid PNG bytes for next run
    try:
      png_dblock = core.DataBlock.createFromFile(core.Path(png_path))
      core.DataBlockCache.setDataBlock(dblock_key, png_dblock)
    except Exception:
      pass
    result = lev2.Image.createFromFile(png_path)
    _svg_cache[dblock_key] = result
    return result
  finally:
    os.unlink(svg_path)
    if os.path.exists(png_path):
      os.unlink(png_path)

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
    if not _render_svg_to_png(svg_path, width, height, png_path):
      return _placeholder_icon(width, height)
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
