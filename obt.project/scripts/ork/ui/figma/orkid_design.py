"""
ork.ui.figma.orkid_design — Orkid card UI intelligence.

OrkidFigmaDesign extends FigmaDesign with methods for orkid's
FilesystemView card UI: detecting cards, deriving hover/selected
states, extracting card SVGs.
"""

from ork.ui.figma.design import FigmaDesign, FigmaSection
from ork.ui.figma.converter import _get_solid_fill, _get_solid_stroke
from ork.ui.figma import constants as C


class CardRegion:
    def __init__(self, x, y, w, h, bg_color=None):
        self.x = x
        self.y = y
        self.w = w
        self.h = h
        self.bg_color = bg_color

    def __repr__(self):
        return f"CardRegion({self.x}, {self.y}, {self.w}x{self.h}, bg={self.bg_color})"


class PanelSpec:
    def __init__(self):
        self.gap = C.PANEL_DEFAULT_GAP
        self.width = C.PANEL_DEFAULT_WIDTH
        self.height = C.PANEL_DEFAULT_HEIGHT
        self.corner_radius = C.PANEL_DEFAULT_CORNER_RADIUS
        self.fill = "#efefef"
        self.stroke = "#212121"
        self.stroke_width = C.PANEL_DEFAULT_STROKE_WIDTH
        self.text_fill = "#494949"
        self.text_size = C.PANEL_DEFAULT_TEXT_SIZE
        self.text_offset_x = C.PANEL_DEFAULT_TEXT_OFFSET_X
        self.text_offset_y = C.PANEL_DEFAULT_TEXT_OFFSET_Y


class OrkidFigmaDesign(FigmaDesign):
    """FigmaDesign with orkid card UI methods."""

    def detect_cards(self, frame, min_size=None, min_radius=None):
        if min_size is None:
            min_size = C.CARD_MIN_SIZE
        if min_radius is None:
            min_radius = C.CARD_MIN_RADIUS
        fx, fy = frame.bbox[0], frame.bbox[1]
        cards = []
        for child in frame.raw.get("children", []):
            if child.get("type") != "RECTANGLE":
                continue
            bbox = child["absoluteBoundingBox"]
            w, h = bbox["width"], bbox["height"]
            r = child.get("cornerRadius", 0)
            if w >= min_size and h >= min_size and r >= min_radius:
                cards.append(CardRegion(
                    x=round(bbox["x"] - fx), y=round(bbox["y"] - fy),
                    w=round(w), h=round(h),
                    bg_color=_get_solid_fill(child),
                ))
        cards.sort(key=lambda c: c.x)
        return cards

    def extract_card_svgs(self, frame, cards):
        return [frame.region_to_svg(c.x, c.y, c.w, c.h) for c in cards]

    def icon_colors_in_region(self, frame, card):
        fx, fy = frame.bbox[0], frame.bbox[1]
        abs_cx, abs_cy = fx + card.x, fy + card.y
        colors = set()
        for child in frame.raw.get("children", []):
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

    def derive_hover_color(self, default_frame, hover_frame):
        fx, fy = default_frame.bbox[0], default_frame.bbox[1]
        hfx, hfy = hover_frame.bbox[0], hover_frame.bbox[1]
        d_fills = {}
        for c in default_frame.raw.get("children", []):
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

    def make_hover_svg(self, frame, card, hover_color, icon_colors=None):
        svg = frame.region_to_svg(card.x, card.y, card.w, card.h)
        if not svg:
            return None
        if icon_colors is None:
            icon_colors = self.icon_colors_in_region(frame, card)
        for color in icon_colors:
            svg = svg.replace(f'fill="{color}"', f'fill="{hover_color}"')
            svg = svg.replace(f'stroke="{color}"', f'stroke="{hover_color}"')
        return svg

    def detect_panel(self, selected_frame, cards):
        if not cards:
            return PanelSpec()
        fx, fy = selected_frame.bbox[0], selected_frame.bbox[1]
        card_bottom = cards[0].y + cards[0].h
        spec = PanelSpec()
        panel_x = panel_y = None
        for child in selected_frame.raw.get("children", []):
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
                spec.stroke_width = child.get("strokeWeight", C.PANEL_DEFAULT_STROKE_WIDTH)
                spec.gap = ry - card_bottom
                panel_x = round(bbox["x"] - fx)
                panel_y = ry
            elif child.get("type") == "TEXT":
                style = child.get("style", {})
                spec.text_fill = _get_solid_fill(child) or spec.text_fill
                spec.text_size = style.get("fontSize", spec.text_size)
                if panel_x is not None and panel_y is not None:
                    spec.text_offset_x = round(bbox["x"] - fx) - panel_x
                    spec.text_offset_y = ry - panel_y
        return spec

    def extract_selected_svg(self, selected_frame, card, panel):
        total_h = card.h + panel.gap + int(panel.height)
        return selected_frame.region_to_svg(card.x, card.y, card.w, total_h)
