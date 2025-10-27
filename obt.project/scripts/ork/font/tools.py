################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Font glyph extraction utilities
"""

from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.basePen import decomposeQuadraticSegment

def extract_glyph_beziers(font_path, char, scale=1.0, center_x=0.0, center_y=0.0):
    """
    Extract quadratic bezier curves from a font glyph.

    Args:
        font_path: Path to TrueType or OpenType font file
        char: Character to extract (e.g. 'Ω', 'Φ')
        scale: Scale factor for the glyph
        center_x: X coordinate to center the glyph around
        center_y: Y coordinate to center the glyph around

    Returns:
        List of bezier curve points. Each bezier is represented as tuples of (x, y) coordinates.
        For quadratic beziers: [(x0, y0), (x1, y1), (x2, y2)]
        The points are scaled and centered according to the parameters.
    """
    font = TTFont(font_path)
    glyph_set = font.getGlyphSet()

    # Get glyph name for character
    cmap = font.getBestCmap()
    if ord(char) not in cmap:
        raise ValueError(f"Character '{char}' not found in font")

    glyph_name = cmap[ord(char)]
    glyph = glyph_set[glyph_name]

    # Extract outline
    pen = RecordingPen()
    glyph.draw(pen)

    # Get glyph metrics for centering
    units_per_em = font['head'].unitsPerEm

    # Calculate actual bounding box from the pen recording
    min_x = float('inf')
    max_x = float('-inf')
    min_y = float('inf')
    max_y = float('-inf')

    for command, args in pen.value:
        if command == 'moveTo' or command == 'lineTo':
            for point in args:
                min_x = min(min_x, point[0])
                max_x = max(max_x, point[0])
                min_y = min(min_y, point[1])
                max_y = max(max_y, point[1])
        elif command in ('qCurveTo', 'curveTo'):
            for point in args:
                min_x = min(min_x, point[0])
                max_x = max(max_x, point[0])
                min_y = min(min_y, point[1])
                max_y = max(max_y, point[1])

    if min_x == float('inf'):
        # Fallback if no points found
        glyph_cx = units_per_em / 2.0
        glyph_cy = units_per_em / 2.0
    else:
        glyph_cx = (min_x + max_x) / 2.0
        glyph_cy = (min_y + max_y) / 2.0

    # Convert pen recording to bezier curves
    beziers = []
    contours = []  # Group beziers by contour
    current_contour = []
    current_point = None
    start_point = None

    for command, args in pen.value:
        if command == 'moveTo':
            # Start of new contour
            if current_contour:
                contours.append(current_contour)
                current_contour = []
            current_point = args[0]
            start_point = args[0]
        elif command == 'lineTo':
            # Convert line to degenerate quadratic bezier (control point = midpoint)
            if current_point:
                p0 = current_point
                p2 = args[0]
                p1 = ((p0[0] + p2[0]) / 2.0, (p0[1] + p2[1]) / 2.0)

                # Transform and scale
                bezier = [
                    transform_point(p0, glyph_cx, glyph_cy, scale, center_x, center_y),
                    transform_point(p1, glyph_cx, glyph_cy, scale, center_x, center_y),
                    transform_point(p2, glyph_cx, glyph_cy, scale, center_x, center_y)
                ]
                current_contour.append(bezier)
                current_point = args[0]
        elif command == 'qCurveTo':
            # Quadratic bezier - this is what TrueType fonts use
            # Use fontTools' decomposition function to properly handle chained curves
            if current_point:
                points = list(args)

                # decomposeQuadraticSegment returns list of (pt1, pt2) tuples
                # where pt1 is control point and pt2 is end point
                # It automatically handles implied on-curve points
                segments = decomposeQuadraticSegment([current_point] + points)

                for pt1, pt2 in segments:
                    bezier = [
                        transform_point(current_point, glyph_cx, glyph_cy, scale, center_x, center_y),
                        transform_point(pt1, glyph_cx, glyph_cy, scale, center_x, center_y),
                        transform_point(pt2, glyph_cx, glyph_cy, scale, center_x, center_y)
                    ]
                    current_contour.append(bezier)
                    current_point = pt2

                current_point = points[-1]
        elif command == 'curveTo':
            # Cubic bezier - need to convert to quadratic approximation
            if current_point:
                p0 = current_point
                # curveTo has (cp1, cp2, end_point)
                cp1, cp2, end = args

                # Simple approximation: use midpoint of cubic control points as quadratic control
                p1 = ((cp1[0] + cp2[0]) / 2.0, (cp1[1] + cp2[1]) / 2.0)
                p2 = end

                bezier = [
                    transform_point(p0, glyph_cx, glyph_cy, scale, center_x, center_y),
                    transform_point(p1, glyph_cx, glyph_cy, scale, center_x, center_y),
                    transform_point(p2, glyph_cx, glyph_cy, scale, center_x, center_y)
                ]
                current_contour.append(bezier)
                current_point = end
        elif command == 'closePath':
            # Close the path by connecting back to start if needed
            if current_point and start_point and current_point != start_point:
                # Add closing line segment
                p0 = current_point
                p2 = start_point
                p1 = ((p0[0] + p2[0]) / 2.0, (p0[1] + p2[1]) / 2.0)

                bezier = [
                    transform_point(p0, glyph_cx, glyph_cy, scale, center_x, center_y),
                    transform_point(p1, glyph_cx, glyph_cy, scale, center_x, center_y),
                    transform_point(p2, glyph_cx, glyph_cy, scale, center_x, center_y)
                ]
                current_contour.append(bezier)

            if current_contour:
                contours.append(current_contour)
                current_contour = []
            current_point = None
            start_point = None

    # Add any remaining contour
    if current_contour:
        contours.append(current_contour)

    # Flatten all contours into single list
    for contour in contours:
        beziers.extend(contour)

    return beziers


def transform_point(point, glyph_cx, glyph_cy, scale, center_x, center_y):
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
