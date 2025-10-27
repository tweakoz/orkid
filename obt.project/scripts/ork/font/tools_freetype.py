################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Font glyph extraction using FreeType
"""

import freetype

def extract_glyph_beziers_freetype(font_path, char, scale=1.0, center_x=0.0, center_y=0.0, tension=0.1):
    """
    Extract smooth quadratic bezier curves from a font glyph using FreeType.
    Uses Catmull-Rom spline smoothing to convert polygon points to beziers.
    Respects contour boundaries - does not connect across different contours.

    Args:
        font_path: Path to TrueType or OpenType font file
        char: Character to extract (e.g. 'Ω', 'Φ')
        scale: Scale factor for the glyph
        center_x: X coordinate to center the glyph around
        center_y: Y coordinate to center the glyph around
        tension: Catmull-Rom tension parameter (0.0-1.0, lower = smoother)

    Returns:
        List of bezier curve points. Each bezier is [(x0, y0), (x1, y1), (x2, y2)]
    """
    # Load font with FreeType
    face = freetype.Face(font_path)
    face.set_char_size(48 * 64)  # Set size in 1/64th points

    # Load the glyph
    face.load_char(char, freetype.FT_LOAD_NO_BITMAP)

    outline = face.glyph.outline
    points = outline.points
    contours = outline.contours

    # Calculate bounding box for centering
    if len(points) > 0:
        min_x = min(p[0] for p in points)
        max_x = max(p[0] for p in points)
        min_y = min(p[1] for p in points)
        max_y = max(p[1] for p in points)
        glyph_cx = (min_x + max_x) / 2.0
        glyph_cy = (min_y + max_y) / 2.0
    else:
        glyph_cx = 0
        glyph_cy = 0

    # Convert polygon points to smooth beziers using Catmull-Rom, respecting contour boundaries
    beziers = []
    start_idx = 0

    for contour_end in contours:
        # Process each contour separately
        contour_len = contour_end - start_idx + 1

        for i in range(start_idx, contour_end + 1):
            # Get 4 points for Catmull-Rom, wrapping within contour
            idx_offset = i - start_idx
            p0_idx = start_idx + ((idx_offset - 1) % contour_len)
            p1_idx = i
            p2_idx = start_idx + ((idx_offset + 1) % contour_len)
            p3_idx = start_idx + ((idx_offset + 2) % contour_len)

            p0 = points[p0_idx]
            p1 = points[p1_idx]
            p2 = points[p2_idx]
            p3 = points[p3_idx]

            start, control, end = _catmull_rom_to_quad_bezier(p0, p1, p2, p3, tension)

            bezier = [
                _transform_point(start, glyph_cx, glyph_cy, scale, center_x, center_y),
                _transform_point(control, glyph_cx, glyph_cy, scale, center_x, center_y),
                _transform_point(end, glyph_cx, glyph_cy, scale, center_x, center_y)
            ]
            beziers.append(bezier)

        start_idx = contour_end + 1

    return beziers


def _catmull_rom_to_quad_bezier(p0, p1, p2, p3, tension=0.1):
    """
    Convert Catmull-Rom segment to quadratic Bezier approximation.

    Args:
        p0, p1, p2, p3: Four consecutive points in the polygon
        tension: Controls smoothness (0.0-1.0, lower = smoother)

    Returns:
        Tuple of (start, control, end) points for quadratic bezier
    """
    # Tangent at p1 (start of segment)
    t1_x = tension * (p2[0] - p0[0])
    t1_y = tension * (p2[1] - p0[1])

    # Tangent at p2 (end of segment)
    t2_x = tension * (p3[0] - p1[0])
    t2_y = tension * (p3[1] - p1[1])

    # Cubic control points
    c0_x = p1[0] + t1_x / 3
    c0_y = p1[1] + t1_y / 3
    c1_x = p2[0] - t2_x / 3
    c1_y = p2[1] - t2_y / 3

    # Approximate cubic with quadratic by using midpoint of cubic controls
    ctrl_x = (c0_x + c1_x) / 2
    ctrl_y = (c0_y + c1_y) / 2

    return (p1, (ctrl_x, ctrl_y), p2)


def extract_glyph_lines_freetype(font_path, char, scale=1.0, center_x=0.0, center_y=0.0):
    """
    Extract line segments from a font glyph using FreeType.
    Returns the outline as simple connected line segments (polygon).
    Respects contour boundaries - does not connect across different contours.

    Args:
        font_path: Path to TrueType or OpenType font file
        char: Character to extract (e.g. 'Ω', 'Φ')
        scale: Scale factor for the glyph
        center_x: X coordinate to center the glyph around
        center_y: Y coordinate to center the glyph around

    Returns:
        List of line segments. Each line is [(x0, y0), (x1, y1)]
    """
    # Load font with FreeType
    face = freetype.Face(font_path)
    face.set_char_size(48 * 64)  # Set size in 1/64th points

    # Load the glyph
    face.load_char(char, freetype.FT_LOAD_NO_BITMAP)

    outline = face.glyph.outline
    points = outline.points
    contours = outline.contours

    # Calculate bounding box for centering
    if len(points) > 0:
        min_x = min(p[0] for p in points)
        max_x = max(p[0] for p in points)
        min_y = min(p[1] for p in points)
        max_y = max(p[1] for p in points)
        glyph_cx = (min_x + max_x) / 2.0
        glyph_cy = (min_y + max_y) / 2.0
    else:
        glyph_cx = 0
        glyph_cy = 0

    # Convert polygon points to line segments, respecting contour boundaries
    lines = []
    start_idx = 0

    for contour_end in contours:
        # Draw lines within this contour only
        for i in range(start_idx, contour_end + 1):
            # Next point wraps within the contour
            next_i = i + 1
            if next_i > contour_end:
                next_i = start_idx

            p0 = points[i]
            p1 = points[next_i]

            line = [
                _transform_point(p0, glyph_cx, glyph_cy, scale, center_x, center_y),
                _transform_point(p1, glyph_cx, glyph_cy, scale, center_x, center_y)
            ]
            lines.append(line)

        start_idx = contour_end + 1

    return lines


def extract_glyph_contours_freetype(font_path, char, scale=1.0, center_x=0.0, center_y=0.0):
    """
    Extract polygon contours from a font glyph using FreeType.
    Returns contours as lists of points for polygon filling.

    Args:
        font_path: Path to TrueType or OpenType font file
        char: Character to extract (e.g. 'Ω', 'Φ')
        scale: Scale factor for the glyph
        center_x: X coordinate to center the glyph around
        center_y: Y coordinate to center the glyph around

    Returns:
        List of contours, where each contour is a list of (x, y) tuples
    """
    # Load font with FreeType
    face = freetype.Face(font_path)
    face.set_char_size(48 * 64)  # Set size in 1/64th points

    # Load the glyph
    face.load_char(char, freetype.FT_LOAD_NO_BITMAP)

    outline = face.glyph.outline
    points = outline.points
    contours = outline.contours

    # Calculate bounding box for centering
    if len(points) > 0:
        min_x = min(p[0] for p in points)
        max_x = max(p[0] for p in points)
        min_y = min(p[1] for p in points)
        max_y = max(p[1] for p in points)
        glyph_cx = (min_x + max_x) / 2.0
        glyph_cy = (min_y + max_y) / 2.0
    else:
        glyph_cx = 0
        glyph_cy = 0

    # Extract contours as point lists
    result_contours = []
    start_idx = 0

    for contour_end in contours:
        contour_points = []

        for i in range(start_idx, contour_end + 1):
            pt = points[i]
            transformed = _transform_point(pt, glyph_cx, glyph_cy, scale, center_x, center_y)
            contour_points.append(transformed)

        result_contours.append(contour_points)
        start_idx = contour_end + 1

    return result_contours


def _transform_point(point, glyph_cx, glyph_cy, scale, center_x, center_y):
    """Transform a point from glyph space to target space"""
    x, y = point
    # Center around glyph center
    x = x - glyph_cx
    y = y - glyph_cy
    # Flip Y (font coordinates are bottom-up, screen is top-down)
    y = -y
    # Scale
    x *= scale
    y *= scale
    # Translate to target center
    x += center_x
    y += center_y
    return (x, y)
