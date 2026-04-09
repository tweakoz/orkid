################################################################################
# Catalog Tool — UI Library (reusable canvas/overlay/outliner boilerplate)
#
# This module provides the reusable UI layer for catalog tools, following an
# MVC pattern. It contains:
#
#   - Outliner models (CatalogOutlinerModel, ImportConfigEditorModel,
#     TestMatchOutlinerModel) that implement the OutlinerModel interface
#     (getChildren / getDisplayName / hasChildren) to present hierarchical
#     tree data.  Each node is identified by a string key that encodes its
#     path in the tree.
#
#   - CanvasDetailView — a base class for PrimCanvas-based detail panels
#     that renders text, buttons, progress bars, and grids via GPU
#     primitives (QuadPrimitive / TextPrimitive).
#
#   - ImportConfigEditor — a self-contained overlay editor for import config
#     JSON files, built from a BorderFrame > VPack > (Toolbar + HPack >
#     Outliner + PropertySheet) widget tree.
#
# Coordinate note: layout code uses a top-down Y axis (y=0 at the top),
# but QuadData uses OpenGL bottom-up coordinates.  Quad positions are
# therefore flipped as:  quad_y = canvas_height - layout_y - quad_height.
################################################################################

import os, json, copy
from orkengine import core
from orkengine import lev2
from ork.catalog_tool import (
  CatalogModel, format_size, sanitize_path, resolve_bad_path,
  build_namespace_varmap, build_asset_varmap,
)

tokens = core.CrcStringProxy()
vec2 = core.vec2
vec3 = core.vec3
vec4 = core.vec4

################################################################################
# Outliner Models
#
# Each outliner model implements the OutlinerModel interface:
#   getChildren(parent_key) -> list[str]   — child keys for a node
#   getDisplayName(key)     -> str         — label shown in the tree
#   hasChildren(key)        -> bool        — whether to draw expand arrow
#
# Nodes are identified by opaque string keys.  The key encoding scheme
# varies per model (see per-class comments below).
################################################################################

class CatalogOutlinerModel(lev2.ui.OutlinerModel):
  """Outliner tree model: projects -> namespaces -> assets.

  Key encoding scheme (three levels):
    "project"              — top-level project node
    "project/namespace"    — namespace within a project
    "project/ns|asset"     — leaf asset (pipe separates ns from asset id)

  The "|" character distinguishes asset leaves from namespace branches,
  so hasChildren simply checks for its absence.
  """

  def __init__(self, model):
    super().__init__()
    self.model = model       # CatalogModel from catalog_tool.py
    self.allow_rename = False
    self.allow_delete = False
    self.allow_add = False

  def getChildren(self, parent_key):
    # Root -> list of project names
    if parent_key == "":
      return self.model.projects
    # Project -> "project/ns" keys for each namespace
    if parent_key in self.model.project_namespaces:
      return [f"{parent_key}/{ns}" for ns in self.model.project_namespaces[parent_key]]
    # Namespace -> "project/ns|asset" keys for each asset (only if no "|")
    if "/" in parent_key and "|" not in parent_key:
      ns = parent_key.split("/", 1)[1]
      return [f"{parent_key.split('/')[0]}/{fqid}" for fqid in self.model.ns_assets.get(ns, [])]
    return []

  def getDisplayName(self, key):
    # Asset leaf: show just the asset name after "|"
    if "|" in key:
      return key.split("|", 1)[1]
    # Namespace: show namespace with label
    if "/" in key:
      return f"{key.split('/', 1)[1]} (namespace)"
    # Project: show project with label
    return f"{key} (project)"

  def hasChildren(self, key):
    # Assets (containing "|") are always leaves
    return "|" not in key


class ImportConfigEditorModel(lev2.ui.OutlinerModel):
  """Outliner model for editing an import config JSON.

  Key encoding (two levels):
    "Config"            — the global config section
    "AssetPaks"         — parent group for asset paks
    "AssetPaks/<id>"    — individual asset pak entry

  The model reads/writes through self.editor._editor_data, which is
  the shared mutable dict backing the ImportConfigEditor.
  """

  def __init__(self, editor):
    super().__init__()
    self.editor = editor     # ImportConfigEditor instance (owns _editor_data)
    self.allow_rename = True
    self.allow_delete = True
    self.allow_add = True

  @property
  def data(self):
    # Shared mutable dict — edits here are immediately visible to the editor
    return self.editor._editor_data

  def getChildren(self, parent_key):
    if parent_key == "":
      return ["Config", "AssetPaks"]   # Two fixed top-level nodes
    if parent_key == "AssetPaks":
      assets = self.data.get("assets", [])
      return [f"AssetPaks/{a['id']}" for a in assets]
    return []

  def getDisplayName(self, key):
    if key in ("Config", "AssetPaks"):
      return key
    if key.startswith("AssetPaks/"):
      return key.split("/", 1)[1]
    return key

  def hasChildren(self, key):
    return key == "AssetPaks"

  def getFactories(self, parent_key):
    if parent_key == "AssetPaks":
      return [{"id": "assetpak", "display_name": "AssetPak"}]
    return []

  def createItem(self, parent_key, name, factory_id):
    """Add a new asset pak entry.  Returns "" if name already exists (reject duplicate)."""
    if parent_key == "AssetPaks" and factory_id == "assetpak":
      assets = self.data.setdefault("assets", [])
      for a in assets:
        if a["id"] == name:
          return ""              # Duplicate — reject
      assets.append({"id": name, "include": "*"})
      new_key = f"AssetPaks/{name}"
      self.editor._editor_dirty = True
      self.notifyItemAdded(new_key)
      return new_key
    return ""

  def renameItem(self, old_key, new_name):
    """Rename an asset pak.  Returns None if rejected (only pak items are renameable)."""
    if not old_key.startswith("AssetPaks/"):
      return None
    old_id = old_key.split("/", 1)[1]
    assets = self.data.get("assets", [])
    # Reject if new name already exists
    for a in assets:
      if a["id"] == new_name:
        return None
    for a in assets:
      if a["id"] == old_id:
        a["id"] = new_name
        self.editor._editor_dirty = True
        return f"AssetPaks/{new_name}"
    return None

  def removeItem(self, key):
    """Delete an asset pak entry by key."""
    if not key.startswith("AssetPaks/"):
      return
    pak_id = key.split("/", 1)[1]
    assets = self.data.get("assets", [])
    self.data["assets"] = [a for a in assets if a["id"] != pak_id]
    self.editor._editor_dirty = True
    self.notifyItemRemoved(key)


class TestMatchOutlinerModel(lev2.ui.OutlinerModel):
  """Read-only outliner showing matched files per asset pak.

  Key encoding:
    "<pak_id>"              — top-level pak node (shows file count)
    "<pak_id>/<rel_path>"   — individual matched file (leaf)

  results is a list of (pak_id, [pathlib.Path, ...]) tuples.
  """

  def __init__(self, results, source_dir):
    super().__init__()
    self._results = results      # [(pak_id, [Path, ...]), ...]
    self._source_dir = source_dir
    self.allow_rename = False
    self.allow_delete = False
    self.allow_add = False

  def getChildren(self, parent_key):
    # Root -> one node per asset pak
    if parent_key == "":
      return [pak_id for pak_id, _ in self._results]
    # Pak -> one leaf per matched file (shown as relative path)
    for pak_id, files in self._results:
      if pak_id == parent_key:
        src = self._source_dir
        children = []
        for f in files:
          try:
            rel = str(f.relative_to(src)) if src else str(f)
          except ValueError:
            rel = str(f)
          children.append(f"{pak_id}/{rel}")
        return children
    return []

  def getDisplayName(self, key):
    # Pak node: append file count
    if "/" not in key:
      for pak_id, files in self._results:
        if pak_id == key:
          return f"{key} ({len(files)} files)"
      return key
    # File leaf: show relative path only
    return key.split("/", 1)[1]

  def hasChildren(self, key):
    # Paks (no "/") have children; files are leaves
    return "/" not in key


################################################################################
# CanvasDetailView
#
# Base class for GPU-rendered detail panels using PrimCanvas.
#
# Key concepts:
#   - "Text roles" are named TextPrimitive instances (e.g. "label", "value",
#     "status_ok") each with a fixed color.  Subclasses add text items to
#     the appropriate role to get correctly colored text.
#   - bg_prim is a single QuadPrimitive that accumulates all background
#     rectangles (buttons, section headers, progress bars, chunk grids).
#   - The button registry (self.buttons) maps button names to (x,y,w,h)
#     rects for hit-testing in handleCanvasEvent.
#   - Layout uses top-down Y (y=0 at top of canvas), but QuadData uses
#     OpenGL bottom-up coords, so quad positions are flipped:
#       quad_y = canvas_height - layout_y - quad_height
################################################################################

class CanvasDetailView:
  """Base class for PrimCanvas-based property/detail panels."""

  def __init__(self, canvas):
    self.canvas = canvas
    self.buttons = {}          # name -> (x, y, w, h) for hit-testing
    self.hover_button = None   # currently hovered button name (or None)
    self.bg_prim = None        # shared QuadPrimitive for all background quads
    self._texts = {}           # role_name -> TextPrimitive
    self.text_sm = None        # small-font text prim (auxiliary)
    self.on_text_clicked = None  # callback(text_value) — fired when clickable text is clicked
    # Flash highlight: 2-frame animation (frame 0 = highlight, frame 1 = restore)
    self._flash_rect = None    # (x, y, w, h) in layout coords, or None
    self._flash_frames = 0     # frames remaining (2 = draw highlight, 1 = redraw normal)

  def gpuInit(self, ctx):
    """Create quad prim + text prims by named color role."""
    self.canvas.gpuInit(ctx)
    self.canvas_font = lev2.FontManager.fontForId("i14")
    self.canvas_font_sm = lev2.FontManager.fontForId("i12")
    self.canvas_layer = self.canvas.createLayer("status")

    # Single quad primitive collects all background rectangles
    self.bg_prim = lev2.ui.QuadPrimitive(pipeline=self.canvas.pipelineSolid)
    self.canvas_layer.addPrimitive(self.bg_prim)

    # Each "text role" is a TextPrimitive with a distinct color.
    # Subclasses add items via self._texts["role"].addItem(text, position).
    text_colors = {
      "label": vec4(0.6, 0.6, 0.7, 1),
      "value": vec4(0.9, 0.9, 0.9, 1),
      "button": vec4(0.9, 0.9, 0.95, 1),
      "status_ok": vec4(0.4, 0.9, 0.4, 1),
      "status_err": vec4(0.9, 0.3, 0.3, 1),
      "status_warn": vec4(0.9, 0.9, 0.3, 1),
      "cdn_hdr": vec4(0.5, 0.7, 0.9, 1),
    }
    for name, color in text_colors.items():
      tp = lev2.ui.TextPrimitive(font=self.canvas_font, color=color)
      self._texts[name] = tp
      self.canvas_layer.addPrimitive(tp)

    self.text_sm = lev2.ui.TextPrimitive(font=self.canvas_font_sm, color=vec4(0.7, 0.7, 0.7, 1))
    self.canvas_layer.addPrimitive(self.text_sm)

    # Cache font metrics for text hit-testing (monospace: width = advance * len)
    self._font_advance_w = self.canvas_font.description.advance_width
    self._font_advance_h = self.canvas_font.description.advance_height

  def setClickableRoles(self, *role_names):
    """Mark text roles as clickable (enables per-item hit-testing).
    Call after gpuInit. Only items in clickable roles will respond to clicks.
    Example: detail_view.setClickableRoles("value", "status_ok")
    """
    for name in role_names:
      tp = self._texts.get(name)
      if tp:
        tp.clickable = True

  def isFlashActive(self):
    """Returns True if flash still needs a redraw."""
    return self._flash_frames > 0

  def beginRedraw(self):
    """Clear all prims and reset button registry."""
    self.buttons = {}
    self.bg_prim.clearQuads()
    for tp in self._texts.values():
      tp.clearItems()
    self.text_sm.clearItems()
    # 2-frame flash: frame 2 = draw highlight, frame 1 = normal redraw (clears it)
    if self._flash_frames > 0:
      if self._flash_frames == 2 and self._flash_rect is not None:
        fx, fy, fw, fh = self._flash_rect
        h = max(self.canvas.height, 200)
        qd = lev2.ui.QuadData()
        qd.setPosition(fx - 2, h - fy - fh)
        qd.setSize(fw + 4, fh)
        qd.setColor(vec4(0.3, 0.6, 1.0, 0.35))
        self.bg_prim.addQuad(qd)
      self._flash_frames -= 1
      if self._flash_frames == 0:
        self._flash_rect = None

  def addButton(self, name, x, y, bw, bh, label, ghost=False, danger=False):
    """Draw clickable button quad with hover/danger states.
    ghost=True makes a non-interactive (disabled-looking) button.
    """
    h = max(self.canvas.height, 200)
    if not ghost:
      self.buttons[name] = (x, y, bw, bh)   # Register for hit-testing
    hovered = not ghost and self.hover_button == name
    if ghost:
      color = vec4(0.12, 0.12, 0.14, 1)
    elif danger and hovered:
      color = vec4(0.55, 0.15, 0.15, 1)
    elif danger:
      color = vec4(0.40, 0.10, 0.10, 1)
    elif hovered:
      color = vec4(0.30, 0.35, 0.50, 1)
    else:
      color = vec4(0.20, 0.25, 0.35, 1)
    qd = lev2.ui.QuadData()
    # Flip Y: layout y is top-down, QuadData y is bottom-up (OpenGL)
    qd.setPosition(x, h - y - bh)
    qd.setSize(bw, bh)
    qd.setColor(color)
    self.bg_prim.addQuad(qd)
    txt_role = "label" if ghost else "button"
    self._texts[txt_role].addItem(label, vec2(x + 6, y + 3))

  def drawPropRow(self, label, value, y, val_x=140):
    """Draw label: value text row."""
    self._texts["label"].addItem(f"{label}:", vec2(8, y))
    self._texts["value"].addItem(value, vec2(val_x, y))

  def drawSectionHeader(self, title, y, w, h):
    """Draw a section header with background bar.  h is the canvas height (for Y flip)."""
    qd = lev2.ui.QuadData()
    qd.setPosition(4, h - y - 16)   # Y flip: bottom-up OpenGL coords
    qd.setSize(w - 8, 16)
    qd.setColor(vec4(0.15, 0.15, 0.20, 1))
    self.bg_prim.addQuad(qd)
    self._texts["cdn_hdr"].addItem(title, vec2(8, y))

  def drawProgressBar(self, x, y, w, h_bar, fraction, color, bg_color, canvas_h):
    """Draw a filled progress bar."""
    qd = lev2.ui.QuadData()
    qd.setPosition(x, canvas_h - y - h_bar - 1)
    qd.setSize(w, h_bar)
    qd.setColor(bg_color)
    self.bg_prim.addQuad(qd)
    fill_w = int(w * fraction)
    if fill_w > 0:
      qd2 = lev2.ui.QuadData()
      qd2.setPosition(x, canvas_h - y - h_bar - 1)
      qd2.setSize(fill_w, h_bar)
      qd2.setColor(color)
      self.bg_prim.addQuad(qd2)

  def drawChunkGrid(self, chunks_present, chunks_valid, cdn_list, fqid_active, x, y, cell_w, canvas_h, cells_per_row, label):
    """Draw colored chunk status grid.  Returns new y after the grid.

    Each cell represents one chunk.  Colors indicate status:
      red    — missing / not on CDN
      yellow — present but invalid checksum
      blue   — present, valid, and currently active
      green  — present and valid
    """
    total = len(chunks_present)
    if total == 0:
      return y
    self._texts["label"].addItem(f"{label}:", vec2(8, y))
    for i in range(total):
      row = i // cells_per_row
      col = i % cells_per_row
      cx = x + col * cell_w
      cy = y + row * 16
      qd = lev2.ui.QuadData()
      qd.setPosition(cx, canvas_h - cy - 12)   # Y flip
      qd.setSize(cell_w - 2, 12)
      if label == "CDN":
        if i < len(cdn_list) and cdn_list[i]:
          qd.setColor(vec4(0.2, 0.6, 0.2, 1))   # green: present on CDN
        else:
          qd.setColor(vec4(0.5, 0.15, 0.15, 1))  # red: missing from CDN
      else:
        if not chunks_present[i]:
          qd.setColor(vec4(0.5, 0.15, 0.15, 1))  # red: missing
        elif i < len(chunks_valid) and not chunks_valid[i]:
          qd.setColor(vec4(0.7, 0.7, 0.2, 1))    # yellow: invalid checksum
        elif fqid_active:
          qd.setColor(vec4(0.2, 0.3, 0.7, 1))    # blue: active
        else:
          qd.setColor(vec4(0.2, 0.6, 0.2, 1))    # green: valid
      self.bg_prim.addQuad(qd)
    rows = (total + cells_per_row - 1) // cells_per_row
    return y + rows * 16 + 4

  def drawCdnHealthHeader(self, model, y, w):
    """Draw CDN health status bar. Returns new y."""
    x = 8
    self._texts["cdn_hdr"].addItem("CDN:", vec2(x, y))
    cdn_x0 = 40
    n_cdn = len(model.cdn_health)
    if n_cdn > 0:
      col_w = (w - cdn_x0 - 8) // n_cdn
      for idx, (host, info) in enumerate(model.cdn_health.items()):
        cx = cdn_x0 + idx * col_w
        loc_name = info.get("location", host)
        label = f"{loc_name} ({host})"
        if info.get("reachable"):
          txt = f"{label} {info['latency_ms']:.0f}ms"
          self._texts["status_ok"].addItem(txt, vec2(cx, y))
        else:
          txt = f"{label} offline"
          self._texts["status_err"].addItem(txt, vec2(cx, y))
        ip = info.get("ip", "")
        if ip == "unresolvable":
          self._texts["status_err"].addItem(f"  ip: {ip}", vec2(cx, y + 14))
        elif ip:
          self._texts["label"].addItem(f"  ip: {ip}", vec2(cx, y + 14))
    return y + 30

  def drawTransferStatus(self, model, y):
    """Draw DL or UL status line (uploads take over when active). Returns new y."""
    if model.active_uploads:
      # Aggregate upload stats from all active UploadRequests
      ul_active = len(model.active_uploads)
      ul_bytes_done = 0
      ul_bytes_total = 0
      ul_chunks_done = 0
      ul_chunks_total = 0
      for req in model.active_uploads.values():
        ul_bytes_done += req.bytes_uploaded
        ul_bytes_total += req.bytes_total
        ul_chunks_done += req.chunks_completed
        ul_chunks_total += req.chunks_total
      pending_mb = (ul_bytes_total - ul_bytes_done) / 1048576.0
      total_mb = ul_bytes_total / 1048576.0
      done_mb = ul_bytes_done / 1048576.0
      txt = f"UL: active={ul_active}  {done_mb:.1f}/{total_mb:.1f}MB  pending={pending_mb:.1f}MB  chunks={ul_chunks_done}/{ul_chunks_total}"
      self._texts["status_warn"].addItem(txt, vec2(8, y))
    else:
      dm = model.catalog.download_manager
      if dm:
        active = dm.active_download_count()
        pending = dm.pending_count
        completed = dm.completed_count
        failed = dm.failed_count
        total_mb = dm.total_bytes_downloaded / 1048576.0
        dm_txt = f"DL: active={active}  pending={pending}  completed={completed}  failed={failed}  total={total_mb:.1f}MB"
        self._texts["label"].addItem(dm_txt, vec2(8, y))
    return y + 16

  def _hitTestClickableText(self, mx, my):
    """Hit-test clickable TextPrimitives at (mx, my) in canvas-local coords.
    Returns (text, (x, y, w, h)) of the hit item, or (None, None).
    Only checks TextPrimitives with clickable=True.
    Uses cached font metrics (monospace: width = advance_width * len(text)).
    """
    aw = self._font_advance_w
    ah = self._font_advance_h
    for tp in self._texts.values():
      if not tp.clickable:
        continue
      for i in range(tp.itemCount):
        item = tp.item(i)
        ix = item.position.x
        iy = item.position.y
        iw = aw * len(item.text)
        if ix <= mx < ix + iw and iy <= my < iy + ah:
          return item.text, (ix, iy, iw, ah)
    return None, None

  def handleCanvasEvent(self, canvas, ev):
    """Hit-test buttons and clickable text, track hover.
    Returns the clicked button name on PUSH, "__hover_changed__" if the
    hovered button changed on MOVE, or None otherwise.
    Clickable text clicks are dispatched via on_text_clicked callback.
    """
    clicked = None
    if ev.code == tokens.PUSH.hashed:
      mx, my = canvas.rootToLocal(ev.x, ev.y)
      # Check buttons first
      for name, (bx, by, bw, bh) in self.buttons.items():
        if bx <= mx < bx + bw and by <= my < by + bh:
          clicked = name
          break
      # If no button hit, check clickable text items
      if not clicked:
        hit_text, hit_rect = self._hitTestClickableText(mx, my)
        if hit_text and self.on_text_clicked:
          self.on_text_clicked(hit_text)
          # Start 2-frame flash highlight on the clicked text
          self._flash_rect = hit_rect
          self._flash_frames = 2
          return "__text_copied__"
    elif ev.code == tokens.MOVE.hashed:
      mx, my = canvas.rootToLocal(ev.x, ev.y)
      old_hover = self.hover_button
      self.hover_button = None
      for name, (bx, by, bw, bh) in self.buttons.items():
        if bx <= mx < bx + bw and by <= my < by + bh:
          self.hover_button = name
          break
      # Signal hover change so the caller can trigger a repaint
      if self.hover_button != old_hover:
        return "__hover_changed__"
    return clicked


################################################################################
# ImportConfigEditor
#
# Self-contained overlay editor for import config JSON files.
#
# Widget tree (created in open()):
#   BorderFrame
#     VPack
#       Toolbar          — CLOSE / SAVE / REVERT / AUTOCORRECT / TEST MATCH
#       HPack
#         Outliner       — shows Config + AssetPaks tree
#         PropertySheet  — editable properties for selected node
#
# Data flow:
#   - _editor_data is the in-memory mutable dict loaded from the JSON file.
#   - ImportConfigEditorModel reads/writes _editor_data via its .data property.
#   - PropertySheet displays VarMaps built from _editor_data and writes back
#     through _on_prop_changed.
#   - _editor_dirty tracks unsaved changes.
#
# Custom inline editors:
#   - "Platforms" annotation -> checkbox row (mac / linux)
#   - "FolderBrowse" annotation -> line edit + "..." button that opens a
#     secondary window with a FilesystemBrowser
################################################################################

################################################################################
# TextPromptOverlay — small overlay with a label + line edit for text input
################################################################################

class TextPromptOverlay:
  """Small centered overlay that prompts for a text value.
  Shows a label, a LineEdit, and commits on Enter / cancels on Escape."""

  def __init__(self, uicontext, ezapp, label, default_text="", on_commit=None):
    self.uicontext = uicontext
    self.on_commit = on_commit

    lg = ezapp.topLayoutGroup
    top_w = lg.width
    top_h = lg.height
    ow = min(500, top_w - 40)
    oh = 120
    ox = (top_w - ow) // 2
    oy = (top_h - oh) // 2

    # Just push a LineEdit directly as the overlay
    lineedit = uicontext.createOverlayWidget(
      lev2.ui.LineEdit, ["prompt_input", default_text, vec3(0.15, 0.15, 0.2)])
    lineedit.highlight = True

    prompt = self

    def on_text_committed(text):
      uicontext.popOverlay()
      if prompt.on_commit and text.strip():
        prompt.on_commit(text.strip())

    def on_cancel():
      uicontext.popOverlay()

    lineedit.onTextCommitted(on_text_committed)
    lineedit.onCancel(on_cancel)

    uicontext.pushOverlay(lineedit, ox, oy, ow, oh, dismiss_on_click_outside=True)


################################################################################
# ImportConfigEditor
################################################################################

class ImportConfigEditor:
  """Self-contained overlay editor for import config JSON files."""

  def __init__(self, uicontext, model, ezapp):
    self.model = model              # CatalogModel (shared state)
    self.uicontext = uicontext      # UI context for overlay management
    self.ezapp = ezapp              # EzApp for window dimensions / secondary windows
    self.on_close = None            # Optional callback when editor closes
    self.on_save = None             # Optional callback after save
    self._editor_ic_name = None     # Name of the import config being edited
    self._editor_ic_path = None     # Filesystem path to the JSON file
    self._editor_data = None        # Mutable dict — the live working copy
    self.__editor_dirty = False     # True if unsaved edits exist (use property)
    self._editor_selected_key = None  # Currently selected outliner key
    self._btn_close = None
    self._btn_save = None

  @property
  def _editor_dirty(self):
    return self.__editor_dirty

  @_editor_dirty.setter
  def _editor_dirty(self, val):
    self.__editor_dirty = val
    self._update_dirty_buttons()

  def _update_dirty_buttons(self):
    """Update close/save button colors based on dirty state."""
    if not self._btn_close or not self._btn_save:
      return
    if self.__editor_dirty:
      self._btn_close.color_override = vec4(0.5, 0.12, 0.12, 1)  # red = unsaved changes
      self._btn_save.color_override = vec4(0.12, 0.4, 0.12, 1)   # green = save available
    else:
      self._btn_close.color_override = vec4(0, 0, 0, 0)  # transparent = default
      self._btn_save.color_override = vec4(0, 0, 0, 0)

  def open(self, ic_name):
    """Open editor overlay for named import config."""
    path = self.model.import_config_paths.get(ic_name)
    if not path:
      return
    try:
      with open(path, 'r') as f:
        data = json.load(f)
    except Exception:
      return

    self._editor_ic_name = ic_name
    self._editor_ic_path = path
    self._editor_data = data
    self._editor_dirty = False
    self._editor_selected_key = None

    lg = self.ezapp.topLayoutGroup
    top_w = lg.width
    top_h = lg.height
    margin = int(min(top_w, top_h) * 0.05)

    # Build widget tree: BorderFrame > VPack > (Toolbar, HPack > (Outliner, PropertySheet))
    self._editor_frame = self.uicontext.createOverlayWidget(
      lev2.ui.BorderFrame, ["ic_editor_frame"])
    self._editor_frame.border_width = 12
    self._editor_frame.border_edge_width = 3
    self._editor_frame.border_color = vec4(0, 0, 0, 1)
    self._editor_frame.border_outer_color = vec4(0, 0, 0, 1)
    self._editor_frame.border_inner_color = vec4(1, 1, 0, 1)

    self._editor_vpack = lev2.ui.VerticalPack.wfactory(["ic_editor_vpack"])
    self._editor_frame.child = self._editor_vpack
    self._editor_vpack.margin = 4
    self._editor_vpack.item_height = 36   # Fixed height for the toolbar row

    # Toolbar
    self._editor_toolbar = self._editor_vpack.makeChild(
      uiclass=lev2.ui.Toolbar, args=["ic_editor_toolbar"])
    self._editor_toolbar.bgcolor = vec4(0.15, 0.15, 0.18, 1)
    self._editor_toolbar.button_color = vec4(0.20, 0.20, 0.25, 1)
    self._editor_toolbar.button_hover_color = vec4(0.28, 0.28, 0.35, 1)
    self._editor_toolbar.button_pressed_color = vec4(0.25, 0.45, 0.65, 1)
    self._editor_toolbar.button_border_color = vec4(0.35, 0.35, 0.42, 1)
    self._editor_toolbar.button_border_width = 1
    self._editor_toolbar.separator_color = vec4(0.30, 0.30, 0.35, 1)
    self._editor_toolbar.icon_size = 28
    self._editor_toolbar.button_padding = 4
    self._editor_toolbar.label_padding = 4
    self._editor_toolbar.item_spacing = 8
    self._editor_toolbar.edge_padding = 8

    self._btn_close = self._editor_toolbar.addTextButton("ed_close", "CLOSE")
    self._btn_close.custom_width = 56
    self._btn_close.onPressed(lambda: self.close())

    self._btn_save = self._editor_toolbar.addTextButton("ed_save", "SAVE")
    self._btn_save.custom_width = 48
    self._btn_save.onPressed(lambda: self.save())

    btn_revert = self._editor_toolbar.addTextButton("ed_revert", "REVERT")
    btn_revert.custom_width = 64
    btn_revert.onPressed(lambda: self.revert())

    btn_autocorrect = self._editor_toolbar.addTextButton("ed_autocorrect", "AUTOCORRECT")
    btn_autocorrect.custom_width = 96
    btn_autocorrect.onPressed(lambda: self.autocorrect())

    btn_test = self._editor_toolbar.addTextButton("ed_testmatch", "TEST MATCH")
    btn_test.custom_width = 88
    btn_test.onPressed(lambda: self.test_match())

    btn_browse = self._editor_toolbar.addTextButton("ed_browseloc", "BROWSELOC")
    btn_browse.custom_width = 88
    btn_browse.onPressed(lambda: self.browse_local_loc())

    btn_preset = self._editor_toolbar.addTextButton("ed_preset_byext", "PRESET:BYEXT")
    btn_preset.custom_width = 100
    btn_preset.onPressed(lambda: self.preset_by_ext())

    btn_delete = self._editor_toolbar.addTextButton("ed_delete", "DELETE")
    btn_delete.custom_width = 56
    btn_delete.color_override = vec4(0.4, 0.1, 0.1, 1)
    btn_delete.onPressed(lambda: self.delete_config())

    # HPack for outliner + propsheet (fills remaining vertical space)
    self._editor_hpack = self._editor_vpack.makeChild(
      uiclass=lev2.ui.HorizontalPack, args=["ic_editor_hpack"])
    self._editor_hpack.margin = 2
    self._editor_hpack.item_width = int((top_w - 2 * margin) * 0.30)  # Outliner gets 30% width
    self._editor_vpack.fill_widget = self._editor_hpack

    # Outliner
    self._editor_outliner = self._editor_hpack.makeChild(
      uiclass=lev2.ui.Outliner, args=["ic_editor_outliner"])
    self._editor_outliner.bgcolor = vec4(0.10, 0.10, 0.12, 1)
    self._editor_outliner.item_height = 22

    # Wire up the outliner model (shares _editor_data through self)
    self._editor_outliner_model = ImportConfigEditorModel(self)
    self._editor_outliner.model = self._editor_outliner_model

    self._editor_outliner.onSelect(self._on_select)
    self._editor_outliner.onRename(self._on_rename)
    self._editor_outliner.onDelete(self._on_delete)
    self._editor_outliner.onAdd(self._on_add)

    # PropertySheet (fills remaining horizontal space)
    self._editor_propsheet = self._editor_hpack.makeChild(
      uiclass=lev2.ui.PropertySheet, args=["ic_editor_propsheet"])
    self._editor_propsheet.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    self._editor_propsheet.label_color = vec4(0.9, 0.9, 0.9, 1)
    self._editor_propsheet.group_color = vec4(0.18, 0.18, 0.22, 1)
    self._editor_propsheet.row_height = 28
    self._editor_propsheet.label_width = 140
    self._editor_hpack.fill_widget = self._editor_propsheet

    self._editor_propsheet.onPropertyChanged(self._on_prop_changed)

    # Register custom inline editors for specific annotation types.
    # When a property has a matching annotation, the PropertySheet calls
    # the factory instead of using the default text editor.
    self._editor_propsheet.registerEditorFactory(
      tokens.Platforms,
      self._create_platforms_inline_editor)
    self._editor_propsheet.registerEditorFactory(
      tokens.FolderBrowse,
      self._create_folder_browse_inline_editor)
    self._editor_propsheet.registerEditorFactory(
      tokens.NamespaceSelect,
      self._create_namespace_inline_editor)

    self.uicontext.pushOverlay(
      self._editor_frame,
      margin, margin,
      top_w - 2 * margin, top_h - 2 * margin,
      dismiss_on_click_outside=False)

    self._editor_outliner.expandAll()

  def close(self):
    """Close editor overlay."""
    self.uicontext.popOverlay()
    self.model.import_config_data.pop(self._editor_ic_name, None)
    if self.on_close:
      self.on_close()

  def save(self):
    """Save in-memory dict to JSON file.
    Only writes fields that differ from defaults (omits default platforms, zero priority, etc.)
    """
    data = self._editor_data
    # Build a clean output dict with only non-default fields
    save_data = {}
    for k in ["namespace", "source_dir", "local_loc", "manifest"]:
      if k in data:
        save_data[k] = data[k]
    # Grab unresolved encryption_key from namespace config (config.json)
    ns = data.get("namespace", "")
    ns_enc_key = self.model.get_namespace_encryption_key_raw(ns) if ns else None
    if ns_enc_key:
      save_data["encryption_key"] = ns_enc_key
    elif data.get("encryption_key"):
      save_data["encryption_key"] = data["encryption_key"]
    else:
      print(f"WARNING: No encryption key set for namespace '{ns}' — import may fail")
    if data.get("platforms") and data["platforms"] != ["mac", "linux"]:
      save_data["platforms"] = data["platforms"]
    if data.get("priority", 0) != 0:
      save_data["priority"] = data["priority"]
    if data.get("assets"):
      save_data["assets"] = []
      for a in data["assets"]:
        ad = {"id": a["id"], "include": a["include"]}
        if a.get("exclude"):
          ad["exclude"] = a["exclude"]
        save_data["assets"].append(ad)

    with open(self._editor_ic_path, 'w') as f:
      json.dump(save_data, f, indent=2)
      f.write('\n')
    self._editor_dirty = False
    self.model.import_config_data.pop(self._editor_ic_name, None)
    if self.on_save:
      self.on_save()

  def revert(self):
    """Reload from disk, discard edits."""
    try:
      with open(self._editor_ic_path, 'r') as f:
        self._editor_data = json.load(f)
    except Exception:
      return
    self._editor_dirty = False
    self._editor_outliner_model.notifyModelReset()
    self._editor_outliner.expandAll()
    if self._editor_selected_key:
      self._on_select(self._editor_selected_key)

  def autocorrect(self):
    """Run path sanitization on source_dir and manifest."""
    data = self._editor_data
    changed = False
    manifest_dir = os.path.dirname(self._editor_ic_path)
    for key in ["source_dir", "manifest"]:
      val = data.get(key, "")
      if not val or not os.path.isabs(val):
        continue
      if os.path.exists(val):
        new_val = sanitize_path(val)
      else:
        new_val = resolve_bad_path(val, manifest_dir)
      if new_val != val:
        data[key] = new_val
        changed = True
    if changed:
      self._editor_dirty = True
      if self._editor_selected_key == "Config":
        self._on_select("Config")

  def test_match(self):
    """Open test match overlay showing file enumeration results."""
    data = self._editor_data
    try:
      results, source_dir = self.model.list_import_assets(data)
    except Exception as e:
      results = []
      source_dir = ""

    lg = self.ezapp.topLayoutGroup
    top_w = lg.width
    top_h = lg.height
    margin = int(min(top_w, top_h) * 0.08)

    self._match_frame = self.uicontext.createOverlayWidget(
      lev2.ui.BorderFrame, ["match_frame"])
    self._match_frame.border_width = 12
    self._match_frame.border_edge_width = 3
    self._match_frame.border_color = vec4(0, 0, 0, 1)
    self._match_frame.border_outer_color = vec4(0, 0, 0, 1)
    self._match_frame.border_inner_color = vec4(1, 1, 0, 1)

    self._match_vpack = lev2.ui.VerticalPack.wfactory(["match_vpack"])
    self._match_frame.child = self._match_vpack
    self._match_vpack.margin = 4
    self._match_vpack.item_height = 36

    self._match_toolbar = self._match_vpack.makeChild(
      uiclass=lev2.ui.Toolbar, args=["match_toolbar"])
    self._match_toolbar.bgcolor = vec4(0.15, 0.15, 0.18, 1)
    self._match_toolbar.button_color = vec4(0.20, 0.20, 0.25, 1)
    self._match_toolbar.button_hover_color = vec4(0.28, 0.28, 0.35, 1)
    self._match_toolbar.button_pressed_color = vec4(0.25, 0.45, 0.65, 1)
    self._match_toolbar.button_border_color = vec4(0.35, 0.35, 0.42, 1)
    self._match_toolbar.button_border_width = 1
    self._match_toolbar.separator_color = vec4(0.30, 0.30, 0.35, 1)
    self._match_toolbar.icon_size = 28
    self._match_toolbar.button_padding = 4
    self._match_toolbar.label_padding = 4
    self._match_toolbar.item_spacing = 8
    self._match_toolbar.edge_padding = 8

    btn_close = self._match_toolbar.addTextButton("match_close", "CLOSE")
    btn_close.custom_width = 56
    btn_close.onPressed(lambda: self.uicontext.popOverlay())

    total_files = sum(len(files) for _, files in results)
    total_paks = len(results)
    btn_summary = self._match_toolbar.addTextButton("match_summary",
      f"{total_paks} paks, {total_files} files matched")
    btn_summary.custom_width = 200

    self._match_outliner = self._match_vpack.makeChild(
      uiclass=lev2.ui.Outliner, args=["match_outliner"])
    self._match_outliner.bgcolor = vec4(0.10, 0.10, 0.12, 1)
    self._match_outliner.item_height = 20
    self._match_vpack.fill_widget = self._match_outliner

    self._match_outliner_model = TestMatchOutlinerModel(results, source_dir)
    self._match_outliner.model = self._match_outliner_model

    self.uicontext.pushOverlay(
      self._match_frame,
      margin, margin,
      top_w - 2 * margin, top_h - 2 * margin,
      dismiss_on_click_outside=True)
    self._match_outliner.expandAll()

  # ── Internal ──

  def _build_config_varmap(self):
    VarMap = core.VarMap
    data = self._editor_data
    vm = VarMap()
    vm.namespace = data.get("namespace", "")
    vm.source_dir = data.get("source_dir", "")
    vm.local_loc = data.get("local_loc", "")
    vm.manifest = data.get("manifest", "")
    platforms = data.get("platforms", ["mac", "linux"])
    vm.platforms = ", ".join(platforms) if isinstance(platforms, list) else str(platforms)
    return vm

  def _build_pak_varmap(self, pak_id):
    VarMap = core.VarMap
    assets = self._editor_data.get("assets", [])
    pak = None
    for a in assets:
      if a["id"] == pak_id:
        pak = a
        break
    if not pak:
      return VarMap()
    vm = VarMap()
    vm.id = pak["id"]
    inc = pak.get("include", "")
    if isinstance(inc, list):
      vm.include = ", ".join(inc)
    else:
      vm.include = str(inc)
    exc = pak.get("exclude", [])
    vm.exclude = ", ".join(exc) if isinstance(exc, list) else str(exc)
    return vm

  def _on_select(self, key):
    """Handle outliner selection — populate the PropertySheet for the selected node."""
    self._editor_selected_key = key
    if key == "Config":
      self._editor_propsheet.data = self._build_config_varmap()
      # Attach annotations so the PropertySheet uses custom inline editors
      model = self._editor_propsheet.model
      plat_annot = core.VarMap()
      plat_annot.type = tokens.Platforms
      model.setAnnotations("platforms", plat_annot)
      for fkey in ("source_dir", "local_loc", "manifest"):
        fb_annot = core.VarMap()
        fb_annot.type = tokens.FolderBrowse
        model.setAnnotations(fkey, fb_annot)
      ns_annot = core.VarMap()
      ns_annot.type = tokens.NamespaceSelect
      model.setAnnotations("namespace", ns_annot)
    elif key.startswith("AssetPaks/"):
      pak_id = key.split("/", 1)[1]
      self._editor_propsheet.data = self._build_pak_varmap(pak_id)
    else:
      self._editor_propsheet.data = None
    self._editor_propsheet.expandAll()

  def _on_rename(self, old_key, new_name):
    self._editor_outliner_model.renameItem(old_key, new_name)

  def _on_delete(self, key):
    self._editor_outliner_model.removeItem(key)
    if self._editor_selected_key == key:
      self._editor_propsheet.data = None
      self._editor_selected_key = None

  def _on_add(self, key):
    pass

  def _on_prop_changed(self, key, value):
    """Write property edits back into _editor_data."""
    sel = self._editor_selected_key
    if not sel:
      return
    self._editor_dirty = True
    data = self._editor_data

    if sel == "Config":
      # Property keys may be prefixed with group path — extract the field name
      field = key.split("/")[-1] if "/" in key else key
      if field == "platforms":
        # Comma-separated string -> list
        data["platforms"] = [s.strip() for s in str(value).split(",") if s.strip()]
      elif field == "encryption_key":
        if str(value).strip():
          data["encryption_key"] = str(value)
        else:
          data.pop("encryption_key", None)  # Remove empty key entirely
      else:
        data[field] = str(value)
    elif sel.startswith("AssetPaks/"):
      pak_id = sel.split("/", 1)[1]
      assets = data.get("assets", [])
      for a in assets:
        if a["id"] == pak_id:
          field = key.split("/")[-1] if "/" in key else key
          if field == "id":
            # Renaming via property sheet — update key and refresh outliner
            new_id = str(value)
            a["id"] = new_id
            self._editor_selected_key = f"AssetPaks/{new_id}"
            self._editor_outliner_model.notifyModelReset()
            self._editor_outliner.expandAll()
          elif field == "include":
            val = str(value)
            parts = [s.strip() for s in val.split(",") if s.strip()]
            # Single pattern stored as string, multiple as list
            a["include"] = parts[0] if len(parts) == 1 else parts
          elif field == "exclude":
            val = str(value)
            parts = [s.strip() for s in val.split(",") if s.strip()]
            if parts:
              a["exclude"] = parts
            else:
              a.pop("exclude", None)  # Remove empty exclude
          break

  def _create_folder_browse_inline_editor(self, sheet, key, value, annotations):
    """Custom inline editor: text field + "..." browse button.
    The browse button opens a secondary window with a FilesystemBrowser.
    """
    hpack = lev2.ui.HorizontalPack.wfactory(["fb_hpack_" + key])
    hpack.item_width = 28    # Fixed width for the browse button
    current = str(value) if value else ""
    lineedit = hpack.makeChild(
      uiclass=lev2.ui.LineEdit,
      args=["", current, vec3(0.15, 0.15, 0.18)])
    hpack.fill_widget = lineedit  # Text field fills remaining space
    field_key = key
    editor = self

    def on_text_committed(text):
      # Sanitize absolute paths on commit
      if text and os.path.isabs(text):
        text = sanitize_path(text)
        text = text.replace("${ASSETCACHE}", "<assetcache>")
      editor._editor_data[field_key] = text
      editor._editor_dirty = True

    lineedit.onTextCommitted(on_text_committed)

    btn = hpack.makeChild(
      uiclass=lev2.ui.Toolbar, args=["fb_btn_" + key])
    btn.bgcolor = vec4(0.20, 0.25, 0.35, 1)
    btn.button_hover_color = vec4(0.30, 0.35, 0.50, 1)
    btn.button_pressed_color = vec4(0.25, 0.45, 0.65, 1)
    btn.icon_size = 14
    btn.button_padding = 2
    btn.item_spacing = 0
    btn.edge_padding = 2
    browse_btn = btn.addTextButton("browse_" + key, "...")
    browse_btn.custom_width = 24
    browse_btn.onPressed(lambda: editor._browse_folder(field_key))
    return hpack

  def _create_platforms_inline_editor(self, sheet, key, value, annotations):
    """Custom inline editor: checkbox row for platform selection (mac / linux)."""
    hpack = lev2.ui.HorizontalPack.wfactory(["platforms_hpack"])
    hpack.item_width = 80
    chk_color = vec3(0.7, 0.7, 0.8)
    platforms = self._editor_data.get("platforms", ["mac", "linux"])

    chk_mac = hpack.makeChild(
      uiclass=lev2.ui.Checkbox, args=["mac", chk_color])
    chk_mac.toggled = "mac" in platforms
    chk_mac.bg_color = vec3(0, 0, 0)

    chk_linux = hpack.makeChild(
      uiclass=lev2.ui.Checkbox, args=["linux", chk_color])
    chk_linux.toggled = "linux" in platforms
    chk_linux.bg_color = vec3(0, 0, 0)

    editor = self
    def on_toggled(chk):
      plats = []
      if chk_mac.toggled:
        plats.append("mac")
      if chk_linux.toggled:
        plats.append("linux")
      editor._editor_data["platforms"] = plats if plats else ["mac", "linux"]
      editor._editor_dirty = True

    chk_mac.onToggled = on_toggled
    chk_linux.onToggled = on_toggled
    return hpack

  def _create_enc_key_inline_editor(self, sheet, key, value, annotations):
    """Custom inline editor: ChoicelistWidget dropdown of env vars matching *ENC_KEY*."""
    current = str(value) if value else ""
    # Scan environment for vars containing ENC_KEY
    enc_vars = sorted(v for v in os.environ if "ENC_KEY" in v)
    choices = [f"${{{v}}}" for v in enc_vars]
    if current and current not in choices:
      choices.insert(0, current)  # keep current value visible even if not in env

    widget = lev2.ui.ChoicelistWidget("ek_" + key, current if current else "(none)")
    widget.setChoices(choices)
    widget.bg_color = vec4(0.15, 0.15, 0.2, 1)

    editor = self
    def on_selected(selected):
      editor._editor_data["encryption_key"] = selected
      editor._editor_dirty = True
      widget.current_value = selected

    widget.onChoiceSelected = on_selected
    return widget

  def _create_namespace_inline_editor(self, sheet, key, value, annotations):
    """Custom inline editor: ChoicelistWidget dropdown of project's namespaces."""
    from ork.catalog_tool import derive_project_name
    current = str(value) if value else ""
    # Derive project from the import config's path
    path = self._editor_ic_path or ""
    project = derive_project_name(path) if path else ""
    ns_list = sorted(self.model.project_namespaces.get(project, []))
    if current and current not in ns_list:
      ns_list.insert(0, current)

    widget = lev2.ui.ChoicelistWidget("ns_" + key, current if current else "(none)")
    widget.setChoices(ns_list)
    widget.bg_color = vec4(0.15, 0.15, 0.2, 1)

    editor = self
    def on_selected(selected):
      editor._editor_data["namespace"] = selected
      editor._editor_dirty = True
      widget.current_value = selected

    widget.onChoiceSelected = on_selected
    return widget

  def _browse_folder(self, field_key):
    """Open a secondary window with a FilesystemBrowser for folder selection.
    The selected path is sanitized and written back to _editor_data.
    """
    from ork.ui.filesystem_browser import FilesystemBrowser
    from obt import path as obt_path

    assetcache = str(obt_path.stage() / "assetcache")
    current_val = self._editor_data.get(field_key, "")
    if current_val:
      from ork.catalog_import import resolve_variables
      try:
        resolved = resolve_variables(current_val)
        if os.path.isdir(resolved):
          initial_path = resolved
        else:
          initial_path = assetcache
      except Exception:
        initial_path = assetcache
    else:
      initial_path = assetcache

    title = f"Browse: {field_key}"
    popup = self.ezapp.createSecondaryWindow(
      width=800, height=600, x=200, y=150,
      title=title, decorated=True, resizable=True, floating=True)
    uic = popup.ui_context
    root = lev2.ui.LayoutGroup.create("popup_lg")
    root.setRect(0, 0, popup.width, popup.height)
    uic.top = root
    root.margin = 4

    browser_item = root.makeChild(
      uiclass=FilesystemBrowser,
      args=["browser", initial_path, "", vec3(0.1, 0.1, 0.1), "select"],
      fill=True)
    browser = browser_item.widget.uservars.filesystem_browser

    # manifest field accepts files; source_dir/local_loc accept only directories
    if field_key != "manifest":
      browser.model.directories_only = True
    editor = self

    def on_activate(path):
      if os.path.isdir(path) or (field_key == "manifest" and os.path.isfile(path)):
        sanitized = sanitize_path(path)
        sanitized = sanitized.replace("${ASSETCACHE}", "<assetcache>")
        editor._editor_data[field_key] = sanitized
        editor._editor_dirty = True
        if editor._editor_selected_key == "Config":
          editor._on_select("Config")
      popup.requestClose()

    browser.onActivate = on_activate
    browser.onCancel = lambda: popup.requestClose()

  def browse_local_loc(self):
    """Open a read-only secondary window browser at the local_loc path."""
    from ork.ui.filesystem_browser import FilesystemBrowser
    from obt import path as obt_path

    local_loc = self._editor_data.get("local_loc", "")
    if local_loc:
      from ork.catalog_import import resolve_variables
      try:
        initial_path = resolve_variables(local_loc)
        if not os.path.isdir(initial_path):
          initial_path = str(obt_path.stage() / "assetcache")
      except Exception:
        initial_path = str(obt_path.stage() / "assetcache")
    else:
      initial_path = str(obt_path.stage() / "assetcache")

    popup = self.ezapp.createSecondaryWindow(
      width=800, height=600, x=200, y=150,
      title=f"Browse: {local_loc}", decorated=True, resizable=True, floating=True)
    uic = popup.ui_context
    root = lev2.ui.LayoutGroup.create("popup_lg")
    root.setRect(0, 0, popup.width, popup.height)
    uic.top = root
    root.margin = 4

    browser_item = root.makeChild(
      uiclass=FilesystemBrowser,
      args=["browser", initial_path, "", vec3(0.1, 0.1, 0.1), "select"],
      fill=True)
    browser = browser_item.widget.uservars.filesystem_browser

    browser.onActivate = lambda path: popup.requestClose()
    browser.onCancel = lambda: popup.requestClose()

  def preset_by_ext(self):
    """Scan source_dir for file extensions, create one AssetPak per extension."""
    from ork.catalog_import import resolve_variables
    source_dir = self._editor_data.get("source_dir", "")
    if not source_dir:
      return
    try:
      resolved = resolve_variables(source_dir)
    except Exception:
      return
    if not os.path.isdir(resolved):
      return
    # Walk directory, collect unique extensions
    extensions = set()
    for root, dirs, files in os.walk(resolved):
      for f in files:
        ext = os.path.splitext(f)[1].lstrip(".")
        if ext:
          extensions.add(ext)
    if not extensions:
      return
    # Replace assets list with one pak per extension (recursive glob)
    self._editor_data["assets"] = [
      {"id": ext, "include": f"**/*.{ext}"}
      for ext in sorted(extensions)
    ]
    self._editor_dirty = True
    self._editor_outliner_model.notifyModelReset()
    self._editor_outliner.expandAll()
    # Refresh propsheet if an asset pak was selected
    if self._editor_selected_key and self._editor_selected_key.startswith("AssetPaks/"):
      self._editor_propsheet.data = None
      self._editor_selected_key = None

  def delete_config(self):
    """Delete the import config file from disk, close editor, rescan."""
    path = self._editor_ic_path
    if not path or not os.path.exists(path):
      return
    os.remove(path)
    self.model.import_config_data.pop(self._editor_ic_name, None)
    self.model.import_config_paths.pop(self._editor_ic_name, None)
    self.uicontext.popOverlay()
    # Rescan so the deleted config disappears from the UI
    from orkengine import core
    core.AssetCatalog.reloadAllManifests(self.model.catalog)
    self.model.scan()
    if self.on_close:
      self.on_close()
