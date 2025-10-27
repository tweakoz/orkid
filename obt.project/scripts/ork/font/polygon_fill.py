################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Scanline polygon filling with winding rule for proper font glyph rendering
"""

def fill_polygon_contours(image, contours, color):
    """
    Fill polygon defined by multiple contours using scanline rasterization with winding rule.

    Args:
        image: Image object with pixel32f(x, y) access
        contours: List of contours, where each contour is a list of (x, y) points
        color: (r, g, b, a) color tuple
    """
    if not contours or len(contours) == 0:
        return

    # Get image dimensions
    width = image.width
    height = image.height

    # Build edge list from all contours
    edges = []
    for contour in contours:
        n = len(contour)
        if n < 3:
            continue

        for i in range(n):
            p1 = contour[i]
            p2 = contour[(i + 1) % n]

            x1, y1 = p1
            x2, y2 = p2

            # Skip horizontal edges
            if abs(y2 - y1) < 0.001:
                continue

            # Ensure y1 < y2
            if y1 > y2:
                x1, y1, x2, y2 = x2, y2, x1, y1

            # Store edge: (y_min, y_max, x_at_ymin, dx/dy, direction)
            # Direction: +1 if edge goes down (y increasing), -1 if up
            direction = 1 if p2[1] > p1[1] else -1
            dx_dy = (x2 - x1) / (y2 - y1) if abs(y2 - y1) > 0.001 else 0

            edges.append({
                'y_min': y1,
                'y_max': y2,
                'x': x1,
                'dx_dy': dx_dy,
                'direction': direction
            })

    # Find y range
    if not edges:
        return

    y_min = int(min(e['y_min'] for e in edges))
    y_max = int(max(e['y_max'] for e in edges)) + 1

    # Clip to image bounds
    y_min = max(0, y_min)
    y_max = min(height, y_max)

    # Scanline fill
    for y in range(y_min, y_max):
        # Find all active edges for this scanline
        active_edges = []

        for edge in edges:
            if edge['y_min'] <= y < edge['y_max']:
                # Calculate x intersection
                x = edge['x'] + (y - edge['y_min']) * edge['dx_dy']
                active_edges.append({
                    'x': x,
                    'direction': edge['direction']
                })

        # Sort by x coordinate
        active_edges.sort(key=lambda e: e['x'])

        # Apply winding rule to fill spans
        winding = 0
        i = 0

        while i < len(active_edges):
            # Accumulate winding number until we find a span
            start_x = None

            while i < len(active_edges):
                edge = active_edges[i]

                if winding == 0:
                    start_x = edge['x']

                winding += edge['direction']
                i += 1

                if winding == 0 and start_x is not None:
                    # Found a filled span from start_x to current edge x
                    end_x = edge['x']

                    # Fill pixels in this span
                    x1_pixel = max(0, int(start_x))
                    x2_pixel = min(width - 1, int(end_x))

                    for x in range(x1_pixel, x2_pixel + 1):
                        pixel = image.pixel32f(x, y)
                        pixel[0] = color[0]
                        pixel[1] = color[1]
                        pixel[2] = color[2]
                        pixel[3] = color[3]

                    break  # Move to next span
