#!/usr/bin/env ork.python
"""
Font Atlas Generator using FreeType
Produces deterministic, high-quality rasterized font atlases
Works on Mac and Linux
"""

import numpy as np
import freetype
from PIL import Image
import json
import os
import argparse
from dataclasses import dataclass
from typing import List, Tuple, Dict, Optional

@dataclass
class GlyphInfo:
    """Metadata for a single glyph in the atlas"""
    char: str
    x: int
    y: int
    width: int
    height: int
    bearing_x: int
    bearing_y: int
    advance: int
    
class FontAtlasGenerator:
    def __init__(self, font_path: str, pixel_size: int = 16, dpi: int = 96):
        """
        Initialize font atlas generator
        
        Args:
            font_path: Path to TTF/OTF font file
            pixel_size: Target pixel size for font
            dpi: DPI for rendering (96 is standard screen DPI)
        """
        self.face = freetype.Face(font_path)
        
        # Set pixel size - this is crucial for deterministic results
        self.face.set_pixel_sizes(0, pixel_size)
        
        # Alternative: use set_char_size for more control
        # self.face.set_char_size(pixel_size * 64, 0, dpi, dpi)
        
        self.pixel_size = pixel_size
        self.dpi = dpi
        self.glyphs: Dict[str, GlyphInfo] = {}
        
    def generate_atlas(self, 
                      charset: str,
                      atlas_width: int = 512,
                      atlas_height: int = 512,
                      padding: int = 2,
                      render_mode: int = freetype.FT_RENDER_MODE_NORMAL,
                      enable_kerning: bool = True) -> Tuple[np.ndarray, Dict]:
        """
        Generate font atlas from character set
        
        Args:
            charset: String containing all characters to render
            atlas_width: Width of atlas texture
            atlas_height: Height of atlas texture
            padding: Padding between glyphs
            render_mode: FreeType render mode (NORMAL, LIGHT, MONO, LCD)
            enable_kerning: Enable kerning information
            
        Returns:
            Tuple of (atlas_image_array, metadata_dict)
        """
        
        # Create atlas buffer
        atlas = np.zeros((atlas_height, atlas_width), dtype=np.uint8)
        
        # Current position in atlas
        pen_x = padding
        pen_y = padding
        row_height = 0
        
        metadata = {
            'font_size': self.pixel_size,
            'dpi': self.dpi,
            'atlas_width': atlas_width,
            'atlas_height': atlas_height,
            'glyphs': {},
            'kerning': {}
        }
        
        # First pass: render all glyphs and pack into atlas
        for char in charset:
            # Load and render glyph
            self.face.load_char(char, freetype.FT_LOAD_RENDER | 
                              (freetype.FT_LOAD_TARGET_NORMAL if render_mode == freetype.FT_RENDER_MODE_NORMAL else 0))
            
            bitmap = self.face.glyph.bitmap
            
            # Skip empty glyphs (like space)
            if bitmap.width == 0 or bitmap.rows == 0:
                # Still store metrics for spacing
                metadata['glyphs'][char] = {
                    'x': 0,
                    'y': 0,
                    'width': 0,
                    'height': 0,
                    'bearing_x': self.face.glyph.bitmap_left,
                    'bearing_y': self.face.glyph.bitmap_top,
                    'advance': self.face.glyph.advance.x >> 6  # Convert from 26.6 fixed point
                }
                continue
            
            # Check if we need to move to next row
            if pen_x + bitmap.width + padding > atlas_width:
                pen_x = padding
                pen_y += row_height + padding
                row_height = 0
            
            # Check if we've run out of atlas space
            if pen_y + bitmap.rows + padding > atlas_height:
                raise ValueError(f"Atlas size {atlas_width}x{atlas_height} too small for charset")
            
            # Copy bitmap to atlas
            buffer = np.array(bitmap.buffer, dtype=np.uint8).reshape(bitmap.rows, bitmap.width)
            atlas[pen_y:pen_y+bitmap.rows, pen_x:pen_x+bitmap.width] = buffer
            
            # Store glyph metadata
            metadata['glyphs'][char] = {
                'x': pen_x,
                'y': pen_y,
                'width': bitmap.width,
                'height': bitmap.rows,
                'bearing_x': self.face.glyph.bitmap_left,
                'bearing_y': self.face.glyph.bitmap_top,
                'advance': self.face.glyph.advance.x >> 6
            }
            
            # Update position
            pen_x += bitmap.width + padding
            row_height = max(row_height, bitmap.rows)
        
        # Second pass: generate kerning table if requested
        if enable_kerning and self.face.has_kerning:
            for char1 in charset:
                for char2 in charset:
                    # Get kerning value
                    kerning = self.face.get_kerning(
                        self.face.get_char_index(char1),
                        self.face.get_char_index(char2),
                        freetype.FT_KERNING_DEFAULT
                    )
                    
                    kern_x = kerning.x >> 6  # Convert from 26.6 fixed point
                    if kern_x != 0:
                        key = f"{char1}{char2}"
                        metadata['kerning'][key] = kern_x
        
        return atlas, metadata
    
    def generate_f2i_style_atlas(self, grid_size: int = 16, ssaa: int = 1) -> Tuple[np.ndarray, Dict]:
        """
        Generate atlas in F2IBuilder style layout (16x16 grid)

        Args:
            grid_size: Number of cells in grid (16 for 16x16 = 256 chars)
            ssaa: Supersampling factor (1, 4, 9, 16, 25 for 1x, 2x, 3x, 4x, 5x)

        Returns:
            Tuple of (atlas_image_array, metadata_dict)
        """
        # Validate SSAA value
        valid_ssaa = [1, 4, 9, 16, 25]
        if ssaa not in valid_ssaa:
            raise ValueError(f"SSAA must be one of {valid_ssaa}, got {ssaa}")

        ssaa_scale = int(np.sqrt(ssaa))  # 1, 2, 3, 4, or 5
        # Calculate cell size based on font metrics at target resolution
        self.face.load_char('M', freetype.FT_LOAD_RENDER)

        # Get maximum dimensions from font metrics
        max_width = (self.face.size.max_advance >> 6)
        max_height = (self.face.size.height >> 6)

        # Use square cells with 12 pixels total margin (6 per side)
        cell_size = max(max_width, max_height) + 12

        # Create atlas at target resolution
        atlas_width = cell_size * grid_size
        atlas_height = cell_size * grid_size
        atlas = np.zeros((atlas_height, atlas_width), dtype=np.uint8)

        # If SSAA enabled, temporarily scale up font size for rendering
        original_pixel_size = self.pixel_size
        if ssaa > 1:
            self.face.set_pixel_sizes(0, self.pixel_size * ssaa_scale)
        
        metadata = {
            'font_size': self.pixel_size,
            'grid_size': grid_size,
            'cell_width': cell_size,
            'cell_height': cell_size,
            'glyphs': {}
        }
        
        # Render each ASCII character in grid position
        for i in range(256):
            # Printable ASCII (32-126) + Latin-1 Supplement (160-255)
            char = chr(i) if (i >= 32 and i < 127) or (i >= 160 and i < 256) else ''
            if not char:
                continue
                
            # Calculate grid position
            grid_x = i % grid_size
            grid_y = i // grid_size

            # Calculate pixel position (centered in cell with 6px margin)
            cell_x = grid_x * cell_size
            cell_y = grid_y * cell_size

            # Load and render character
            try:
                self.face.load_char(char, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL)
                bitmap = self.face.glyph.bitmap

                if bitmap.width > 0 and bitmap.rows > 0:
                    # Get bitmap buffer
                    buffer = np.array(bitmap.buffer, dtype=np.uint8).reshape(bitmap.rows, bitmap.width)

                    # If SSAA enabled, downsample with high-quality filter
                    if ssaa > 1:
                        # Convert to PIL Image for high-quality resampling
                        img_hi = Image.fromarray(buffer, mode='L')
                        target_width = bitmap.width // ssaa_scale
                        target_height = bitmap.rows // ssaa_scale
                        # Use LANCZOS for best quality downsampling
                        img_lo = img_hi.resize((target_width, target_height), Image.LANCZOS)
                        buffer = np.array(img_lo, dtype=np.uint8)

                        # Adjust bearing for downsampled size
                        bearing_x = self.face.glyph.bitmap_left // ssaa_scale
                        bearing_y = self.face.glyph.bitmap_top // ssaa_scale
                    else:
                        bearing_x = self.face.glyph.bitmap_left
                        bearing_y = self.face.glyph.bitmap_top

                    final_width = buffer.shape[1]
                    final_height = buffer.shape[0]

                    # Center glyph in cell with 6px margin on each side
                    offset_x = (cell_size - final_width) // 2 + bearing_x
                    offset_y = cell_size - (cell_size - final_height) // 2 - bearing_y

                    # Ensure we don't go out of bounds
                    render_x = max(0, min(cell_x + offset_x, atlas_width - final_width))
                    render_y = max(0, min(cell_y + offset_y, atlas_height - final_height))

                    # Copy bitmap to atlas
                    atlas[render_y:render_y+final_height, render_x:render_x+final_width] = buffer
                    
                    advance = (self.face.glyph.advance.x >> 6)
                    if ssaa > 1:
                        advance = advance // ssaa_scale

                    metadata['glyphs'][char] = {
                        'index': i,
                        'x': render_x,
                        'y': render_y,
                        'width': final_width,
                        'height': final_height,
                        'advance': advance
                    }
            except Exception as e:
                pass  # Skip chars that can't be rendered

        # Restore original font size
        if ssaa > 1:
            self.face.set_pixel_sizes(0, original_pixel_size)

        return atlas, metadata

def save_atlas(atlas: np.ndarray, metadata: Dict, output_prefix: str, 
               add_grid: bool = False, grid_color: Tuple[int, int, int] = (0, 255, 0)):
    """
    Save atlas image and metadata
    
    Args:
        atlas: Numpy array of atlas image
        metadata: Dictionary of atlas metadata
        output_prefix: Prefix for output files (will create .png and .json)
        add_grid: Add debug grid overlay
        grid_color: RGB color for grid
    """
    # Convert to RGBA for better compatibility
    img = Image.fromarray(atlas, mode='L').convert('RGBA')
    
    # Optionally add grid overlay (for debugging)
    if add_grid and 'cell_width' in metadata and 'cell_height' in metadata:
        import numpy as np
        from PIL import ImageDraw
        
        draw = ImageDraw.Draw(img)
        
        # Draw vertical lines
        for x in range(0, img.width, metadata['cell_width']):
            draw.line([(x, 0), (x, img.height)], fill=grid_color + (128,), width=1)
            
        # Draw horizontal lines  
        for y in range(0, img.height, metadata['cell_height']):
            draw.line([(0, y), (img.width, y)], fill=grid_color + (128,), width=1)
    
    # Save image
    img.save(f"{output_prefix}.png")
    
    # Save metadata
    with open(f"{output_prefix}.json", 'w') as f:
        json.dump(metadata, f, indent=2)
    
    print(f"Saved atlas to {output_prefix}.png and {output_prefix}.json")

# Example usage
if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Generate font atlas textures with optional SSAA',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                              # Generate with default settings
  %(prog)s --ssaa 4                     # Generate with 2x SSAA
  %(prog)s --ssaa 16 --pixel-size 24    # Generate 24px font with 4x SSAA
  %(prog)s --font /path/to/font.ttf     # Use custom font
        """
    )

    parser.add_argument('--font', type=str,
                       default="/System/Library/Fonts/Helvetica.ttc",
                       help='Path to font file (default: Helvetica.ttc)')
    parser.add_argument('--pixel-size', type=int, default=16,
                       help='Font size in pixels (default: 16)')
    parser.add_argument('--ssaa', type=int, choices=[1, 4, 9, 16, 25], default=1,
                       help='Supersampling anti-aliasing: 1=off, 4=2x, 9=3x, 16=4x, 25=5x (default: 1)')
    parser.add_argument('--output', type=str, default="font_atlas_grid",
                       help='Output file prefix (default: font_atlas_grid)')
    parser.add_argument('--no-grid', action='store_true',
                       help='Disable debug grid overlay')

    args = parser.parse_args()

    # Standard ASCII charset
    ASCII_CHARSET = ''.join(chr(i) for i in range(32, 127))

    # Extended charset with common symbols
    EXTENDED_CHARSET = ASCII_CHARSET + "©®™€£¥°±×÷√∞∑∏∫≈≠≤≥"

    # Create generator
    print(f"Generating font atlas:")
    print(f"  Font: {args.font}")
    print(f"  Size: {args.pixel_size}px")
    print(f"  SSAA: {args.ssaa}x ({int(np.sqrt(args.ssaa))}x scale)" if args.ssaa > 1 else f"  SSAA: disabled")

    gen = FontAtlasGenerator(args.font, pixel_size=args.pixel_size, dpi=96)

    # Generate F2IBuilder-style grid atlas
    atlas, metadata = gen.generate_f2i_style_atlas(grid_size=16, ssaa=args.ssaa)
    save_atlas(atlas, metadata, args.output, add_grid=not args.no_grid)

    print("\nAtlas generation complete!")
    print(f"  Output: {args.output}.png / {args.output}.json")
    print(f"  Glyphs: {len(metadata['glyphs'])}")
    print(f"  Atlas size: {atlas.shape[1]}x{atlas.shape[0]}")
    print(f"  Cell size: {metadata['cell_width']}x{metadata['cell_height']}")