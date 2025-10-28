"""Font utilities for Orkid"""

# Export font discovery functions
from .discovery import (
    generate_short_name,
    find_system_fonts,
    analyze_font,
    find_monospace_fonts,
    find_font_by_name,
    list_monospace_fonts,
)

# Export font atlas functions
from .atlas import (
    generate_fcpp_metadata,
    save_atlas,
    generate_sdf_atlas,
    populate_fontdesc_from_metadata,
)

__all__ = [
    # Discovery
    'generate_short_name',
    'find_system_fonts',
    'analyze_font',
    'find_monospace_fonts',
    'find_font_by_name',
    'list_monospace_fonts',
    # Atlas
    'generate_fcpp_metadata',
    'save_atlas',
    'generate_sdf_atlas',
    'populate_fontdesc_from_metadata',
]
