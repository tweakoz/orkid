"""
ork.ui.figma.constants — Configurable constants for Figma design parsing.

All numeric thresholds and defaults used by OrkidFigmaDesign and
SwiftFigmaDesign are collected here.  The 2D Editor stores
overrides in an "editor" key inside the exported JSON so they persist
across sessions.
"""


# ── Detection thresholds ────────────────────────────────────────────
# Font-size ranges used to classify text nodes by role.

ICON_FONT_SIZE_MIN = 100          # TEXT node with fs >= this is treated as SF Symbol icon
HEADING_FONT_SIZE_MIN = 30        # TEXT node with fs >= this (and < ICON) is a heading
DESCRIPTION_FONT_SIZE_MIN = 6     # lower bound for description text
DESCRIPTION_FONT_SIZE_MAX = 10    # upper bound for description text
TITLE_SYMBOL_FONT_SIZE_MAX = 16   # title bar symbol must have fs <= this
BOLD_FONT_WEIGHT_MIN = 600        # font weight >= this is rendered bold

# Unicode thresholds
SF_SYMBOL_CODEPOINT_MIN = 0xF0000  # codepoints above this are SF Symbol privates

# Dimension thresholds for element detection
CONTENT_RECT_WIDTH_MIN = 100      # RECTANGLE wider than this is a content area
DMG_WINDOW_WIDTH_MIN = 400        # RECTANGLE in DMG frame wider than this is the window
DMG_ICON_WIDTH_MIN = 100          # lower bound for DMG icon detection
DMG_ICON_WIDTH_MAX = 200          # upper bound for DMG icon detection

# Card detection (OrkidFigmaDesign)
CARD_MIN_SIZE = 200               # minimum width/height to be detected as a card
CARD_MIN_RADIUS = 10              # minimum corner radius to be detected as a card


# ── Layout defaults ─────────────────────────────────────────────────
# Fallback values when the Figma JSON doesn't supply them.

DEFAULT_WINDOW_SIZE = (503, 300)  # default (w, h) when no Utility Panel found
DEFAULT_DMG_ICON_SIZE = 130       # fallback icon size in DMG layout

# PanelSpec defaults (OrkidFigmaDesign selected-state panel)
PANEL_DEFAULT_GAP = 32
PANEL_DEFAULT_WIDTH = 300
PANEL_DEFAULT_HEIGHT = 263
PANEL_DEFAULT_CORNER_RADIUS = 40
PANEL_DEFAULT_STROKE_WIDTH = 2
PANEL_DEFAULT_TEXT_SIZE = 16
PANEL_DEFAULT_TEXT_OFFSET_X = 33
PANEL_DEFAULT_TEXT_OFFSET_Y = 37

# Fallback heading when not found in Figma tree
DEFAULT_HEADING_FONT_SIZE = 40
DEFAULT_HEADING_FONT_WEIGHT = 400

# Default colors
DEFAULT_DESCRIPTION_COLOR = "#2f2f2f"
DEFAULT_SHEET_CORNER_RADIUS = 26


# ── Rendering constants ─────────────────────────────────────────────
# Used by the SVG converter and AppKit builder.  These are rendering
# math, not design-level parameters.

HEADING_HEIGHT_MULTIPLIER = 1.3   # line-height for heading text
LINE_HEIGHT_MULTIPLIER = 1.2     # line-height for multi-line text
TEXT_ASCENDER_RATIO = 0.76        # ascender as fraction of fontSize (alphabetic baseline offset)
TEXT_CHAR_WIDTH_RATIO = 0.42      # average character width as fraction of fontSize (for word wrapping)
DESCRIPTION_TEXT_OPACITY = 0.85   # default opacity for small description text
LABEL_FONT_SIZE = 13              # default font size for UI labels


# ── AppKit-specific ─────────────────────────────────────────────────
# Used only by SwiftFigmaDesign.build_page() for macOS NSView rendering.

APPKIT_BOX_TYPE = 4               # NSBoxCustom
APPKIT_BORDER_TYPE = 0            # NSNoBorder
APPKIT_DETAIL_MARGIN = 20         # margin for detail label
APPKIT_DETAIL_HEIGHT = 20
APPKIT_CANCEL_MARGIN_RIGHT = 20
APPKIT_CANCEL_MARGIN_BOTTOM = 10
APPKIT_DESCRIPTION_MIN_WIDTH = 200
APPKIT_PROGRESS_MIN = 0
APPKIT_PROGRESS_MAX = 100


def to_dict():
    """Export all constants as a dict (for JSON serialization)."""
    return {k: v for k, v in globals().items()
            if k.isupper() and not k.startswith("_")}


def from_dict(d):
    """Apply overrides from a dict (loaded from JSON "editor" field)."""
    g = globals()
    for k, v in d.items():
        if k in g and k.isupper():
            g[k] = type(g[k])(v) if not isinstance(v, (list, tuple)) else tuple(v)
