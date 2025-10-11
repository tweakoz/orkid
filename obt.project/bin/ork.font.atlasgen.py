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
    
    def generate_f2i_style_atlas(self, grid_size: int = 16) -> Tuple[np.ndarray, Dict]:
        """
        Generate atlas in F2IBuilder style layout (16x16 grid)
        
        Args:
            grid_size: Number of cells in grid (16 for 16x16 = 256 chars)
            
        Returns:
            Tuple of (atlas_image_array, metadata_dict)
        """
        # Calculate cell size based on font metrics
        self.face.load_char('M', freetype.FT_LOAD_RENDER)
        
        # Get maximum dimensions from font metrics
        max_width = (self.face.size.max_advance >> 6) + 2
        max_height = (self.face.size.height >> 6) + 2
        
        # Create atlas
        atlas_width = max_width * grid_size
        atlas_height = max_height * grid_size
        atlas = np.zeros((atlas_height, atlas_width), dtype=np.uint8)
        
        metadata = {
            'font_size': self.pixel_size,
            'grid_size': grid_size,
            'cell_width': max_width,
            'cell_height': max_height,
            'glyphs': {}
        }
        
        # Render each ASCII character in grid position
        for i in range(256):
            char = chr(i) if i >= 32 and i < 127 else ''  # Printable ASCII range
            if not char:
                continue
                
            # Calculate grid position
            grid_x = i % grid_size
            grid_y = i // grid_size
            
            # Calculate pixel position (centered in cell)
            cell_x = grid_x * max_width
            cell_y = grid_y * max_height
            
            # Load and render character
            try:
                self.face.load_char(char, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL)
                bitmap = self.face.glyph.bitmap
                
                if bitmap.width > 0 and bitmap.rows > 0:
                    # Center glyph in cell
                    offset_x = (max_width - bitmap.width) // 2 + self.face.glyph.bitmap_left
                    offset_y = max_height - (max_height - bitmap.rows) // 2 - self.face.glyph.bitmap_top
                    
                    # Ensure we don't go out of bounds
                    render_x = max(0, min(cell_x + offset_x, atlas_width - bitmap.width))
                    render_y = max(0, min(cell_y + offset_y, atlas_height - bitmap.rows))
                    
                    # Copy bitmap to atlas
                    buffer = np.array(bitmap.buffer, dtype=np.uint8).reshape(bitmap.rows, bitmap.width)
                    atlas[render_y:render_y+bitmap.rows, render_x:render_x+bitmap.width] = buffer
                    
                    metadata['glyphs'][char] = {
                        'index': i,
                        'x': render_x,
                        'y': render_y,
                        'width': bitmap.width,
                        'height': bitmap.rows,
                        'advance': self.face.glyph.advance.x >> 6
                    }
            except:
                pass  # Skip chars that can't be rendered
                
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
    # Standard ASCII charset
    ASCII_CHARSET = ''.join(chr(i) for i in range(32, 127))
    
    # Extended charset with common symbols
    EXTENDED_CHARSET = ASCII_CHARSET + "©®™€£¥°±×÷√∞∑∏∫≈≠≤≥"
    
    # Path to font (update this to your font path)
    # On Linux: often in /usr/share/fonts/
    # On Mac: often in /System/Library/Fonts/ or /Library/Fonts/
    font_path = "/System/Library/Fonts/Helvetica.ttc"  # Mac example
    # font_path = "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf"  # Linux example
    
    # Create generator
    gen = FontAtlasGenerator(font_path, pixel_size=16, dpi=96)
    
    # Method 1: Generate F2IBuilder-style grid atlas
    atlas, metadata = gen.generate_f2i_style_atlas(grid_size=16)
    save_atlas(atlas, metadata, "font_atlas_grid", add_grid=True)
    
    # Method 2: Generate packed atlas (more efficient)
    atlas2, metadata2 = gen.generate_atlas(
        charset=EXTENDED_CHARSET,
        atlas_width=512,
        atlas_height=512,
        padding=2,
        render_mode=freetype.FT_RENDER_MODE_NORMAL,
        enable_kerning=True
    )
    save_atlas(atlas2, metadata2, "font_atlas_packed")
    
    print("\nAtlas generation complete!")
    print(f"Grid atlas: {len(metadata['glyphs'])} glyphs")
    print(f"Packed atlas: {len(metadata2['glyphs'])} glyphs")
    if metadata2.get('kerning'):
        print(f"Kerning pairs: {len(metadata2['kerning'])}")