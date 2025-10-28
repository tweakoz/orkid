"""
Font Atlas Utilities

Shared functions for font atlas generation, rendering, and persistence.
"""

import numpy as np
import freetype
import json
from PIL import Image, ImageDraw
from pathlib import Path
from typing import Dict, Tuple, Optional

# Import Orkid
from orkengine.core import vec2, vec4
from orkengine import lev2
from ork.font.tools_freetype import extract_glyph_contours_freetype

################################################################################
# C++ Metadata Generation
################################################################################

def generate_fcpp_metadata(metadata: Dict, font_name: str, font_var_name: str,
                          texture_path: str) -> str:
    """
    Generate C++ FontDesc metadata format

    Args:
        metadata: Atlas metadata dictionary
        font_name: Short font name (e.g., "i24")
        font_var_name: C++ variable name (e.g., "Inconsolata24")
        texture_path: Texture path (e.g., "lev2://textures/Inconsolata24")

    Returns:
        String containing C++ code
    """
    # Get atlas dimensions
    tex_width = metadata['atlas_width']
    tex_height = metadata['atlas_height']
    cell_width = metadata['cell_width']
    cell_height = metadata['cell_height']
    pixel_size = metadata['font_size']

    # Calculate typical character metrics from a sample character ('M' or 'A')
    sample_char = None
    for ch in ['M', 'A', 'W', 'a']:
        if ch in metadata['glyphs']:
            sample_char = metadata['glyphs'][ch]
            break

    if sample_char:
        advance_width = sample_char['advance']
    else:
        # Fallback to cell size
        advance_width = cell_width

    # Use pixel_size for character dimensions
    char_width = pixel_size
    char_height = pixel_size
    advance_height = pixel_size

    # Calculate offsets (margin around character in cell)
    char_offset_x = (cell_width - char_width) // 2
    char_offset_y = (cell_height - char_height) // 2

    # Y shift (typically small adjustment)
    y_shift = 0

    # Generate C++ code
    cpp_code = f"""  FontDesc {font_var_name};
  {font_var_name}.mFontName       = "{font_name}";
  {font_var_name}.mFontFile       = "{texture_path}";
  {font_var_name}.miTexWidth      = {tex_width};
  {font_var_name}.miTexHeight     = {tex_height};
  {font_var_name}.miCellWidth     = {cell_width};
  {font_var_name}.miCellHeight    = {cell_height};
  {font_var_name}.miCharWidth     = {char_width};
  {font_var_name}.miCharHeight    = {char_height};
  {font_var_name}.miCharOffsetX   = {char_offset_x};
  {font_var_name}.miCharOffsetY   = {char_offset_y};
  {font_var_name}.miYShift        = {y_shift};
  {font_var_name}.miAdvanceWidth  = {advance_width};
  {font_var_name}.miAdvanceHeight = {advance_height};
"""
    return cpp_code

################################################################################
# Atlas Persistence
################################################################################

def save_atlas(atlas: np.ndarray, metadata: Dict, output_prefix: str,
               add_grid: bool = False, grid_color: Tuple[int, int, int] = (0, 255, 0),
               font_name: str = None, texture_path: str = None):
    """
    Save atlas image and metadata

    Args:
        atlas: Numpy array of atlas image
        metadata: Dictionary of atlas metadata
        output_prefix: Prefix for output files (will create .png and .json)
        add_grid: Add debug grid overlay
        grid_color: RGB color for grid
        font_name: Short font name for .fcpp output (e.g., "i24")
        texture_path: Texture path for .fcpp output (e.g., "lev2://textures/Inconsolata24")
    """
    # Convert to RGBA for better compatibility
    img = Image.fromarray(atlas, mode='L').convert('RGBA')

    # Optionally add grid overlay (for debugging)
    if add_grid and 'cell_width' in metadata and 'cell_height' in metadata:
        draw = ImageDraw.Draw(img)

        # Draw vertical lines
        for x in range(0, img.width, metadata['cell_width']):
            draw.line([(x, 0), (x, img.height)], fill=grid_color + (128,), width=1)

        # Draw horizontal lines
        for y in range(0, img.height, metadata['cell_height']):
            draw.line([(0, y), (img.width, y)], fill=grid_color + (128,), width=1)

    # Save image (compress_level=0 for uncompressed PNG to avoid any artifacts)
    img.save(f"{output_prefix}.png", compress_level=0)

    # Save JSON metadata
    with open(f"{output_prefix}.json", 'w') as f:
        json.dump(metadata, f, indent=2)

    # Save C++ metadata if font_name provided
    if font_name and texture_path:
        # Extract variable name from output prefix (last part of path)
        var_name = Path(output_prefix).name
        # Capitalize first letter for C++ convention
        if var_name:
            var_name = var_name[0].upper() + var_name[1:]

        fcpp_code = generate_fcpp_metadata(metadata, font_name, var_name, texture_path)

        with open(f"{output_prefix}.fcpp", 'w') as f:
            f.write(fcpp_code)

        print(f"Saved atlas to {output_prefix}.png, {output_prefix}.json, and {output_prefix}.fcpp")
    else:
        print(f"Saved atlas to {output_prefix}.png and {output_prefix}.json")

################################################################################
# SDF Atlas Generation
################################################################################

def generate_sdf_atlas(font_path, pixel_size):
    """
    Generate SDF font atlas using ImageRenderer

    Args:
        font_path: Path to font file
        pixel_size: Target font size

    Returns:
        Tuple of (atlas_image, metadata_dict)
    """
    # Grid parameters (16x16 for 256 chars)
    grid_cols = 16
    grid_rows = 16

    # Calculate cell size - make it slightly larger than pixel size for padding
    cell_size = int(pixel_size * 1.5)  # 1.5x for some padding

    # Atlas dimensions
    atlas_width = cell_size * grid_cols
    atlas_height = cell_size * grid_rows

    # Create renderer
    renderer = lev2.ImageRenderer(atlas_width, atlas_height)
    renderer.enable_bbox_optimization = True

    # Clear background
    renderer.clear(vec4(0, 0, 0, 0))  # Transparent background

    # Create brushes/pens
    white_brush = lev2.ImageBrush(vec4(1.0, 1.0, 1.0, 1.0))
    black_pen = lev2.ImagePen(vec4(0.0, 0.0, 0.0, 1.0), 0.5)

    # Scale factor for glyph to fit in cell
    glyph_scale = cell_size * 0.00025  # Empirical scaling

    # Load font for metrics
    face = freetype.Face(str(font_path))
    face.set_pixel_sizes(0, pixel_size)

    # Get baseline metrics from 'M' character
    face.load_char('M', freetype.FT_LOAD_DEFAULT)
    advance_width = face.glyph.advance.x >> 6

    # Initialize metadata
    metadata = {
        'font_size': pixel_size,
        'grid_size': grid_cols,
        'atlas_width': atlas_width,
        'atlas_height': atlas_height,
        'cell_width': cell_size,
        'cell_height': cell_size,
        'glyphs': {}
    }

    # Helper to draw glyph
    def draw_glyph(contours, brush, pen):
        if not contours:
            return
        vec_contours = [[vec2(p[0], p[1]) for p in contour] for contour in contours]
        renderer.fillPolygon(vec_contours, brush)
        renderer.strokePolygon(vec_contours, pen)

    # Render each character
    for char_code in range(256):
        # Grid position
        col = char_code % grid_cols
        row = char_code // grid_cols

        # Cell center
        cell_center_x = (col + 0.5) * cell_size
        cell_center_y = (row + 0.5) * cell_size

        try:
            char = chr(char_code)

            # Get glyph metrics for metadata
            face.load_char(char, freetype.FT_LOAD_DEFAULT)
            glyph_advance = face.glyph.advance.x >> 6

            # Extract and render contours
            contours = extract_glyph_contours_freetype(
                str(font_path),
                char,
                scale=glyph_scale,
                center_x=cell_center_x,
                center_y=cell_center_y
            )

            draw_glyph(contours, white_brush, black_pen)

            # Store glyph metadata (simplified for grid-based atlas)
            metadata['glyphs'][char] = {
                'x': col * cell_size,
                'y': row * cell_size,
                'width': cell_size,
                'height': cell_size,
                'bearing_x': 0,
                'bearing_y': 0,
                'advance': glyph_advance if glyph_advance > 0 else advance_width
            }

        except Exception as e:
            # Skip characters that can't be rendered
            # Still add placeholder metadata
            metadata['glyphs'][chr(char_code)] = {
                'x': col * cell_size,
                'y': row * cell_size,
                'width': 0,
                'height': 0,
                'bearing_x': 0,
                'bearing_y': 0,
                'advance': advance_width
            }

    return renderer.color_buffer, metadata

################################################################################
# FontDesc Utilities
################################################################################

def populate_fontdesc_from_metadata(desc, metadata: Dict, font_id: str,
                                   texture_path: str, pixel_size: int):
    """
    Populate a lev2.FontDesc from atlas metadata

    Args:
        desc: lev2.FontDesc object to populate
        metadata: Dictionary from atlas JSON
        font_id: Font identifier string
        texture_path: Absolute path to texture (without extension)
        pixel_size: Font pixel size

    Returns:
        The populated FontDesc object
    """
    desc.fontname = font_id
    desc.fontfile = texture_path
    desc.tex_width = metadata['atlas_width']
    desc.tex_height = metadata['atlas_height']
    desc.cell_width = metadata['cell_width']
    desc.cell_height = metadata['cell_height']
    desc.char_width = pixel_size+4
    desc.char_height = pixel_size+4
    desc.char_offset_x = (metadata['cell_width'] - pixel_size) // 2
    desc.char_offset_y = (metadata['cell_height'] - pixel_size) // 2
    desc.y_shift = 0

    # Get advance width from actual glyph data
    if 'M' in metadata['glyphs']:
        desc.advance_width = metadata['glyphs']['M']['advance']
    else:
        desc.advance_width = pixel_size

    desc.advance_height = pixel_size+2

    return desc
