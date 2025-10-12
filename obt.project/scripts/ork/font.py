"""
Orkid Font Utilities

Provides font discovery, analysis, and atlas generation utilities
for the Orkid engine.
"""

import sys
import freetype
from pathlib import Path
from typing import List, Dict

################################################################################
# Font Discovery
################################################################################

def find_system_fonts(search_dirs: List[str] = None) -> List[Path]:
    """
    Find all font files in system directories

    Args:
        search_dirs: Optional list of directories to search.
                    If None, uses platform defaults.

    Returns:
        List of Path objects to font files
    """
    font_paths = []

    # Use provided directories or platform defaults
    if search_dirs is None:
        if sys.platform == 'darwin':  # macOS
            search_dirs = [
                '/System/Library/Fonts',
                '/Library/Fonts',
                str(Path.home() / 'Library' / 'Fonts'),
            ]
        elif sys.platform.startswith('linux'):
            search_dirs = [
                '/usr/share/fonts',
                '/usr/local/share/fonts',
                str(Path.home() / '.fonts'),
                str(Path.home() / '.local' / 'share' / 'fonts'),
            ]
        else:  # Windows
            import os
            search_dirs = [
                str(Path(os.environ.get('WINDIR', 'C:\\Windows')) / 'Fonts'),
            ]

    # Font file extensions
    font_extensions = {'.ttf', '.otf', '.ttc', '.otc', '.dfont'}

    # Search directories
    for search_dir in search_dirs:
        search_path = Path(search_dir)
        if not search_path.exists():
            continue
        for ext in font_extensions:
            font_paths.extend(search_path.rglob(f'*{ext}'))

    return sorted(set(font_paths))

################################################################################

def analyze_font(font_path: Path) -> Dict:
    """
    Analyze a font file and extract metadata

    Args:
        font_path: Path to font file

    Returns:
        Dictionary with font metadata:
        - path: Path to font file
        - family: Font family name
        - style: Font style name
        - type: 'vector' or 'bitmap'
        - spacing: 'fixed' or 'proportional'
        - num_glyphs: Number of glyphs in font
        - success: True if analysis succeeded
    """
    try:
        face = freetype.Face(str(font_path))

        # Determine if font is scalable (vector) or bitmap
        is_scalable = bool(face.face_flags & freetype.FT_FACE_FLAG_SCALABLE)
        font_type = "vector" if is_scalable else "bitmap"

        # Determine if font is fixed-width (monospace)
        is_fixed = bool(face.face_flags & freetype.FT_FACE_FLAG_FIXED_WIDTH)
        spacing = "fixed" if is_fixed else "proportional"

        # Get font family and style
        family = face.family_name.decode('utf-8') if isinstance(face.family_name, bytes) else face.family_name
        style = face.style_name.decode('utf-8') if isinstance(face.style_name, bytes) else face.style_name

        # Get number of glyphs
        num_glyphs = face.num_glyphs

        return {
            'path': font_path,
            'family': family,
            'style': style,
            'type': font_type,
            'spacing': spacing,
            'num_glyphs': num_glyphs,
            'success': True
        }
    except Exception as e:
        return {
            'path': font_path,
            'error': str(e),
            'success': False
        }

################################################################################

def find_monospace_fonts(search_dirs: List[str] = None) -> List[Dict]:
    """
    Find all monospace fonts in system directories

    Args:
        search_dirs: Optional list of directories to search

    Returns:
        List of font info dictionaries (only monospace fonts)
    """
    fonts = find_system_fonts(search_dirs)
    monospace_fonts = []

    for font_path in fonts:
        info = analyze_font(font_path)
        if info['success'] and info['spacing'] == 'fixed':
            monospace_fonts.append(info)

    # Sort by family, then style
    monospace_fonts.sort(key=lambda x: (x['family'].lower(), x['style'].lower()))

    return monospace_fonts

################################################################################

def find_font_by_name(font_name: str, search_dirs: List[str] = None,
                     monospace_only: bool = True) -> Dict:
    """
    Find a font by family name

    Args:
        font_name: Font family name (case-insensitive, partial match)
        search_dirs: Optional list of directories to search
        monospace_only: Only search monospace fonts

    Returns:
        Font info dictionary or None if not found
    """
    fonts = find_system_fonts(search_dirs)

    for font_path in fonts:
        info = analyze_font(font_path)
        if not info['success']:
            continue

        # Skip non-monospace if requested
        if monospace_only and info['spacing'] != 'fixed':
            continue

        # Check for name match (case-insensitive)
        if font_name.lower() in info['family'].lower():
            return info

    return None

################################################################################

def list_monospace_fonts(search_dirs: List[str] = None, verbose: bool = True):
    """
    List all monospace fonts to stdout

    Args:
        search_dirs: Optional list of directories to search
        verbose: Print detailed output
    """
    if verbose:
        print("Scanning for monospace fonts...\n")

    fonts = find_monospace_fonts(search_dirs)

    if not fonts:
        print("No monospace fonts found")
        return

    if verbose:
        print(f"Found {len(fonts)} monospace fonts\n")
        print(f"{'Family':<30} {'Style':<20} Path")
        print("=" * 100)

    for info in fonts:
        family = info['family'][:28]
        style = info['style'][:18]
        path = str(info['path'])
        print(f"{family:<30} {style:<20} {path}")

    if verbose:
        print(f"\nTotal: {len(fonts)} monospace fonts")

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
    desc.char_width = pixel_size
    desc.char_height = pixel_size
    desc.char_offset_x = (metadata['cell_width'] - pixel_size) // 2
    desc.char_offset_y = (metadata['cell_height'] - pixel_size) // 2
    desc.y_shift = 0

    # Get advance width from actual glyph data
    if 'M' in metadata['glyphs']:
        desc.advance_width = metadata['glyphs']['M']['advance']
    else:
        desc.advance_width = pixel_size

    desc.advance_height = pixel_size

    return desc
