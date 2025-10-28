"""
Orkid Font Utilities

Provides font discovery, analysis, and atlas generation utilities
for the Orkid engine.
"""

import sys
import re
import freetype
from pathlib import Path
from typing import List, Dict, Optional

################################################################################
# Short Name Generation
################################################################################

def generate_short_name(family: str) -> str:
    """
    Generate a short, typeable name from font family name

    Algorithm:
    - Single word: take first 4 chars (lowercase)
    - Multiple words: take first 2 chars of each of first 2 words
    - This provides good disambiguation with short names

    Examples:
        "Inconsolata" → "inco"
        "Inconsolata SemiExpanded" → "inse"
        "Inconsolata UltraCondensed" → "inuc"
        "Inconsolata UltraExpanded" → "inul"
        "PT Mono" → "ptmo"
        "Menlo" → "menl"

    Args:
        family: Font family name

    Returns:
        Short name string (4 chars typically)
    """
    # Clean up the name
    family = family.strip()

    # Split into words (by spaces, hyphens, underscores)
    words = re.split(r'[\s\-_]+', family)

    # Further split camelCase/PascalCase words
    expanded_words = []
    for word in words:
        # Split on capital letters: "UltraCondensed" → ["Ultra", "Condensed"]
        sub_words = re.sub('([A-Z][a-z]+)', r' \1', re.sub('([A-Z]+)', r' \1', word)).split()
        expanded_words.extend(sub_words)

    # Remove empty strings
    expanded_words = [w for w in expanded_words if w]

    if len(expanded_words) == 0:
        return family[:4].lower()

    if len(expanded_words) == 1:
        # Single word: take first 4 chars for easy typing
        name = expanded_words[0].lower()
        return name[:4] if len(name) > 4 else name

    # Multiple words: take first 2 chars of each of first 2 words
    # "Inconsolata UltraCondensed" → ["Inconsolata", "Ultra", "Condensed"]
    # Take "in" from Inconsolata, "ul" from Ultra...
    # Actually, skip first word and use next two for variants
    if expanded_words[0].lower().startswith('incon'):  # Special case for Inconsolata
        # Use chars from position 1 and 2 (or 2 and 3 if only 2 total)
        if len(expanded_words) >= 3:
            result = expanded_words[0][:2].lower() + expanded_words[1][0].lower() + expanded_words[2][0].lower()
        elif len(expanded_words) == 2:
            result = expanded_words[0][:2].lower() + expanded_words[1][:2].lower()
        else:
            result = expanded_words[0][:4].lower()
    else:
        # Standard case: first 2 chars of first 2 words
        result = ''
        for word in expanded_words[:2]:
            if word:
                result += word[:2].lower()

    return result if result else expanded_words[0][:4].lower()

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

        # Generate short name
        short_name = generate_short_name(family)

        return {
            'path': font_path,
            'family': family,
            'style': style,
            'type': font_type,
            'spacing': spacing,
            'num_glyphs': num_glyphs,
            'short_name': short_name,
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

    # Resolve short name collisions
    monospace_fonts = _resolve_short_name_collisions(monospace_fonts)

    return monospace_fonts

def _resolve_short_name_collisions(fonts: List[Dict]) -> List[Dict]:
    """
    Resolve short name collisions by adding number suffixes

    Args:
        fonts: List of font info dictionaries with short_name

    Returns:
        Updated list with unique short names
    """
    # Simple approach: just add numbers to collisions
    final_names = {}

    for info in fonts:
        short = info['short_name']
        original_short = short

        # If collision, add number suffix
        counter = 1
        while short in final_names:
            counter += 1
            short = f"{original_short}{counter}"

        info['short_name'] = short
        final_names[short] = info

    return fonts

################################################################################

def find_font_by_name(font_name: str, search_dirs: List[str] = None,
                     monospace_only: bool = True) -> Optional[Dict]:
    """
    Find a font by family name or short name

    Args:
        font_name: Font family name, short name, or partial match (case-insensitive)
        search_dirs: Optional list of directories to search
        monospace_only: Only search monospace fonts

    Returns:
        Font info dictionary or None if not found
    """
    # Get all fonts with resolved short names (including collision numbers)
    if monospace_only:
        fonts = find_monospace_fonts(search_dirs)
    else:
        all_fonts = find_system_fonts(search_dirs)
        fonts = []
        for font_path in all_fonts:
            info = analyze_font(font_path)
            if info['success']:
                fonts.append(info)

    font_name_lower = font_name.lower()

    # First pass: try exact short name match
    for info in fonts:
        # Check for exact short name match (with or without numbers)
        if info.get('short_name', '').lower() == font_name_lower:
            return info

    # Second pass: try family name partial match
    for info in fonts:
        # Check for family name match (case-insensitive, partial)
        if font_name_lower in info['family'].lower():
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
        print(f"{'Short':<8} {'Family':<30} {'Style':<20} Path")
        print("=" * 110)

    for info in fonts:
        short = info.get('short_name', '')[:7]
        family = info['family'][:28]
        style = info['style'][:18]
        path = str(info['path'])
        print(f"{short:<8} {family:<30} {style:<20} {path}")

    if verbose:
        print(f"\nTotal: {len(fonts)} monospace fonts")
        print("\nUsage: Use 'Short' name or 'Family' name with --show")
