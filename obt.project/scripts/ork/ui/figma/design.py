"""
ork.ui.figma.design — Generic Figma JSON access.

FigmaDesign loads a Figma REST API JSON export and provides structured
navigation: pages, sections, frames, find/find_all, region_to_svg.

No platform-specific dependencies (no AppKit, no orkid C++ UI).
Subclass for platform-specific intelligence:
  - OrkidFigmaDesign: orkid card UI (detect_cards, hover SVGs, etc.)
  - SwiftFigmaDesign: macOS AppKit (build_page, dmg_layout, etc.)
"""

import json
from pathlib import Path

from ork.ui.figma.converter import (
    figma_node_to_svg,
    _get_solid_fill,
    _get_solid_stroke,
    _figma_color_to_hex,
)


# -------------------------------------------------------------------------
# FigmaNode — generic wrapper
# -------------------------------------------------------------------------

class FigmaNode:
    """Wrapper around a raw Figma JSON node dict."""

    def __init__(self, data, root_design=None):
        self._data = data
        self._design = root_design

    @property
    def name(self):
        return self._data.get("name", "")

    @property
    def type(self):
        return self._data.get("type", "")

    @property
    def bbox(self):
        b = self._data.get("absoluteBoundingBox") or {}
        return b.get("x", 0), b.get("y", 0), b.get("width", 0), b.get("height", 0)

    @property
    def width(self):
        return (self._data.get("absoluteBoundingBox") or {}).get("width", 0)

    @property
    def height(self):
        return (self._data.get("absoluteBoundingBox") or {}).get("height", 0)

    @property
    def fill_color(self):
        return _get_solid_fill(self._data)

    @property
    def stroke_color(self):
        return _get_solid_stroke(self._data)

    @property
    def background_color(self):
        bg = self._data.get("backgroundColor")
        if bg:
            return _figma_color_to_hex(bg)
        return self.fill_color

    @property
    def corner_radius(self):
        return self._data.get("cornerRadius", 0)

    @property
    def rotation(self):
        return self._data.get("rotation", 0)

    @property
    def text(self):
        return self._data.get("characters", "")

    @property
    def font_size(self):
        return self._data.get("style", {}).get("fontSize", 0)

    @property
    def font_family(self):
        return self._data.get("style", {}).get("fontFamily", "")

    @property
    def font_weight(self):
        return self._data.get("style", {}).get("fontWeight", 400)

    @property
    def opacity(self):
        return self._data.get("opacity", 1.0)

    @property
    def children(self):
        return [FigmaNode(c, self._design) for c in self._data.get("children", [])]

    @property
    def raw(self):
        return self._data

    def component_property(self, key):
        """Get a componentProperties value by key (exact or partial match)."""
        cp = self._data.get("componentProperties", {})
        if key in cp:
            return cp[key].get("value")
        for k, v in cp.items():
            if k.startswith(key) or key in k:
                return v.get("value")
        return None

    def find(self, name=None, type=None):
        for child in self._data.get("children", []):
            match = (name is None or child.get("name") == name) and \
                    (type is None or child.get("type") == type)
            if match:
                return FigmaNode(child, self._design)
            result = FigmaNode(child, self._design).find(name=name, type=type)
            if result is not None:
                return result
        return None

    def find_all(self, name=None, type=None):
        results = []
        for child in self._data.get("children", []):
            match = (name is None or child.get("name") == name) and \
                    (type is None or child.get("type") == type)
            if match:
                results.append(FigmaNode(child, self._design))
            results.extend(FigmaNode(child, self._design).find_all(name=name, type=type))
        return results

    def to_svg(self, width=None, height=None):
        return figma_node_to_svg(self._data, width=width, height=height)

    def region_to_svg(self, x, y, w, h):
        bx, by, _, _ = self.bbox
        abs_x = bx + x
        abs_y = by + y
        region_children = []
        for child in self._data.get("children", []):
            cb = child.get("absoluteBoundingBox") or {}
            cx, cy = cb.get("x", 0), cb.get("y", 0)
            cw, ch = cb.get("width", 0), cb.get("height", 0)
            if cx + cw > abs_x and cx < abs_x + w and \
               cy + ch > abs_y and cy < abs_y + h:
                region_children.append(child)
        if not region_children:
            return None
        synthetic = {
            "absoluteBoundingBox": {"x": abs_x, "y": abs_y, "width": w, "height": h},
            "children": region_children,
        }
        return figma_node_to_svg(synthetic, width=w, height=h)


# -------------------------------------------------------------------------
# FigmaPage / FigmaSection
# -------------------------------------------------------------------------

class FigmaPage(FigmaNode):

    def section(self, name):
        for child in self._data.get("children", []):
            if child.get("name") == name and child.get("type") == "SECTION":
                return FigmaSection(child, self._design)
        return None

    def frame(self, name, section=None):
        if section:
            sec = self.section(section)
            return sec.frame(name) if sec else None
        for child in self._data.get("children", []):
            if child.get("name") == name and child.get("type") == "FRAME":
                return FigmaNode(child, self._design)
        return None


class FigmaSection(FigmaNode):

    def frame(self, name):
        for child in self._data.get("children", []):
            if child.get("name") == name and child.get("type") == "FRAME":
                return FigmaNode(child, self._design)
        return None

    @property
    def frames(self):
        return [
            FigmaNode(c, self._design)
            for c in self._data.get("children", [])
            if c.get("type") == "FRAME"
        ]


# -------------------------------------------------------------------------
# FigmaDesign — generic loader
# -------------------------------------------------------------------------

class FigmaDesign:
    """Load a Figma REST API JSON and provide structured access.

    Supports /v1/files and /v1/files/.../nodes response formats.
    """

    def __init__(self, path):
        self._path = Path(path)
        with open(self._path) as f:
            self._data = json.load(f)
        self._pages = self._extract_pages()

    def _extract_pages(self):
        doc = self._data.get("document")
        if doc and "children" in doc:
            return doc["children"]
        nodes = self._data.get("nodes")
        if nodes:
            pages = []
            for node_id, node_data in nodes.items():
                node_doc = node_data.get("document", {})
                node_type = node_doc.get("type", "")
                if node_type == "CANVAS":
                    pages.append(node_doc)
                elif node_type == "SECTION":
                    pages.append({
                        "name": node_doc.get("name", ""),
                        "type": "CANVAS",
                        "children": [node_doc],
                    })
                elif node_type == "FRAME":
                    pages.append({
                        "name": node_doc.get("name", ""),
                        "type": "CANVAS",
                        "children": [{
                            "name": node_doc.get("name", ""),
                            "type": "SECTION",
                            "children": [node_doc],
                        }],
                    })
                else:
                    pages.append(node_doc)
            return pages
        return []

    @property
    def name(self):
        return self._data.get("name", "")

    @property
    def pages(self):
        return [FigmaPage(c, self) for c in self._pages]

    def page(self, name):
        for child in self._pages:
            if child.get("name") == name:
                return FigmaPage(child, self)
        return None

    def page_by_index(self, index):
        if 0 <= index < len(self._pages):
            return FigmaPage(self._pages[index], self)
        return None

    def _get_section(self):
        """Helper: get the first section from the first page."""
        page = self.pages[0] if self.pages else None
        if not page:
            return None
        for child in page.children:
            if child.type == "SECTION":
                return FigmaSection(child.raw, self)
        return None
