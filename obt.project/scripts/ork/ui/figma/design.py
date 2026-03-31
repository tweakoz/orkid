"""
ork.ui.figma.design — FigmaDesign class for structured access to Figma exports.

Load a Figma REST API JSON export once, then query pages, frames, and
elements by name.  Convert any node or region to SVG deterministically.

Provides frame comparison methods to derive hover colors, icon colors,
panel specs, and card regions — all from the Figma data itself.

Usage::

    design = FigmaDesign("/path/to/figma.json")
    page = design.page("UI_V2")
    section = page.section("UI_V2")
    default = section.frame("Default_State")
    hover = section.frame("Hover_State")
    selected = section.frame("Selected_State")

    # Detect cards (large rounded rects)
    cards = default.detect_cards()

    # Extract card SVGs
    svgs = default.extract_card_svgs(cards)

    # Derive hover color by comparing frames
    hover_color = default.derive_hover_color(hover)

    # Get icon colors per card region
    icon_colors = default.icon_colors_in_region(card)

    # Build hover SVG by replacing icon colors
    hover_svg = default.make_hover_svg(card, hover_color)

    # Extract panel specs from selected frame
    panel = selected.detect_panel(cards)

    # Build selected SVG (hover card + panel)
    selected_svg = default.make_selected_svg(card, hover_color, panel, "description")
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
# Card region descriptor
# -------------------------------------------------------------------------

class CardRegion:
    """A detected card region in a frame."""
    def __init__(self, x, y, w, h, bg_color=None):
        self.x = x
        self.y = y
        self.w = w
        self.h = h
        self.bg_color = bg_color

    def __repr__(self):
        return f"CardRegion({self.x}, {self.y}, {self.w}x{self.h}, bg={self.bg_color})"


# -------------------------------------------------------------------------
# Panel descriptor
# -------------------------------------------------------------------------

class PanelSpec:
    """Description panel specs derived from a selected-state frame."""
    def __init__(self):
        self.gap = 32         # gap between card bottom and panel top
        self.width = 300
        self.height = 263
        self.corner_radius = 40
        self.fill = "#efefef"
        self.stroke = "#212121"
        self.stroke_width = 2
        self.text_fill = "#494949"
        self.text_size = 16
        self.text_offset_x = 33
        self.text_offset_y = 37


# -------------------------------------------------------------------------
# FigmaNode
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
        b = self._data.get("absoluteBoundingBox", {})
        return b.get("x", 0), b.get("y", 0), b.get("width", 0), b.get("height", 0)

    @property
    def width(self):
        return self._data.get("absoluteBoundingBox", {}).get("width", 0)

    @property
    def height(self):
        return self._data.get("absoluteBoundingBox", {}).get("height", 0)

    @property
    def fill_color(self):
        return _get_solid_fill(self._data)

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
    def children(self):
        return [FigmaNode(c, self._design) for c in self._data.get("children", [])]

    @property
    def raw(self):
        return self._data

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
            cb = child.get("absoluteBoundingBox", {})
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
# FigmaFrame — with card UI intelligence
# -------------------------------------------------------------------------

class FigmaFrame(FigmaNode):
    """A Figma FRAME with methods to detect cards, derive states, and build SVGs."""

    def detect_cards(self, min_size=200, min_radius=10):
        """Detect card positions by finding large rounded rectangles.

        Returns list of CardRegion sorted left-to-right.
        """
        fx, fy = self.bbox[0], self.bbox[1]
        cards = []
        for child in self._data.get("children", []):
            if child.get("type") != "RECTANGLE":
                continue
            bbox = child["absoluteBoundingBox"]
            w, h = bbox["width"], bbox["height"]
            r = child.get("cornerRadius", 0)
            if w >= min_size and h >= min_size and r >= min_radius:
                cards.append(CardRegion(
                    x=round(bbox["x"] - fx),
                    y=round(bbox["y"] - fy),
                    w=round(w),
                    h=round(h),
                    bg_color=_get_solid_fill(child),
                ))
        cards.sort(key=lambda c: c.x)
        return cards

    def extract_card_svgs(self, cards):
        """Extract SVGs for a list of CardRegion."""
        return [self.region_to_svg(c.x, c.y, c.w, c.h) for c in cards]

    def icon_colors_in_region(self, card):
        """Collect non-background fill and stroke colors within a card region.

        These are the icon/decoration colors that change on hover.
        """
        fx, fy = self.bbox[0], self.bbox[1]
        abs_cx, abs_cy = fx + card.x, fy + card.y
        colors = set()

        for child in self._data.get("children", []):
            bbox = child.get("absoluteBoundingBox", {})
            bx, by = bbox.get("x", 0), bbox.get("y", 0)
            bw, bh = bbox.get("width", 0), bbox.get("height", 0)
            if not (bx + bw > abs_cx and bx < abs_cx + card.w and
                    by + bh > abs_cy and by < abs_cy + card.h):
                continue
            if child.get("type") == "TEXT":
                continue
            fill = _get_solid_fill(child)
            if fill and fill != card.bg_color:
                colors.add(fill)
            stroke = _get_solid_stroke(child)
            if stroke:
                colors.add(stroke)
        return list(colors)

    def derive_hover_color(self, hover_frame):
        """Compare this frame's elements with a hover frame to find the replacement color.

        Returns the color that icons change to on hover.
        """
        fx, fy = self.bbox[0], self.bbox[1]
        hfx, hfy = hover_frame.bbox[0], hover_frame.bbox[1]

        d_fills = {}
        for c in self._data.get("children", []):
            if c.get("type") == "BOOLEAN_OPERATION":
                bbox = c["absoluteBoundingBox"]
                key = (round(bbox["x"] - fx), round(bbox["y"] - fy))
                d_fills[key] = _get_solid_fill(c)

        for c in hover_frame.raw.get("children", []):
            if c.get("type") == "BOOLEAN_OPERATION":
                bbox = c["absoluteBoundingBox"]
                key = (round(bbox["x"] - hfx), round(bbox["y"] - hfy))
                hf = _get_solid_fill(c)
                df = d_fills.get(key)
                if df and hf and df != hf:
                    return hf
        return None

    def make_hover_svg(self, card, hover_color, icon_colors=None):
        """Build a hover-state SVG for a card by replacing icon colors.

        Args:
            card: CardRegion
            hover_color: The color icons change to on hover.
            icon_colors: List of colors to replace. If None, auto-detected.
        """
        svg = self.region_to_svg(card.x, card.y, card.w, card.h)
        if not svg:
            return None
        if icon_colors is None:
            icon_colors = self.icon_colors_in_region(card)
        for color in icon_colors:
            svg = svg.replace(f'fill="{color}"', f'fill="{hover_color}"')
            svg = svg.replace(f'stroke="{color}"', f'stroke="{hover_color}"')
        return svg

    def detect_panel(self, cards):
        """Detect the description panel from a selected-state frame.

        Looks for elements below the card row (large rounded rect + text).
        Returns a PanelSpec.
        """
        if not cards:
            return PanelSpec()

        fx, fy = self.bbox[0], self.bbox[1]
        card_bottom = cards[0].y + cards[0].h  # all cards at same y

        spec = PanelSpec()

        for child in self._data.get("children", []):
            bbox = child["absoluteBoundingBox"]
            ry = round(bbox["y"] - fy)
            if ry <= card_bottom:
                continue

            if child.get("type") == "RECTANGLE":
                spec.width = bbox["width"]
                spec.height = bbox["height"]
                spec.corner_radius = child.get("cornerRadius", 0)
                spec.fill = _get_solid_fill(child) or spec.fill
                spec.stroke = _get_solid_stroke(child) or spec.stroke
                spec.stroke_width = child.get("strokeWeight", 2)
                spec.gap = ry - card_bottom
                panel_x = round(bbox["x"] - fx)
                panel_y = ry

            elif child.get("type") == "TEXT":
                style = child.get("style", {})
                spec.text_fill = _get_solid_fill(child) or spec.text_fill
                spec.text_size = style.get("fontSize", spec.text_size)
                text_x = round(bbox["x"] - fx)
                text_y = round(bbox["y"] - fy)
                if "panel_x" in dir() and "panel_y" in dir():
                    spec.text_offset_x = text_x - panel_x
                    spec.text_offset_y = text_y - panel_y

        return spec

    def extract_selected_svg(self, selected_frame, card, panel):
        """Extract a selected-state SVG from a selected frame.

        Extracts a taller region that includes the card and the panel
        below it, directly from the selected frame's elements.

        Args:
            selected_frame: The FigmaFrame for the selected state.
            card: CardRegion of the card to extract.
            panel: PanelSpec from detect_panel().

        Returns:
            SVG string with viewBox matching the card+panel region.
        """
        total_h = card.h + panel.gap + int(panel.height)
        return selected_frame.region_to_svg(card.x, card.y, card.w, total_h)


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
                return FigmaFrame(child, self._design)
        return None


class FigmaSection(FigmaNode):

    def frame(self, name):
        for child in self._data.get("children", []):
            if child.get("name") == name and child.get("type") == "FRAME":
                return FigmaFrame(child, self._design)
        return None

    @property
    def frames(self):
        return [
            FigmaFrame(c, self._design)
            for c in self._data.get("children", [])
            if c.get("type") == "FRAME"
        ]


# -------------------------------------------------------------------------
# FigmaDesign
# -------------------------------------------------------------------------

class FigmaDesign:
    """Top-level container for a Figma REST API JSON export."""

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
