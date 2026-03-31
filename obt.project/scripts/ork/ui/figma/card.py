"""
Generic rounded-rect card SVG builder.

Generates a card with configurable color, corner radius, drop shadow,
and an optional centered icon.  Apps supply all visual parameters —
no design-specific constants live here.
"""

from ork.ui.figma.shapes import (
    person_silhouette_svg,
    multi_person_svg,
    circle_icon_svg,
)

# Registry of built-in icon type renderers.
# Each takes (color, cx, cy, scale, icon_scale) and returns SVG fragment.
# icon_scale is the caller-supplied multiplier for the icon within the card.
_ICON_RENDERERS = {
    "person":       lambda c, cx, cy, s, iscale: person_silhouette_svg(c, cx=cx, cy=cy, scale=s * iscale),
    "multi_person": lambda c, cx, cy, s, iscale: multi_person_svg(c, cx=cx, cy=cy, scale=s * iscale, count=3),
    "circle":       lambda c, cx, cy, s, iscale: circle_icon_svg(c, cx=cx, cy=cy, r=iscale * s),
}


def build_card_svg(card_color, icon_color, icon_type="person", size=300,
                   corner_radius=40, shadow_dy=4, shadow_blur=8, shadow_opacity=0.25,
                   label=None, label_color="#FFFFFF", label_font_size=20,
                   label_bottom_offset=51,
                   icon_cy_offset=0, icon_scale=1.0):
    """Build an SVG string for a rounded-rect card with optional icon and label.

    Args:
        card_color: Fill color for the card background.
        icon_color: Fill color for the centered icon.
        icon_type: Key in _ICON_RENDERERS, or "none" for no icon.
                   Can also pass a callable(color, cx, cy, scale)->str.
        size: Card dimension in pixels (square).
        corner_radius: Corner radius (at size=300 reference; scales proportionally).
        shadow_dy: Drop shadow Y offset.
        shadow_blur: Drop shadow blur radius (stdDeviation).
        shadow_opacity: Drop shadow opacity.
        label: Text to render inside the card near the bottom. None = no label.
        label_color: Fill color for the label text.
        label_font_size: Font size (at size=300 reference; scales proportionally).
        label_bottom_offset: Distance from card bottom to label baseline
                             (at size=300 reference; scales proportionally).
        icon_cy_offset: Vertical offset for the icon center (at size=300 reference).
                        Negative = up. Useful when label occupies the bottom.
        icon_scale: Scale multiplier for the icon shape within the card.
                    Meaning depends on icon_type (e.g. person scale, circle radius).

    Returns:
        Complete SVG markup string.
    """
    s = size / 300  # master scale factor (300 = reference size)
    r = corner_radius * s
    cx = size / 2
    cy = size / 2 + icon_cy_offset * s

    if callable(icon_type):
        icon_svg = icon_type(icon_color, cx, cy, s)
    elif icon_type in _ICON_RENDERERS:
        icon_svg = _ICON_RENDERERS[icon_type](icon_color, cx, cy, s, icon_scale)
    else:
        icon_svg = ""

    label_svg = ""
    if label:
        fs = label_font_size * s
        ly = size - label_bottom_offset * s + fs * 0.35  # baseline adjust
        label_svg = (
            f'<text x="{cx}" y="{ly}" text-anchor="middle" '
            f'font-family="Roboto Mono, monospace" font-size="{fs}" '
            f'fill="{label_color}">{label}</text>'
        )

    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}">'
        f'<defs>'
        f'<filter id="shadow" x="-10%" y="-10%" width="120%" height="130%">'
        f'<feDropShadow dx="0" dy="{shadow_dy}" stdDeviation="{shadow_blur}" '
        f'flood-color="#000000" flood-opacity="{shadow_opacity}"/>'
        f'</filter>'
        f'</defs>'
        f'<rect x="0" y="0" width="{size}" height="{size}" rx="{r}" ry="{r}" '
        f'fill="{card_color}" filter="url(#shadow)"/>'
        f'{icon_svg}'
        f'{label_svg}'
        f'</svg>'
    )
