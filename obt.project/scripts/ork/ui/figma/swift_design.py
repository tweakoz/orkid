"""
ork.ui.figma.swift_design — macOS/Swift UI from Figma design.

SwiftFigmaDesign extends FigmaDesign with:
  - Screen spec extraction (pure data, no AppKit)
  - DMG layout extraction (for deploy_phases)
  - AppKit NSView page building (walks full Figma node tree)
"""

from ork.ui.figma.design import FigmaDesign, FigmaSection
from ork.ui.figma.converter import _get_solid_fill
from ork.ui.figma import constants as C


# SF Symbol Unicode codepoint → symbol name mapping
_SF_SYMBOL_MAP = {
    0x10218e: "arrow.triangle.2.circlepath.icloud.fill",
    0x100894: "checkmark.icloud.fill",
    0x100314: "xmark.icloud.fill",
}


def _component_prop(node, key):
    cp = node.raw.get("componentProperties", {})
    if key in cp:
        return cp[key].get("value")
    for k, v in cp.items():
        if k.startswith(key) or key in k:
            return v.get("value")
    return None


def _find_utility_panel(frame):
    """Find the Utility Panel instance and return it."""
    for node in frame.find_all(type="INSTANCE"):
        if "Utility Panel" in node.name:
            return node
    return None


def _extract_screen_spec(frame):
    """Extract UI element info from a FigmaNode frame. Pure data, no AppKit."""
    panel = _find_utility_panel(frame)
    panel_bbox = panel.raw["absoluteBoundingBox"] if panel else None
    px = panel_bbox["x"] if panel_bbox else frame.bbox[0]
    py = panel_bbox["y"] if panel_bbox else frame.bbox[1]

    heading = ""
    icon_symbol = ""
    icon_color = ""
    icon_opacity = 1.0
    icon_size = 0
    icon_center_x = 0
    icon_center_y = 0
    icon_has_glass = False
    buttons = []
    text_fields = []
    has_progress = False
    title = ""
    title_symbol = ""

    for node in frame.find_all(type="TEXT"):
        fs = node.font_size
        if fs >= C.ICON_FONT_SIZE_MIN:
            char = node.text.strip()
            if char:
                codepoint = ord(char[0])
                icon_symbol = _SF_SYMBOL_MAP.get(codepoint, "")
                if not icon_symbol and codepoint > C.SF_SYMBOL_CODEPOINT_MIN:
                    icon_symbol = "questionmark.circle"
            icon_size = fs
            bbox = node.raw.get("absoluteBoundingBox", {})
            icon_center_x = bbox.get("x", 0) + bbox.get("width", 0) / 2 - px
            icon_center_y = bbox.get("y", 0) + bbox.get("height", 0) / 2 - py
            icon_has_glass = any(e.get("type") == "GLASS" for e in node.raw.get("effects", []))
            for fill in node.raw.get("fills", []):
                if fill.get("type") == "SOLID":
                    icon_opacity = fill.get("opacity", 1.0)
                    from ork.ui.figma.converter import _figma_color_to_hex
                    icon_color = _figma_color_to_hex(fill["color"])
        elif fs >= C.HEADING_FONT_SIZE_MIN:
            heading = node.text

    # Window title from Title Bar
    if panel:
        for node in panel.find_all(name="Title "):  # note trailing space
            title = node.text
        for node in panel.find_all(name="Symbol"):
            if node.type == "TEXT" and node.font_size <= C.TITLE_SYMBOL_FONT_SIZE_MAX:
                char = node.text.strip()
                if char and ord(char[0]) > C.SF_SYMBOL_CODEPOINT_MIN:
                    title_symbol = char

    for node in frame.find_all(name="Push Button"):
        label = _component_prop(node, "Label") or ""
        style = _component_prop(node, "Style") or "Bordered Neutral"
        state = _component_prop(node, "State") or "Idle"
        bg_color = ""
        for child in node.children:
            if child.name == "BG":
                for gc in child.children:
                    f = gc.fill_color
                    if f: bg_color = f
                if not bg_color:
                    bg_color = child.fill_color or ""
        bbox = node.raw.get("absoluteBoundingBox", {})
        buttons.append({
            "label": label, "style": style, "state": state, "bg_color": bg_color,
            "x": bbox.get("x", 0) - px, "y": bbox.get("y", 0) - py,
            "w": bbox.get("width", 0), "h": bbox.get("height", 0),
        })

    for node in frame.find_all(name="Text Field"):
        placeholder = _component_prop(node, "Value") or ""
        bbox = node.raw.get("absoluteBoundingBox", {})
        text_fields.append({
            "placeholder": placeholder,
            "x": bbox.get("x", 0) - px, "y": bbox.get("y", 0) - py,
            "w": bbox.get("width", 0), "h": bbox.get("height", 0),
        })

    for node in frame.find_all(name="Determinate"):
        has_progress = True

    # Content rectangles (background areas)
    content_rects = []
    mac = frame.children[0] if frame.children else frame
    for child in mac.children:
        if child.type == "RECTANGLE" and child.fill_color and child.width > C.CONTENT_RECT_WIDTH_MIN:
            bbox = child.raw.get("absoluteBoundingBox", {})
            content_rects.append({
                "x": bbox.get("x", 0) - px, "y": bbox.get("y", 0) - py,
                "w": bbox.get("width", 0), "h": bbox.get("height", 0),
                "fill": child.fill_color, "r": child.corner_radius,
            })

    # Description texts (small)
    descriptions = []
    for node in frame.find_all(type="TEXT"):
        if C.DESCRIPTION_FONT_SIZE_MIN <= node.font_size <= C.DESCRIPTION_FONT_SIZE_MAX and node.text:
            bbox = node.raw.get("absoluteBoundingBox", {})
            descriptions.append({
                "text": node.text, "size": node.font_size,
                "x": bbox.get("x", 0) - px, "y": bbox.get("y", 0) - py,
                "color": node.fill_color or C.DEFAULT_DESCRIPTION_COLOR,
            })

    # Sheets
    sheets = []
    for node in frame.find_all(name="Sheet"):
        bbox = node.raw.get("absoluteBoundingBox", {})
        sheets.append({
            "x": bbox.get("x", 0) - px, "y": bbox.get("y", 0) - py,
            "w": bbox.get("width", 0), "h": bbox.get("height", 0),
            "r": node.corner_radius,
        })

    # App icon images
    images = []
    for child in mac.children:
        has_image = any(f.get("type") == "IMAGE" for f in child.raw.get("fills", []))
        if has_image:
            bbox = child.raw.get("absoluteBoundingBox", {})
            images.append({
                "name": child.name,
                "x": bbox.get("x", 0) - px, "y": bbox.get("y", 0) - py,
                "w": bbox.get("width", 0), "h": bbox.get("height", 0),
            })

    return {
        "heading": heading, "title": title, "title_symbol": title_symbol,
        "icon_symbol": icon_symbol, "icon_color": icon_color,
        "icon_opacity": icon_opacity, "icon_size": icon_size,
        "icon_center_x": icon_center_x, "icon_center_y": icon_center_y,
        "icon_has_glass": icon_has_glass,
        "buttons": buttons, "text_fields": text_fields, "has_progress": has_progress,
        "content_rects": content_rects, "descriptions": descriptions,
        "sheets": sheets, "images": images,
    }


# ---------------------------------------------------------------------------
# FigmaPage — returned by build_page
# ---------------------------------------------------------------------------

class FigmaPage:
    def __init__(self):
        self.view = None
        self.heading_label = None
        self.detail_label = None
        self.buttons = {}
        self.text_fields = {}
        self.progress_bar = None
        self.percent_label = None


# ---------------------------------------------------------------------------
# SwiftFigmaDesign
# ---------------------------------------------------------------------------

class SwiftFigmaDesign(FigmaDesign):

    def __init__(self, path):
        super().__init__(path)
        self._screens = {}
        self._frames = {}

        sec = self._get_section()
        if not sec:
            return
        for frame in sec.frames:
            self._screens[frame.name] = _extract_screen_spec(frame)
            self._frames[frame.name] = frame

    @property
    def window_size(self):
        for frame in self._frames.values():
            panel = _find_utility_panel(frame)
            if panel:
                return (round(panel.width), round(panel.height))
        return C.DEFAULT_WINDOW_SIZE

    @property
    def window_title(self):
        for spec in self._screens.values():
            if spec["title"]:
                return spec["title"]
        return "Impossible Downloader"

    @property
    def screen_names(self):
        return list(self._screens.keys())

    def screen_spec(self, name):
        return self._screens.get(name)

    def dmg_layout(self, screen_name="Drap&Drop Downloader"):
        frame = self._frames.get(screen_name)
        if not frame:
            return None
        fx, fy = frame.bbox[0], frame.bbox[1]
        window_rect = None
        for child in frame.find_all(type="RECTANGLE"):
            has_image = any(f.get("type") == "IMAGE" for f in child.raw.get("fills", []))
            if child.fill_color == "#ffffff" and child.width > C.DMG_WINDOW_WIDTH_MIN and not has_image:
                window_rect = child
                break
        if not window_rect:
            return None
        wb = window_rect.raw["absoluteBoundingBox"]
        wx, wy, ww, wh = wb["x"], wb["y"], round(wb["width"]), round(wb["height"])
        icons = []
        for child in frame.find_all(type="RECTANGLE"):
            has_image = any(f.get("type") == "IMAGE" for f in child.raw.get("fills", []))
            if has_image and C.DMG_ICON_WIDTH_MIN < child.width < C.DMG_ICON_WIDTH_MAX:
                b = child.raw["absoluteBoundingBox"]
                icons.append({"x": round(b["x"] - wx), "y": round(b["y"] - wy),
                              "w": round(b["width"]), "h": round(b["height"])})
        icons.sort(key=lambda i: i["x"])
        result = {"window_width": ww, "window_height": wh, "bg_color": "#ffffff",
                  "icon_size": icons[0]["w"] if icons else C.DEFAULT_DMG_ICON_SIZE}
        if len(icons) >= 2:
            result["app_icon_x"] = icons[0]["x"] + icons[0]["w"] // 2
            result["app_icon_y"] = icons[0]["y"] + icons[0]["h"] // 2
            result["apps_folder_x"] = icons[1]["x"] + icons[1]["w"] // 2
            result["apps_folder_y"] = icons[1]["y"] + icons[1]["h"] // 2
        return result

    # ── AppKit page building ──────────────────────────────────────────

    def build_page(self, name, parent_bounds=None, actions=None):
        """Build an NSView from a Figma screen, faithfully recreating the layout."""
        from AppKit import (
            NSView, NSTextField, NSButton, NSProgressIndicator, NSFont, NSColor,
            NSTextAlignmentCenter, NSTextAlignmentLeft, NSBezelStyleRounded,
            NSProgressIndicatorBarStyle, NSImage, NSImageView,
            NSImageSymbolConfiguration, NSImageScaleProportionallyUpOrDown,
            NSVisualEffectView, NSBox,
        )
        from Foundation import NSMakeRect, NSMakePoint

        spec = self._screens.get(name)
        if spec is None:
            return None

        actions = actions or {}
        w, h = self.window_size
        bounds = parent_bounds or NSMakeRect(0, 0, w, h)

        def flip_y(figma_y, elem_h):
            """Convert Figma top-down Y to AppKit bottom-up Y."""
            return h - figma_y - elem_h

        def hex_color(hex_str, opacity=1.0):
            if not hex_str: return None
            s = hex_str.lstrip("#")
            return NSColor.colorWithRed_green_blue_alpha_(
                int(s[0:2],16)/255, int(s[2:4],16)/255, int(s[4:6],16)/255, opacity)

        def make_label(text, x, y, lw, lh, size=C.LABEL_FONT_SIZE, bold=False, color=None, align=NSTextAlignmentLeft):
            lbl = NSTextField.alloc().initWithFrame_(NSMakeRect(x, flip_y(y, lh), lw, lh))
            lbl.setStringValue_(text)
            lbl.setBezeled_(False)
            lbl.setDrawsBackground_(False)
            lbl.setEditable_(False)
            lbl.setSelectable_(False)
            lbl.setFont_(NSFont.boldSystemFontOfSize_(size) if bold else NSFont.systemFontOfSize_(size))
            lbl.setAlignment_(align)
            if color: lbl.setTextColor_(color)
            return lbl

        page = FigmaPage()
        view = NSView.alloc().initWithFrame_(bounds)
        page.view = view

        # Walk the Figma child order to maintain correct Z-order.
        frame = self._frames.get(name)
        panel = _find_utility_panel(frame) if frame else None
        pb = panel.raw["absoluteBoundingBox"] if panel else {"x": frame.bbox[0], "y": frame.bbox[1]}
        mac = frame.children[0] if frame and frame.children else None

        if mac:
            for child in mac.children:
                if not child.raw.get("visible", True):
                    continue
                bbox = child.raw.get("absoluteBoundingBox", {})
                cx = bbox.get("x", 0) - pb["x"]
                cy = bbox.get("y", 0) - pb["y"]
                cw = bbox.get("width", 0)
                ch = bbox.get("height", 0)

                if child.type == "RECTANGLE" and child.fill_color:
                    box = NSBox.alloc().initWithFrame_(
                        NSMakeRect(cx, flip_y(cy, ch), cw, ch))
                    box.setBoxType_(C.APPKIT_BOX_TYPE)
                    box.setBorderType_(C.APPKIT_BORDER_TYPE)
                    box.setFillColor_(hex_color(child.fill_color))
                    box.setCornerRadius_(child.corner_radius)
                    view.addSubview_(box)

                elif child.type == "INSTANCE" and "Utility Panel" in child.name:
                    pass  # Window chrome — handled by the OS

                elif child.type == "INSTANCE" and "Sheet" in child.name:
                    box = NSBox.alloc().initWithFrame_(
                        NSMakeRect(cx, flip_y(cy, ch), cw, ch))
                    box.setBoxType_(C.APPKIT_BOX_TYPE)
                    box.setBorderType_(C.APPKIT_BORDER_TYPE)
                    box.setFillColor_(NSColor.whiteColor())
                    box.setCornerRadius_(child.corner_radius or C.DEFAULT_SHEET_CORNER_RADIUS)
                    view.addSubview_(box)

                elif child.type == "INSTANCE" and child.name == "Push Button":
                    label = _component_prop(child, "Label") or ""
                    style = _component_prop(child, "Style") or ""
                    state = _component_prop(child, "State") or "Idle"
                    bg = ""
                    for sub in child.children:
                        if sub.name == "BG":
                            for gc in sub.children:
                                if gc.fill_color: bg = gc.fill_color
                            if not bg: bg = sub.fill_color or ""
                    btn = NSButton.alloc().initWithFrame_(
                        NSMakeRect(cx, flip_y(cy, ch), cw, ch))
                    btn.setTitle_(label)
                    btn.setBezelStyle_(NSBezelStyleRounded)
                    if style == "Bordered Colored" and bg:
                        btn.setContentTintColor_(hex_color(bg))
                    if state == "Disabled":
                        btn.setEnabled_(False)
                    a = actions.get(label)
                    if a:
                        btn.setTarget_(a[0])
                        btn.setAction_(a[1])
                    view.addSubview_(btn)
                    page.buttons[label] = btn

                elif child.type == "INSTANCE" and "Text Field" in child.name:
                    placeholder = _component_prop(child, "Value") or ""
                    tf = NSTextField.alloc().initWithFrame_(
                        NSMakeRect(cx, flip_y(cy, ch), cw, ch))
                    tf.setPlaceholderString_(placeholder)
                    tf.setFont_(NSFont.systemFontOfSize_(C.LABEL_FONT_SIZE))
                    view.addSubview_(tf)
                    page.text_fields[placeholder] = tf

                elif child.type == "INSTANCE" and "Determinate" in child.name:
                    bar = NSProgressIndicator.alloc().initWithFrame_(
                        NSMakeRect(cx, flip_y(cy, ch), cw, ch))
                    bar.setStyle_(NSProgressIndicatorBarStyle)
                    bar.setMinValue_(C.APPKIT_PROGRESS_MIN)
                    bar.setMaxValue_(C.APPKIT_PROGRESS_MAX)
                    bar.setDoubleValue_(C.APPKIT_PROGRESS_MIN)
                    bar.setIndeterminate_(False)
                    view.addSubview_(bar)
                    page.progress_bar = bar

                elif child.type == "TEXT" and child.font_size >= C.ICON_FONT_SIZE_MIN:
                    char = child.text.strip()
                    codepoint = ord(char[0]) if char else 0
                    symbol_name = _SF_SYMBOL_MAP.get(codepoint, "")
                    if symbol_name:
                        symbol_img = NSImage.imageWithSystemSymbolName_accessibilityDescription_(
                            symbol_name, None)
                        if symbol_img:
                            pt = child.font_size
                            size_cfg = NSImageSymbolConfiguration.configurationWithPointSize_weight_(pt, 0)
                            symbol_img = symbol_img.imageWithSymbolConfiguration_(size_cfg)

                            fill_opacity = 1.0
                            for fill in child.raw.get("fills", []):
                                if fill.get("type") == "SOLID":
                                    fill_opacity = fill.get("opacity", 1.0)
                            ic = hex_color(child.fill_color or "#ffffff", 1.0)

                            icon_view = NSImageView.alloc().initWithFrame_(
                                NSMakeRect(cx - pt/2 + cw/2, flip_y(cy - pt/2 + ch/2, pt), pt, pt))
                            icon_view.setImage_(symbol_img)
                            icon_view.setImageScaling_(NSImageScaleProportionallyUpOrDown)
                            if ic:
                                icon_view.setContentTintColor_(ic)
                            icon_view.setAlphaValue_(fill_opacity)
                            view.addSubview_(icon_view)

                elif child.type == "TEXT" and child.font_size >= C.HEADING_FONT_SIZE_MIN:
                    hh = child.font_size * C.HEADING_HEIGHT_MULTIPLIER
                    lbl = make_label(child.text, cx, cy, cw, hh,
                                   size=child.font_size, bold=(child.font_weight >= C.BOLD_FONT_WEIGHT_MIN),
                                   color=hex_color(child.fill_color) if child.fill_color != "#000000" else None)
                    view.addSubview_(lbl)
                    page.heading_label = lbl

                elif child.type == "TEXT" and C.DESCRIPTION_FONT_SIZE_MIN <= child.font_size <= C.DESCRIPTION_FONT_SIZE_MAX:
                    lbl = make_label(child.text, cx, cy, max(cw, C.APPKIT_DESCRIPTION_MIN_WIDTH), C.APPKIT_DETAIL_HEIGHT,
                                   size=child.font_size,
                                   color=hex_color(child.fill_color or C.DEFAULT_DESCRIPTION_COLOR, C.DESCRIPTION_TEXT_OPACITY))
                    view.addSubview_(lbl)

        # Fallback heading if not found in tree walk
        if not page.heading_label and spec["heading"]:
            heading_node = None
            if frame:
                for node in frame.find_all(type="TEXT"):
                    if C.HEADING_FONT_SIZE_MIN <= node.font_size < C.ICON_FONT_SIZE_MIN and node.text == spec["heading"]:
                        heading_node = node
                        break
            if heading_node:
                hb = heading_node.raw["absoluteBoundingBox"]
                hx = hb["x"] - pb["x"]
                hy = hb["y"] - pb["y"]
                hw = hb["width"]
                heading_size = heading_node.font_size
                heading_weight = heading_node.font_weight
            else:
                hx, hy, hw = C.APPKIT_DETAIL_MARGIN, h/2 - C.APPKIT_DETAIL_MARGIN, w - C.APPKIT_DETAIL_MARGIN * 2
                heading_size = C.DEFAULT_HEADING_FONT_SIZE
                heading_weight = C.DEFAULT_HEADING_FONT_WEIGHT

            hh = heading_size * C.HEADING_HEIGHT_MULTIPLIER
            lbl = make_label(spec["heading"], hx, hy, hw, hh,
                           size=heading_size, bold=(heading_weight >= C.BOLD_FONT_WEIGHT_MIN))
            view.addSubview_(lbl)
            page.heading_label = lbl

        # Detail label (for dynamic status messages — not in Figma, app-level)
        detail = make_label("", C.APPKIT_DETAIL_MARGIN, h - C.APPKIT_DETAIL_MARGIN * 2,
                          w - C.APPKIT_DETAIL_MARGIN * 2, C.APPKIT_DETAIL_HEIGHT,
                          size=C.LABEL_FONT_SIZE,
                          color=NSColor.secondaryLabelColor(), align=NSTextAlignmentCenter)
        view.addSubview_(detail)
        page.detail_label = detail

        # Cancel button for progress pages (app-level, not in Figma)
        if spec["has_progress"]:
            a = actions.get("Cancel")
            if a:
                cancel = NSButton.alloc().initWithFrame_(NSMakeRect(0, 0, 0, 0))
                cancel.setTitle_("Cancel")
                cancel.setBezelStyle_(NSBezelStyleRounded)
                cancel.sizeToFit()
                cf = cancel.frame()
                cancel.setFrameOrigin_(NSMakePoint(
                    w - cf.size.width - C.APPKIT_CANCEL_MARGIN_RIGHT,
                    C.APPKIT_CANCEL_MARGIN_BOTTOM))
                cancel.setTarget_(a[0])
                cancel.setAction_(a[1])
                view.addSubview_(cancel)
                page.buttons["Cancel"] = cancel

        return page
