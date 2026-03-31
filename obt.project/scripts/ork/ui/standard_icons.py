################################################################################
# Standard Icons - SVG icon definitions for UI
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

from ork.ui import icon_library

################################################################################
# Theme colors (matching style.cpp defaults)
################################################################################

ICON_COLOR = "#E6E6E6"        # 0.9 gray - primary icon color
ICON_COLOR_DIM = "#808080"    # 0.5 gray - dimmed/secondary
ACCENT_COLOR = "#4D99CC"      # (0.3, 0.6, 0.8) - blue accent
ACCENT_HIGHLIGHT = "#80B3E6"  # lighter accent for highlights

################################################################################
# SVG Templates
################################################################################

def _svg_wrap(content, viewbox="0 0 24 24"):
  return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{viewbox}">{content}</svg>'

################################################################################
# Filesystem Icons
################################################################################

SVG_FILE = _svg_wrap(f'''
  <path d="M6 2C4.9 2 4 2.9 4 4v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6H6z"
        fill="{ICON_COLOR}" fill-opacity="0.9"/>
  <path d="M14 2v6h6" fill="none" stroke="{ICON_COLOR_DIM}" stroke-width="1"/>
''')

SVG_FILE_IMAGE = _svg_wrap(f'''
  <path d="M6 2C4.9 2 4 2.9 4 4v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6H6z"
        fill="{ICON_COLOR}" fill-opacity="0.9"/>
  <path d="M14 2v6h6" fill="none" stroke="{ICON_COLOR_DIM}" stroke-width="1"/>
  <circle cx="10" cy="12" r="2" fill="{ACCENT_COLOR}"/>
  <path d="M7 18l3-4 2 2 4-5 3 4v3H7z" fill="{ACCENT_COLOR}"/>
''')

SVG_FILE_CODE = _svg_wrap(f'''
  <path d="M6 2C4.9 2 4 2.9 4 4v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6H6z"
        fill="{ICON_COLOR}" fill-opacity="0.9"/>
  <path d="M14 2v6h6" fill="none" stroke="{ICON_COLOR_DIM}" stroke-width="1"/>
  <path d="M9 13l-2 2 2 2M15 13l2 2-2 2M11 11l2 6"
        fill="none" stroke="{ACCENT_COLOR}" stroke-width="1.5" stroke-linecap="round"/>
''')

SVG_FILE_TEXT = _svg_wrap(f'''
  <path d="M6 2C4.9 2 4 2.9 4 4v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6H6z"
        fill="{ICON_COLOR}" fill-opacity="0.9"/>
  <path d="M14 2v6h6" fill="none" stroke="{ICON_COLOR_DIM}" stroke-width="1"/>
  <path d="M7 12h10M7 15h10M7 18h6" stroke="{ICON_COLOR_DIM}" stroke-width="1.5" stroke-linecap="round"/>
''')

SVG_FOLDER = _svg_wrap(f'''
  <path d="M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"
        fill="{ACCENT_COLOR}"/>
''')

SVG_FOLDER_OPEN = _svg_wrap(f'''
  <path d="M20 6h-8l-2-2H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2z"
        fill="{ACCENT_COLOR}" fill-opacity="0.7"/>
  <path d="M4 8h16v10H4z" fill="{ACCENT_COLOR}"/>
''')

SVG_DRIVE = _svg_wrap(f'''
  <path d="M4 6c-1.1 0-2 .9-2 2v8c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2H4z"
        fill="{ICON_COLOR}" fill-opacity="0.9"/>
  <circle cx="18" cy="12" r="1.5" fill="{ACCENT_COLOR}"/>
  <path d="M5 12h8" stroke="{ICON_COLOR_DIM}" stroke-width="2" stroke-linecap="round"/>
''')

SVG_HOME = _svg_wrap(f'''
  <path d="M12 3L2 12h3v8h6v-6h2v6h6v-8h3L12 3z" fill="{ICON_COLOR}"/>
''')

SVG_PARENT = _svg_wrap(f'''
  <path d="M12 4L4 12h4v7h8v-7h4L12 4z" fill="{ICON_COLOR}"/>
''')

################################################################################
# Transport Icons
################################################################################

SVG_PLAY = _svg_wrap(f'''
  <path d="M8 5v14l11-7L8 5z" fill="{ICON_COLOR}"/>
''')

SVG_PAUSE = _svg_wrap(f'''
  <rect x="6" y="4" width="4" height="16" rx="1" fill="{ICON_COLOR}"/>
  <rect x="14" y="4" width="4" height="16" rx="1" fill="{ICON_COLOR}"/>
''')

SVG_STOP = _svg_wrap(f'''
  <rect x="5" y="5" width="14" height="14" rx="2" fill="{ICON_COLOR}"/>
''')

SVG_REWIND = _svg_wrap(f'''
  <path d="M11 12L22 5v14l-11-7zM2 12l11-7v14L2 12z" fill="{ICON_COLOR}"/>
''')

SVG_FAST_FORWARD = _svg_wrap(f'''
  <path d="M13 12L2 5v14l11-7zM22 12L11 5v14l11-7z" fill="{ICON_COLOR}"/>
''')

SVG_SKIP_BACK = _svg_wrap(f'''
  <path d="M19 5v14l-11-7 11-7z" fill="{ICON_COLOR}"/>
  <rect x="5" y="5" width="3" height="14" rx="1" fill="{ICON_COLOR}"/>
''')

SVG_SKIP_FORWARD = _svg_wrap(f'''
  <path d="M5 5v14l11-7L5 5z" fill="{ICON_COLOR}"/>
  <rect x="16" y="5" width="3" height="14" rx="1" fill="{ICON_COLOR}"/>
''')

SVG_RECORD = _svg_wrap(f'''
  <circle cx="12" cy="12" r="8" fill="#CC4D4D"/>
''')

SVG_BAKE_LIGHTING = _svg_wrap(f'''
  <path d="M12 2C8.7 2 6 4.7 6 8c0 2.1 1.1 3.9 2.7 5H9v3c0 .6.4 1 1 1h4c.6 0 1-.4 1-1v-3h.3C16.9 11.9 18 10.1 18 8c0-3.3-2.7-6-6-6z"
        fill="#CCAA4D"/>
  <path d="M10 19h4M10.5 21h3" stroke="#CCAA4D" stroke-width="1.5" stroke-linecap="round"/>
  <path d="M12 0v1.5M4.9 2.9l1 1M1.5 8.5H3M21 8.5h1.5M18.1 2.9l-1 1"
        stroke="#CCAA4D" stroke-width="1.2" stroke-linecap="round"/>
''')

SVG_ENVMAP_STUDIO = _svg_wrap(f'''
  <path d="M4 20L10 4" stroke="#CCAA4D" stroke-width="2" stroke-linecap="round"/>
  <path d="M10 4l2-2 2 2-2 2-2-2z" fill="#CCAA4D"/>
  <circle cx="7" cy="12" r="1" fill="#CCAA4D"/>
  <circle cx="14" cy="9" r="1" fill="#CCAA4D"/>
  <circle cx="11" cy="15" r="1" fill="#CCAA4D"/>
  <circle cx="17" cy="13" r="1" fill="#CCAA4D"/>
  <circle cx="15" cy="18" r="1" fill="#CCAA4D"/>
  <circle cx="19" cy="7" r="1" fill="#CCAA4D"/>
''')

SVG_LOOP = _svg_wrap(f'''
  <path d="M12 4V1L8 5l4 4V6c3.3 0 6 2.7 6 6 0 1-.3 2-.8 2.8l1.5 1.5c.8-1.2 1.3-2.7 1.3-4.3 0-4.4-3.6-8-8-8z" fill="{ICON_COLOR}"/>
  <path d="M12 18c-3.3 0-6-2.7-6-6 0-1 .3-2 .8-2.8L5.3 7.7C4.5 8.9 4 10.4 4 12c0 4.4 3.6 8 8 8v3l4-4-4-4v3z" fill="{ICON_COLOR}"/>
''')

################################################################################
# Action Icons
################################################################################

SVG_NEW = _svg_wrap(f'''
  <path d="M6 2C4.9 2 4 2.9 4 4v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6H6z"
        fill="{ICON_COLOR}" fill-opacity="0.9"/>
  <path d="M14 2v6h6" fill="none" stroke="{ICON_COLOR_DIM}" stroke-width="1"/>
  <path d="M12 10v8M8 14h8" stroke="{ACCENT_COLOR}" stroke-width="2" stroke-linecap="round"/>
''')

SVG_OPEN = _svg_wrap(f'''
  <path d="M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"
        fill="{ACCENT_COLOR}"/>
  <path d="M12 11v6M9 14l3-3 3 3" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
''')

SVG_SAVE = _svg_wrap(f'''
  <path d="M17 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V7l-4-4z" fill="{ICON_COLOR}"/>
  <path d="M17 3v4H7V3" fill="{ICON_COLOR_DIM}"/>
  <rect x="7" y="12" width="10" height="6" rx="1" fill="{ACCENT_COLOR}"/>
''')

SVG_CLOSE = _svg_wrap(f'''
  <circle cx="12" cy="12" r="10" fill="{ICON_COLOR}" fill-opacity="0.2"/>
  <path d="M8 8l8 8M16 8l-8 8" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round"/>
''')

SVG_DELETE = _svg_wrap(f'''
  <path d="M6 6v14c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V6H6z" fill="#CC4D4D"/>
  <path d="M4 6h16M10 6V4h4v2" fill="none" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round"/>
''')

SVG_REFRESH = _svg_wrap(f'''
  <path d="M12 4V1L8 5l4 4V6c3.3 0 6 2.7 6 6s-2.7 6-6 6-6-2.7-6-6H4c0 4.4 3.6 8 8 8s8-3.6 8-8-3.6-8-8-8z"
        fill="{ICON_COLOR}"/>
''')

SVG_SEARCH = _svg_wrap(f'''
  <circle cx="10" cy="10" r="6" fill="none" stroke="{ICON_COLOR}" stroke-width="2"/>
  <path d="M14 14l6 6" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round"/>
''')

SVG_SETTINGS = _svg_wrap(f'''
  <circle cx="12" cy="12" r="3" fill="{ACCENT_COLOR}"/>
  <path d="M12 1v4M12 19v4M4.2 4.2l2.8 2.8M17 17l2.8 2.8M1 12h4M19 12h4M4.2 19.8l2.8-2.8M17 7l2.8-2.8"
        stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round"/>
''')

SVG_INFO = _svg_wrap(f'''
  <circle cx="12" cy="12" r="10" fill="{ACCENT_COLOR}"/>
  <path d="M12 16v-4M12 8h.01" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round"/>
''')

SVG_WARNING = _svg_wrap(f'''
  <path d="M12 2L2 20h20L12 2z" fill="#CCAA4D"/>
  <path d="M12 8v5M12 16h.01" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round"/>
''')

SVG_ERROR = _svg_wrap(f'''
  <circle cx="12" cy="12" r="10" fill="#CC4D4D"/>
  <path d="M8 8l8 8M16 8l-8 8" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round"/>
''')

SVG_CHECK = _svg_wrap(f'''
  <circle cx="12" cy="12" r="10" fill="#4DCC4D"/>
  <path d="M8 12l3 3 5-6" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
''')

SVG_PLUS = _svg_wrap(f'''
  <path d="M12 5v14M5 12h14" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round"/>
''')

SVG_MINUS = _svg_wrap(f'''
  <path d="M5 12h14" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round"/>
''')

SVG_CHEVRON_UP = _svg_wrap(f'''
  <path d="M6 15l6-6 6 6" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none"/>
''')

SVG_CHEVRON_DOWN = _svg_wrap(f'''
  <path d="M6 9l6 6 6-6" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none"/>
''')

SVG_CHEVRON_LEFT = _svg_wrap(f'''
  <path d="M15 6l-6 6 6 6" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none"/>
''')

SVG_CHEVRON_RIGHT = _svg_wrap(f'''
  <path d="M9 6l6 6-6 6" stroke="{ICON_COLOR}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none"/>
''')

################################################################################
# Manipulator Icons
################################################################################

SVG_TRANSLATE = _svg_wrap(f'''
  <path d="M12 2l3 3h-2v5h5v-2l3 3-3 3v-2h-5v5h2l-3 3-3-3h2v-5H5v2l-3-3 3-3v2h5V5H8l4-3z"
        fill="{ICON_COLOR}" fill-opacity="0.9"/>
''')

SVG_ROTATE = _svg_wrap(f'''
  <path d="M12 4V1L8 5l4 4V6c3.31 0 6 2.69 6 6 0 1.01-.25 1.97-.7 2.8l1.46 1.46C19.54 15.03 20 13.57 20 12c0-4.42-3.58-8-8-8zm0 14c-3.31 0-6-2.69-6-6 0-1.01.25-1.97.7-2.8L5.24 7.74C4.46 8.97 4 10.43 4 12c0 4.42 3.58 8 8 8v3l4-4-4-4v3z"
        fill="{ICON_COLOR}" fill-opacity="0.9"/>
''')

SVG_SCALE = _svg_wrap(f'''
  <path d="M21 15h-2v2h2v-2zm0-4h-2v2h2v-2zm-8 8h2v-2h-2v2zm4-12h-2v2h2V7zm4 0h-2v2h2V7zm-4-4v2h2V3h-2zm4 4h-2v2h2V7zM3 21h8v-8H3v8zm2-6h4v4H5v-4zm16 4h-2v2h2v-2zM7 3v2h2V3H7zM3 7v2h2V7H3zm4-4v2h2V3H7zM3 11v2h2v-2H3zM3 3v2h2V3H3z"
        fill="{ICON_COLOR}" fill-opacity="0.9"/>
''')

################################################################################
# Icon Factory Functions
################################################################################

def get(name, width=24, height=24, icon_color=None):
  """
  Get a standard icon as an Image.

  Args:
    name: Icon name (e.g., 'play', 'folder', 'file')
    width: Output width in pixels
    height: Output height in pixels
    icon_color: Optional hex color string to replace default ICON_COLOR

  Returns:
    lev2.Image
  """
  svg = _ICONS.get(name)
  if svg is None:
    raise ValueError(f"Unknown icon: {name}")
  if icon_color:
    svg = svg.replace(ICON_COLOR, icon_color).replace(ICON_COLOR_DIM, icon_color)
  return icon_library.from_svg_string(svg, width, height)

def get_provider(name, width=24, height=24):
  """
  Get a standard icon as an ImageProvider (lazy loading).

  Args:
    name: Icon name (e.g., 'play', 'folder', 'file')
    width: Output width in pixels
    height: Output height in pixels

  Returns:
    lev2.ImageProvider
  """
  svg = _ICONS.get(name)
  if svg is None:
    raise ValueError(f"Unknown icon: {name}")
  return icon_library.provider_from_svg_string(svg, width, height)

def list_icons():
  """Return list of available icon names."""
  return list(_ICONS.keys())

################################################################################
# Icon Registry
################################################################################

_ICONS = {
  # Filesystem
  'file': SVG_FILE,
  'file_image': SVG_FILE_IMAGE,
  'file_code': SVG_FILE_CODE,
  'file_text': SVG_FILE_TEXT,
  'folder': SVG_FOLDER,
  'folder_open': SVG_FOLDER_OPEN,
  'drive': SVG_DRIVE,
  'home': SVG_HOME,
  'parent': SVG_PARENT,

  # Transport
  'play': SVG_PLAY,
  'pause': SVG_PAUSE,
  'stop': SVG_STOP,
  'rewind': SVG_REWIND,
  'fast_forward': SVG_FAST_FORWARD,
  'skip_back': SVG_SKIP_BACK,
  'skip_forward': SVG_SKIP_FORWARD,
  'record': SVG_RECORD,
  'bake_lighting': SVG_BAKE_LIGHTING,
  'envmap_studio': SVG_ENVMAP_STUDIO,
  'loop': SVG_LOOP,

  # Actions
  'new': SVG_NEW,
  'open': SVG_OPEN,
  'save': SVG_SAVE,
  'close': SVG_CLOSE,
  'delete': SVG_DELETE,
  'refresh': SVG_REFRESH,
  'search': SVG_SEARCH,
  'settings': SVG_SETTINGS,

  # Status
  'info': SVG_INFO,
  'warning': SVG_WARNING,
  'error': SVG_ERROR,
  'check': SVG_CHECK,

  # Manipulator
  'translate': SVG_TRANSLATE,
  'rotate': SVG_ROTATE,
  'scale': SVG_SCALE,

  # UI
  'plus': SVG_PLUS,
  'minus': SVG_MINUS,
  'chevron_up': SVG_CHEVRON_UP,
  'chevron_down': SVG_CHEVRON_DOWN,
  'chevron_left': SVG_CHEVRON_LEFT,
  'chevron_right': SVG_CHEVRON_RIGHT,
}

################################################################################
