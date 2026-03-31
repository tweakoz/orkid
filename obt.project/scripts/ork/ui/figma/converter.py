"""
ork.ui.figma.converter — Deterministic Figma JSON node → SVG converter.

Converts Figma node trees (as parsed from the Figma REST API JSON) into
SVG markup.  Handles the node types used in card-based UI designs:

  FRAME, RECTANGLE, VECTOR (ellipse), TEXT, BOOLEAN_OPERATION, INSTANCE, GROUP

When the JSON is fetched with geometry=paths, fillGeometry SVG path data
is used for pixel-perfect rendering.  Otherwise, bounding-box fallbacks
are used.
"""


def _figma_color_to_hex(color_dict):
    """Convert Figma color dict {r, g, b, a} to '#RRGGBB'."""
    r = int(color_dict.get("r", 0) * 255)
    g = int(color_dict.get("g", 0) * 255)
    b = int(color_dict.get("b", 0) * 255)
    return f"#{r:02x}{g:02x}{b:02x}"


def _get_solid_fill(node):
    """Return the first visible SOLID fill hex color, or None."""
    for fill in node.get("fills", []):
        if fill.get("type") == "SOLID" and fill.get("visible", True):
            return _figma_color_to_hex(fill["color"])
    return None


def _get_solid_stroke(node):
    """Return the first visible SOLID stroke hex color, or None."""
    for stroke in node.get("strokes", []):
        if stroke.get("type") == "SOLID" and stroke.get("visible", True):
            return _figma_color_to_hex(stroke["color"])
    return None


_stroke_clip_counter = [0]

def _render_stroke(node, ox, oy, stroke_color):
    """Render a node's stroke as an SVG stroke on its fillGeometry path.

    For strokeAlign=INSIDE: uses double stroke-width clipped to the shape.
    For strokeAlign=CENTER (default): uses stroke-width as-is.
    For strokeAlign=OUTSIDE: uses double stroke-width with inverse clip.
    """
    fill_geom = node.get("fillGeometry", [])
    if not fill_geom:
        return None

    stroke_weight = node.get("strokeWeight", 1)
    stroke_align = node.get("strokeAlign", "CENTER")
    bbox = node["absoluteBoundingBox"]
    x = bbox["x"] - ox
    y = bbox["y"] - oy

    parts = []
    for geom in fill_geom:
        path_data = geom.get("path", "")
        if not path_data:
            continue

        if stroke_align == "INSIDE":
            clip_id = f"sc{_stroke_clip_counter[0]}"
            _stroke_clip_counter[0] += 1
            parts.append(
                f'<defs><clipPath id="{clip_id}">'
                f'<path d="{path_data}" transform="translate({x},{y})"/>'
                f'</clipPath></defs>'
                f'<g clip-path="url(#{clip_id})">'
                f'<path d="{path_data}" fill="none" stroke="{stroke_color}" '
                f'stroke-width="{stroke_weight * 2}" '
                f'transform="translate({x},{y})"/>'
                f'</g>'
            )
        else:
            parts.append(
                f'<path d="{path_data}" fill="none" stroke="{stroke_color}" '
                f'stroke-width="{stroke_weight}" '
                f'transform="translate({x},{y})"/>'
            )

    return "\n".join(parts) if parts else None


def _svg_drop_shadow(effect, filter_id="shadow"):
    """Convert a Figma DROP_SHADOW effect to an SVG filter definition."""
    offset = effect.get("offset", {})
    dx = offset.get("x", 0)
    dy = offset.get("y", 0)
    radius = effect.get("radius", 0)
    color = effect.get("color", {})
    opacity = color.get("a", 0.25)
    return (
        f'<filter id="{filter_id}" x="-20%" y="-20%" width="140%" height="150%">'
        f'<feDropShadow dx="{dx}" dy="{dy}" stdDeviation="{radius / 2}" '
        f'flood-color="{_figma_color_to_hex(color)}" flood-opacity="{opacity}"/>'
        f'</filter>'
    )


# -------------------------------------------------------------------------
# Shape rendering — uses fillGeometry paths when available
# -------------------------------------------------------------------------

def _render_fill_geometry(node, ox, oy, fill):
    """Render a node using its fillGeometry SVG path data.

    Returns SVG string if fillGeometry is present, else None.
    """
    fill_geom = node.get("fillGeometry", [])
    if not fill_geom:
        return None

    bbox = node["absoluteBoundingBox"]
    x = bbox["x"] - ox
    y = bbox["y"] - oy

    paths = []
    for geom in fill_geom:
        path_data = geom.get("path", "")
        if path_data:
            wind = geom.get("windingRule", "NONZERO")
            fill_rule = "evenodd" if wind == "EVENODD" else "nonzero"
            paths.append(
                f'<path d="{path_data}" fill="{fill}" '
                f'fill-rule="{fill_rule}" '
                f'transform="translate({x},{y})"/>'
            )
    return "\n".join(paths) if paths else None


def _convert_shape(node, ox, oy, fill_override=None):
    """Convert a shape node (RECTANGLE, VECTOR, ELLIPSE) to SVG.

    Prefers fillGeometry paths when available for exact rendering.
    Falls back to bounding-box approximations otherwise.
    """
    fill = fill_override or _get_solid_fill(node)
    if fill is None:
        return ""

    # Prefer fillGeometry for all shape types
    svg = _render_fill_geometry(node, ox, oy, fill)
    if svg:
        return svg

    # Fallback: bounding-box based rendering
    node_type = node.get("type", "")
    bbox = node["absoluteBoundingBox"]
    x = bbox["x"] - ox
    y = bbox["y"] - oy
    w = bbox["width"]
    h = bbox["height"]

    if node_type == "RECTANGLE":
        r = node.get("cornerRadius", 0)
        rot = node.get("rotation", 0)
        attrs = f'x="{x}" y="{y}" width="{w}" height="{h}"'
        if r:
            attrs += f' rx="{r}" ry="{r}"'
        attrs += f' fill="{fill}"'
        if abs(rot) > 0.001:
            cx = x + w / 2
            cy = y + h / 2
            return f'<rect {attrs} transform="rotate({-rot} {cx} {cy})"/>'
        return f'<rect {attrs}/>'

    else:  # VECTOR, ELLIPSE
        cx = x + w / 2
        cy = y + h / 2
        rx = w / 2
        ry = h / 2
        return f'<ellipse cx="{cx}" cy="{cy}" rx="{rx}" ry="{ry}" fill="{fill}"/>'


# -------------------------------------------------------------------------
# Text rendering
# -------------------------------------------------------------------------

def _convert_text(node, ox, oy):
    """Convert a Figma TEXT node to SVG text element."""
    bbox = node["absoluteBoundingBox"]
    x = bbox["x"] - ox
    y = bbox["y"] - oy
    w = bbox["width"]
    h = bbox["height"]
    fill = _get_solid_fill(node) or "#FFFFFF"
    text = node.get("characters", "")
    style = node.get("style", {})
    font_family = style.get("fontFamily", "sans-serif")
    font_size = style.get("fontSize", 16)
    font_weight = style.get("fontWeight", 400)
    h_align = style.get("textAlignHorizontal", "LEFT")
    v_align = style.get("textAlignVertical", "TOP")

    if h_align == "CENTER":
        tx = x + w / 2
        anchor = "middle"
    elif h_align == "RIGHT":
        tx = x + w
        anchor = "end"
    else:
        tx = x
        anchor = "start"

    if v_align == "CENTER":
        ty = y + h / 2 + font_size * 0.35
    elif v_align == "BOTTOM":
        ty = y + h
    else:
        ty = y + font_size * 0.85

    weight_attr = f' font-weight="{font_weight}"' if font_weight != 400 else ''

    lines = text.split("\n")
    if len(lines) == 1:
        return (
            f'<text x="{tx}" y="{ty}" text-anchor="{anchor}" '
            f'font-family="{font_family}, monospace" font-size="{font_size}"'
            f'{weight_attr} fill="{fill}">{text}</text>'
        )
    else:
        tspans = []
        for i, line in enumerate(lines):
            line_y = ty + i * font_size * 1.2
            tspans.append(f'<tspan x="{tx}" y="{line_y}">{line}</tspan>')
        return (
            f'<text text-anchor="{anchor}" '
            f'font-family="{font_family}, monospace" font-size="{font_size}"'
            f'{weight_attr} fill="{fill}">{"".join(tspans)}</text>'
        )


# -------------------------------------------------------------------------
# Boolean operations
# -------------------------------------------------------------------------

_bool_id_counter = [0]

def _convert_boolean_operation(node, ox, oy):
    """Convert a Figma BOOLEAN_OPERATION (UNION) to SVG.

    Strategy: render all child shapes into a clipPath, then fill through
    the clip. This produces a true boolean union with no seams.

    When fillGeometry is available on the BOOLEAN_OPERATION node itself,
    we use that directly (it's the pre-computed union outline).
    """
    fill = _get_solid_fill(node)
    if fill is None:
        return ""

    # If the boolean op node itself has fillGeometry, use it directly
    # (this is the computed union as SVG paths)
    fg = node.get("fillGeometry", [])
    if fg:
        bbox = node["absoluteBoundingBox"]
        x = bbox["x"] - ox
        y = bbox["y"] - oy
        paths = []
        for geom in fg:
            path_data = geom.get("path", "")
            if path_data:
                wind = geom.get("windingRule", "NONZERO")
                fill_rule = "evenodd" if wind == "EVENODD" else "nonzero"
                paths.append(
                    f'<path d="{path_data}" fill="{fill}" '
                    f'fill-rule="{fill_rule}" '
                    f'transform="translate({x},{y})"/>'
                )
        if paths:
            return "\n".join(paths)

    # Fallback: clipPath approach from child shapes
    clip_parts = []
    for child in node.get("children", []):
        child_type = child.get("type", "")
        if child_type in ("GROUP", "INSTANCE"):
            for grandchild in child.get("children", []):
                clip_parts.append(_convert_shape(grandchild, ox, oy, fill_override="white"))
        else:
            clip_parts.append(_convert_shape(child, ox, oy, fill_override="white"))

    clip_svg = "\n".join(p for p in clip_parts if p)
    if not clip_svg:
        return ""

    clip_id = f"bool{_bool_id_counter[0]}"
    _bool_id_counter[0] += 1

    bbox = node["absoluteBoundingBox"]
    x = bbox["x"] - ox
    y = bbox["y"] - oy
    w = bbox["width"]
    h = bbox["height"]
    pad = 2
    rx, ry, rw, rh = x - pad, y - pad, w + pad * 2, h + pad * 2

    return (
        f'<defs><clipPath id="{clip_id}">{clip_svg}</clipPath></defs>'
        f'<rect x="{rx}" y="{ry}" width="{rw}" height="{rh}" '
        f'fill="{fill}" clip-path="url(#{clip_id})"/>'
    )


# -------------------------------------------------------------------------
# Public API
# -------------------------------------------------------------------------

def figma_node_to_svg(node, width=None, height=None):
    """Convert a Figma node tree to a complete SVG document string."""
    bbox = node["absoluteBoundingBox"]
    ox = bbox["x"]
    oy = bbox["y"]
    w = width or bbox["width"]
    h = height or bbox["height"]

    # Collect filter definitions from effects
    defs_parts = []
    filter_id_counter = [0]

    def _collect_effects(n):
        for effect in n.get("effects", []):
            if effect.get("type") == "DROP_SHADOW" and effect.get("visible", True):
                fid = f"shadow{filter_id_counter[0]}"
                filter_id_counter[0] += 1
                defs_parts.append(_svg_drop_shadow(effect, fid))
                n["_svg_filter_id"] = fid
        for child in n.get("children", []):
            _collect_effects(child)

    _collect_effects(node)

    defs_svg = ""
    if defs_parts:
        defs_svg = "<defs>" + "".join(defs_parts) + "</defs>"

    body_parts = []
    _render_children(node, ox, oy, body_parts)
    body_svg = "\n".join(body_parts)

    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}">'
        f'{defs_svg}'
        f'{body_svg}'
        f'</svg>'
    )


def figma_children_to_svg_fragment(node, ox, oy):
    """Convert a node's children to SVG fragment (no <svg> wrapper)."""
    parts = []
    _render_children(node, ox, oy, parts)
    return "\n".join(parts)


def _render_children(node, ox, oy, parts):
    """Recursively render a node's children into SVG parts list."""
    for child in node.get("children", []):
        _render_node(child, ox, oy, parts)


def _render_node(node, ox, oy, parts):
    """Render a single Figma node to SVG and append to parts."""
    node_type = node.get("type", "")
    visible = node.get("visible", True)
    if not visible:
        return

    filter_attr = ""
    if "_svg_filter_id" in node:
        filter_attr = f' filter="url(#{node["_svg_filter_id"]})"'

    if node_type in ("RECTANGLE", "VECTOR", "ELLIPSE"):
        shape_parts = []

        fill = _get_solid_fill(node)
        stroke = _get_solid_stroke(node)

        if fill is not None:
            # Has a fill color — render the fill
            fg_svg = _render_fill_geometry(node, ox, oy, fill)
            if fg_svg:
                shape_parts.append(fg_svg)
            else:
                svg = _convert_shape(node, ox, oy)
                if svg:
                    shape_parts.append(svg)

        if stroke is not None:
            # Render strokeGeometry (pre-computed stroke outline) as filled path
            sg_svg = _render_stroke(node, ox, oy, stroke)
            if sg_svg:
                shape_parts.append(sg_svg)

        if shape_parts:
            combined = "\n".join(shape_parts)
            if filter_attr:
                parts.append(f'<g{filter_attr}>{combined}</g>')
            else:
                parts.append(combined)

    elif node_type == "TEXT":
        parts.append(_convert_text(node, ox, oy))

    elif node_type == "BOOLEAN_OPERATION":
        parts.append(_convert_boolean_operation(node, ox, oy))

    elif node_type in ("FRAME", "GROUP", "INSTANCE", "SECTION"):
        fill = _get_solid_fill(node)
        if fill is not None:
            bbox = node["absoluteBoundingBox"]
            x = bbox["x"] - ox
            y = bbox["y"] - oy
            w = bbox["width"]
            h = bbox["height"]
            r = node.get("cornerRadius", 0)
            attrs = f'x="{x}" y="{y}" width="{w}" height="{h}"'
            if r:
                attrs += f' rx="{r}" ry="{r}"'
            attrs += f' fill="{fill}"{filter_attr}'
            parts.append(f'<rect {attrs}/>')
        _render_children(node, ox, oy, parts)
