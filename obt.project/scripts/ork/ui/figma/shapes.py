"""
Parameterized SVG shape primitives for Figma-style UI.

All functions return SVG markup fragments (not complete documents).
Colors, positions, and scale are caller-supplied.
"""


def person_silhouette_svg(color, cx=0, cy=0, scale=1.0):
    """SVG group for a single person silhouette (57x71 base units).

    Head (circle), shoulder bar, and two overlapping body rectangles.

    Args:
        color: Fill color (e.g. "#166714")
        cx, cy: Center position in parent coordinate space
        scale: Scale factor (1.0 = 57x71 units)
    """
    ox = cx - 28.5 * scale
    oy = cy - 35.5 * scale
    s = scale
    return (
        f'<g transform="translate({ox},{oy}) scale({s})">'
        f'<ellipse cx="28.5" cy="14.5" rx="14.5" ry="14.5" fill="{color}"/>'
        f'<rect x="0" y="27" width="57" height="12" rx="2" fill="{color}"/>'
        f'<rect x="7" y="22" width="28" height="50" rx="2" fill="{color}"/>'
        f'<rect x="22" y="22" width="28" height="50" rx="2" fill="{color}"/>'
        f'</g>'
    )


def multi_person_svg(color, cx=0, cy=0, scale=1.0, count=3, positions=None):
    """SVG for multiple person silhouettes at specified positions.

    Args:
        color: Fill color for all silhouettes.
        cx, cy: Center of the group in parent coordinate space.
        scale: Scale factor applied to positions and silhouettes.
        count: Number of persons (capped to len(positions)).
        positions: List of (dx, dy) offsets from (cx, cy) at scale=1.
                   If None, uses a default triangle layout.
    """
    if positions is None:
        positions = [
            (-44.5, 36.5),
            (45.5,  36.5),
            (0.5,  -35.0),
        ]
    parts = []
    for dx, dy in positions[:count]:
        parts.append(person_silhouette_svg(
            color,
            cx=cx + dx * scale,
            cy=cy + dy * scale,
            scale=scale,
        ))
    return "\n".join(parts)


def circle_icon_svg(color, cx=0, cy=0, r=14.5):
    """Simple filled circle."""
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{color}"/>'
