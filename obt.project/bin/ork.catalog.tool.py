#!/usr/bin/env ork.python
################################################################################
# Orkid Asset Catalog GUI Tool — View / Application Layer
#
# This is the slim app/view layer of an MVC-refactored catalog tool.
# It wires the CatalogModel (business logic, in ork.catalog_tool) to a
# PrimCanvas-based UI via CanvasDetailView (drawing helpers, in
# ork.catalog_tool_ui).
#
# Architecture overview:
#   Model  — CatalogModel:       manifests, assets, CDN ops, import configs
#   View   — CanvasDetailView:   text layers, buttons, progress bars, grids
#   App    — CatalogTool (this): widget tree, event routing, redraw dispatch
#
# Widget tree layout:
#   TopLayoutGroup
#     ├─ left dock  (30%) — toolbar (REFRESH) + Outliner (project/ns/asset tree)
#     └─ right dock (70%) — PrimCanvas detail view (properties, chunk grids, etc.)
#
# Data flow:
#   model.on_dirty        → sets canvas_dirty flag → redrawn next _onUpdate
#   model.on_scan_complete→ resets outliner tree data
#   outliner selection     → _on_select → triggers CDN verify, marks dirty
#   canvas UI event        → detail_view.handleCanvasEvent → button name
#                          → _on_button dispatch table
################################################################################

import os, time, json
from orkengine import core
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.catalog_tool import (
  CatalogModel, format_size,
  build_namespace_varmap, build_asset_varmap,
)
from ork.catalog_tool_ui import (
  CatalogOutlinerModel, CanvasDetailView, ImportConfigEditor, TextPromptOverlay,
)

tokens = core.CrcStringProxy()
vec2 = core.vec2
vec3 = core.vec3
vec4 = core.vec4

################################################################################
# Main application
################################################################################

class CatalogTool(ComponentizedApplication):

  def __init__(self):
    super().__init__(profiler_channels=[])
    # MVC wiring: model callback marks canvas dirty so next frame redraws
    self.model = CatalogModel()
    self.model.on_dirty = self._on_model_dirty
    self.selected_key = None                # outliner key currently selected
    self.selected_import_config = None       # when set, detail view shows IC overlay
    self.cdn_validate_mode = False           # when True, detail view shows CDN validation
    self._selecting = False                  # reentrancy guard for _on_select
    self.canvas_dirty = True
    self.canvas_ready = False                # set True after GPU init
    self.detail_view = None                  # CanvasDetailView, created in _onGpuInit
    self.ic_editor = None                    # ImportConfigEditor overlay, created on demand
    self.createEzApp(width=1200, height=800, fullscreen=False, name="Asset Catalog Tool")

  def _on_model_dirty(self):
    self.canvas_dirty = True

  def _start_cdn_validation(self):
    """Enter CDN validation mode: verify all locally-present assets against CDN."""
    self.cdn_validate_mode = True
    self.canvas_dirty = True
    self.model._bg(self.model.validate_cdn_all)

  ############################################################################
  # UI init — builds the widget tree and wires model callbacks to UI
  ############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.12, 0.12, 0.14, 1)

    # --- Right dock: PrimCanvas detail view (created first, then split) ---
    right_dock_item = lg.makeChild(
      fill=True, margin=2,
      uiclass=lev2.ui.DockablePanel, args=["details_dock"])
    self.right_dock = right_dock_item.widget
    self.right_dock.titlebar_color = vec4(0.18, 0.18, 0.22, 1)
    self.right_dock.title_color = vec4(0.7, 0.7, 0.8, 1)
    self.right_dock.title_override = "Details"
    self.right_dock.title_center = True

    self.canvas = self.right_dock.createChild(
      uiclass=lev2.ui.PrimCanvas, args=["detail_canvas"])
    self.canvas.supersample = 0
    self.canvas.bg_color = vec4(0.08, 0.08, 0.10, 1)
    self.canvas.draw_background = True
    self.canvas.onUiEvent = self._on_canvas_event  # route canvas clicks/hovers

    # --- Left dock: toolbar + outliner (split off 30% of layout) ---
    left_dock_item = lg.split(
      layout=right_dock_item.layout,
      proportion=0.30, placement=tokens.LEFT, margin=2,
      uiclass=lev2.ui.DockablePanel, args=["catalog_dock"])
    self.left_dock = left_dock_item.widget
    self.left_dock.titlebar_color = vec4(0.18, 0.15, 0.20, 1)
    self.left_dock.title_color = vec4(0.8, 0.7, 0.9, 1)
    self.left_dock.title_override = "Catalog"
    self.left_dock.title_center = True

    self.left_vpack = self.left_dock.createChild(
      uiclass=lev2.ui.VerticalPack, args=["left_vpack"])
    self.left_vpack.margin = 2
    self.left_vpack.item_height = 36

    # Toolbar
    self.toolbar = self.left_vpack.makeChild(
      uiclass=lev2.ui.Toolbar, args=["toolbar"])
    self.toolbar.bgcolor = vec4(0.15, 0.15, 0.18, 1)
    self.toolbar.button_color = vec4(0.20, 0.20, 0.25, 1)
    self.toolbar.button_hover_color = vec4(0.28, 0.28, 0.35, 1)
    self.toolbar.button_pressed_color = vec4(0.25, 0.45, 0.65, 1)
    self.toolbar.button_border_color = vec4(0.35, 0.35, 0.42, 1)
    self.toolbar.button_border_width = 1
    self.toolbar.separator_color = vec4(0.30, 0.30, 0.35, 1)
    self.toolbar.icon_size = 28
    self.toolbar.button_padding = 4   # 4px vertical margin around text
    self.toolbar.label_padding = 4    # 4px horizontal margin around text
    self.toolbar.item_spacing = 8     # 8px gap between buttons
    self.toolbar.edge_padding = 8

    btn_refresh = self.toolbar.addTextButton("refresh", "REFRESH")
    btn_refresh.custom_width = 72
    btn_refresh.onPressed(lambda: self.model._bg(self.model.refresh))

    btn_validate = self.toolbar.addTextButton("validate_cdn", "VALIDATE CDN")
    btn_validate.custom_width = 100
    btn_validate.onPressed(lambda: self._start_cdn_validation())

    # Outliner
    self.outliner = self.left_vpack.makeChild(
      uiclass=lev2.ui.Outliner, args=["outliner"])
    self.outliner.bgcolor = vec4(0.10, 0.10, 0.12, 1)
    self.outliner.item_height = 22
    self.left_vpack.fill_widget = self.outliner

    self.outliner_model = CatalogOutlinerModel(self.model)
    self.outliner.model = self.outliner_model
    self.outliner.onSelect(self._on_select)

    # MVC wiring: when model finishes scanning manifests, reset the outliner
    # tree so it reflects the new data. Chains with any existing callback.
    orig_on_scan = self.model.on_scan_complete
    def on_scan():
      self.outliner_model.notifyModelReset()
      if orig_on_scan:
        orig_on_scan()
    self.model.on_scan_complete = on_scan

  ############################################################################
  # GPU init — create CanvasDetailView and kick off initial model scan
  ############################################################################

  def _onGpuInit(self, ctx):
    self.uicontext = self.ezapp.uicontext
    base_db = lev2.ui.createDefaultStyleDatabase()
    custom_db = lev2.ui.StyleDatabase.createChild(base_db)
    self.uicontext.theme_engine = lev2.ui.ThemeEngine(custom_db)

    # CanvasDetailView owns the text layers, buttons, and drawing helpers
    self.detail_view = CanvasDetailView(self.canvas)
    self.detail_view.gpuInit(ctx)

    # Enable click-to-copy on value text items
    self.detail_view.setClickableRoles("value")
    self._gfx_ctx = ctx  # stash for clipboard access
    self.detail_view.on_text_clicked = self._on_text_clicked

    self.canvas_ready = True

    self.model.init()  # triggers background manifest scan

  ############################################################################
  # Selection — outliner callback, triggers CDN verification lazily
  ############################################################################

  def _on_select(self, key):
    if self._selecting:  # reentrancy guard
      return
    self._selecting = True
    self.selected_key = key
    self.canvas_dirty = True
    self._selecting = False
    # Lazily verify CDN status when an asset or namespace is first selected
    m = self.model
    kt = m.key_type(key)
    if kt == "asset":
      fqid = m.key_to_fqid(key)
      if fqid and fqid not in m.cdn_status:
        m._bg(lambda: m.verify_cdn(fqid))
    elif kt == "namespace":
      ns = m.key_to_ns(key)
      if ns:
        unchecked = [f for f in m.ns_assets.get(ns, []) if f not in m.cdn_status]
        if unchecked:
          m._bg(lambda fqids=unchecked: m.verify_cdn_batch(fqids))

  def _on_text_clicked(self, text):
    """Copy clicked text value to clipboard via GLFW."""
    self._gfx_ctx.setClipboardText(text)

  ############################################################################
  # Canvas event handling
  # Flow: canvas event → detail_view.handleCanvasEvent (hit-test buttons)
  #       → returns button name string → _on_button dispatch
  ############################################################################

  def _on_canvas_event(self, ev):
    # detail_view does hit-testing; returns button name, "__hover_changed__",
    # "__text_copied__", or None
    result = self.detail_view.handleCanvasEvent(self.canvas, ev)
    if result == "__hover_changed__":
      self.canvas_dirty = True
      return lev2.ui.HandlerResult()
    if result == "__text_copied__":
      self.canvas_dirty = True
      res = lev2.ui.HandlerResult()
      res.setHandler(self.canvas)
      return res
    if result:
      self._on_button(result)
      res = lev2.ui.HandlerResult()
      res.setHandler(self.canvas)
      return res
    return lev2.ui.HandlerResult()

  def _on_button(self, name):
    """Dispatch table for all canvas button actions.
    Button names use prefixes for import-config actions (IC_*, IC:name)."""
    # CDN validate mode has its own buttons
    if name == "CDN_VALIDATE_BACK":
      self.cdn_validate_mode = False
      self.canvas_dirty = True
      return
    if name == "CDN_VALIDATE_REFRESH":
      self.model._bg(self.model.validate_cdn_all)
      return
    m = self.model
    key = self.selected_key
    if not key:
      return
    kt = m.key_type(key)
    fqid = m.key_to_fqid(key)
    ns = m.key_to_ns(key)
    if name == "FETCH":
      if kt == "asset" and fqid:
        m.fetch(fqid)
    elif name == "FETCH ALL":
      if kt == "namespace" and ns:
        m.fetch_namespace(ns)
    elif name == "UPLOAD":
      if kt == "asset" and fqid:
        m.upload(fqid)
      elif kt == "namespace" and ns:
        m.upload_namespace(ns)
    elif name == "CANCEL":
      if kt == "asset" and fqid:
        m.cancel_fetch(fqid)
    elif name == "CANCEL_UPLOAD":
      if kt == "asset" and fqid:
        m.cancel_upload(fqid)
    elif name == "VERIFY HASH":
      if kt == "asset" and fqid:
        m._bg(lambda: m.verify_hashes(fqid))
    elif name == "CLEAR LOCAL":
      if kt == "asset" and fqid:
        m._bg(lambda: m.clear_local(fqid))
    elif name == "DELETE_FROM_MANIFEST":
      if kt == "asset" and fqid:
        m.delete_from_manifest(fqid)
        self.selected_key = None
        self.canvas_dirty = True
    # --- Import config actions (IC_ prefix = global, IC: prefix = select by name) ---
    elif name == "IC_BACK":
      self.selected_import_config = None  # return to normal detail view
      m.import_output_lines = []
      self.canvas_dirty = True
    elif name == "IC_IMPORT":
      if self.selected_import_config:
        m.run_import(self.selected_import_config, dry_run=False)
    elif name == "IC_DRY_RUN":
      if self.selected_import_config:
        m.run_import(self.selected_import_config, dry_run=True)
    elif name == "IC_LIST":
      if self.selected_import_config:
        m.run_import(self.selected_import_config, list_only=True)
    elif name == "IC_EDIT":
      if self.selected_import_config:
        self._open_import_config_editor(self.selected_import_config)
    elif name == "NS_EDIT":
      if kt == "namespace" and ns:
        self._open_namespace_editor(ns)
    elif name == "NS_NEW":
      project = m.key_to_project(key) if key else None
      if project:
        def on_ns_name(ns_name, _proj=project):
          m.create_namespace(_proj, ns_name)
          self.canvas_dirty = True
        self._text_prompt = TextPromptOverlay(self.uicontext, self.ezapp,
                          f"New namespace for [{project}]:",
                          default_text="",
                          on_commit=on_ns_name)
    elif name == "IC_NEW":
      project = m.key_to_project(key) if key else None
      ic_ns = m.key_to_ns(key)
      if project:
        def on_name(config_name, _proj=project, _ns=ic_ns):
          basename = m.create_import_config(_proj, name=config_name, namespace=_ns)
          if basename:
            self.selected_import_config = basename
            self.canvas_dirty = True
        default_name = f"{ic_ns}_import" if ic_ns else f"{project}_import"
        self._text_prompt = TextPromptOverlay(self.uicontext, self.ezapp,
                          f"New import config for [{project}] {ic_ns or ''}:",
                          default_text=default_name,
                          on_commit=on_name)
    elif name.startswith("AUTOCORRECT:"):
      ic_name = name[12:]
      m.autocorrect_import_config(ic_name)
    elif name.startswith("IC:"):
      ic_name = name[3:]  # "IC:foo.json" → select import config "foo.json"
      self.selected_import_config = ic_name
      m.import_output_lines = []
      self.canvas_dirty = True

  def _open_import_config_editor(self, ic_name):
    """Create the ImportConfigEditor overlay on demand (lazy instantiation)."""
    self.ic_editor = ImportConfigEditor(self.uicontext, self.model, self.ezapp)
    self.ic_editor.on_close = lambda: setattr(self, 'canvas_dirty', True)
    self.ic_editor.open(ic_name)

  def _open_namespace_editor(self, ns):
    """Open overlay editor for namespace config (encryption_key + remote_location)."""
    m = self.model
    project = m.ns_project.get(ns)
    if not project:
      return
    # Find config.json path
    ns_list = m.project_namespaces.get(project, [])
    if not ns_list:
      return
    manifests = m.catalog.manifestsForNamespace(ns_list[0])
    if not manifests or not manifests[0].source_file:
      return
    manifest_dir = os.path.dirname(manifests[0].source_file)
    config_path = os.path.join(manifest_dir, "config.json")
    if not os.path.exists(config_path):
      return

    with open(config_path, 'r') as f:
      config_data = json.load(f)
    ns_config = config_data.get("namespaces", {}).get(ns, {})
    locations = sorted(config_data.get("locations", {}).keys())

    # Build overlay: BorderFrame > VPack > Toolbar + PropertySheet
    lg = self.ezapp.topLayoutGroup
    top_w = lg.width
    top_h = lg.height
    ow = min(600, top_w - 80)
    oh = 220
    ox = (top_w - ow) // 2
    oy = (top_h - oh) // 2

    frame = self.uicontext.createOverlayWidget(
      lev2.ui.BorderFrame, ["ns_editor_frame"])
    frame.border_width = 12
    frame.border_edge_width = 3
    frame.border_color = vec4(0, 0, 0, 1)
    frame.border_outer_color = vec4(0, 0, 0, 1)
    frame.border_inner_color = vec4(1, 1, 0, 1)

    self.uicontext.pushOverlay(frame, ox, oy, ow, oh, dismiss_on_click_outside=False)

    vpack = lev2.ui.VerticalPack.wfactory(["ns_editor_vpack"])
    frame.child = vpack
    vpack.margin = 4
    vpack.item_height = 36

    # Toolbar
    toolbar = vpack.makeChild(uiclass=lev2.ui.Toolbar, args=["ns_editor_toolbar"])
    toolbar.bgcolor = vec4(0.15, 0.15, 0.18, 1)
    toolbar.button_color = vec4(0.20, 0.20, 0.25, 1)
    toolbar.button_hover_color = vec4(0.28, 0.28, 0.35, 1)
    toolbar.button_pressed_color = vec4(0.25, 0.45, 0.65, 1)
    toolbar.button_border_color = vec4(0.35, 0.35, 0.42, 1)
    toolbar.button_border_width = 1
    toolbar.icon_size = 28
    toolbar.button_padding = 4
    toolbar.label_padding = 4
    toolbar.item_spacing = 8
    toolbar.edge_padding = 8

    # Mutable state for dirty tracking
    ns_editor_state = {"dirty": False, "data": dict(ns_config)}

    btn_close = toolbar.addTextButton("ns_ed_close", "CLOSE")
    btn_close.custom_width = 56
    btn_save = toolbar.addTextButton("ns_ed_save", "SAVE")
    btn_save.custom_width = 48
    lbl = toolbar.addTextButton("ns_ed_label", f"Namespace: {ns}")
    lbl.custom_width = ow - 200

    uictx = self.uicontext
    app = self

    def do_close():
      uictx.popOverlay()
      app.canvas_dirty = True

    def do_save():
      config_data["namespaces"][ns] = ns_editor_state["data"]
      with open(config_path, 'w') as f:
        json.dump(config_data, f, indent=2)
        f.write('\n')
      ns_editor_state["dirty"] = False
      btn_close.color_override = vec4(0, 0, 0, 0)
      btn_save.color_override = vec4(0, 0, 0, 0)
      # Rescan to pick up config changes
      core.AssetCatalog.reloadAllManifests(m.catalog)
      m.scan()

    btn_close.onPressed(do_close)
    btn_save.onPressed(do_save)

    # PropertySheet
    propsheet = vpack.makeChild(uiclass=lev2.ui.PropertySheet, args=["ns_editor_propsheet"])
    propsheet.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    propsheet.label_color = vec4(0.9, 0.9, 0.9, 1)
    propsheet.group_color = vec4(0.18, 0.18, 0.22, 1)
    propsheet.row_height = 28
    propsheet.label_width = 140
    vpack.fill_widget = propsheet

    # Build VarMap from namespace config
    vm = core.VarMap()
    vm.encryption_key = ns_editor_state["data"].get("encryption_key", "")
    vm.remote_location = ns_editor_state["data"].get("remote_location", "")
    propsheet.data = vm

    # Register custom editors: EncKeySelect for encryption_key, location dropdown
    propsheet.registerEditorFactory(tokens.EncKeySelect,
      lambda sheet, key, value, annot: self._ns_enc_key_editor(sheet, key, value, annot, ns_editor_state, btn_close, btn_save))
    propsheet.registerEditorFactory(tokens.LocationSelect,
      lambda sheet, key, value, annot: self._ns_location_editor(sheet, key, value, annot, ns_editor_state, locations, btn_close, btn_save))

    ps_model = propsheet.model
    ek_annot = core.VarMap()
    ek_annot.type = tokens.EncKeySelect
    ps_model.setAnnotations("encryption_key", ek_annot)
    loc_annot = core.VarMap()
    loc_annot.type = tokens.LocationSelect
    ps_model.setAnnotations("remote_location", loc_annot)
    propsheet.expandAll()

    self._ns_editor_ref = frame  # prevent GC

  def _ns_enc_key_editor(self, sheet, key, value, annotations, state, btn_close, btn_save):
    """ChoicelistWidget for encryption key in namespace editor."""
    current = str(value) if value else ""
    enc_vars = sorted(v for v in os.environ if "ENC_KEY" in v)
    choices = [f"${{{v}}}" for v in enc_vars]
    if current and current not in choices:
      choices.insert(0, current)
    widget = lev2.ui.ChoicelistWidget("ns_ek", current if current else "(none)")
    widget.setChoices(choices)
    widget.bg_color = vec4(0.15, 0.15, 0.2, 1)
    def on_selected(selected):
      state["data"]["encryption_key"] = selected
      state["dirty"] = True
      widget.current_value = selected
      btn_close.color_override = vec4(0.5, 0.12, 0.12, 1)
      btn_save.color_override = vec4(0.12, 0.4, 0.12, 1)
    widget.onChoiceSelected = on_selected
    return widget

  def _ns_location_editor(self, sheet, key, value, annotations, state, locations, btn_close, btn_save):
    """ChoicelistWidget for remote location in namespace editor."""
    current = str(value) if value else ""
    choices = list(locations)
    if current and current not in choices:
      choices.insert(0, current)
    widget = lev2.ui.ChoicelistWidget("ns_loc", current if current else "(none)")
    widget.setChoices(choices)
    widget.bg_color = vec4(0.15, 0.15, 0.2, 1)
    def on_selected(selected):
      state["data"]["remote_location"] = selected
      state["dirty"] = True
      widget.current_value = selected
      btn_close.color_override = vec4(0.5, 0.12, 0.12, 1)
      btn_save.color_override = vec4(0.12, 0.4, 0.12, 1)
    widget.onChoiceSelected = on_selected
    return widget

  ############################################################################
  # Update loop — poll model for background results, redraw if dirty
  ############################################################################

  def _onUpdate(self, updinfo):
    self.model.poll()  # process any completed background tasks
    # Keep redrawing during copy-flash animation
    if self.detail_view and self.detail_view.isFlashActive():
      self.canvas_dirty = True
    if self.canvas_dirty and self.canvas_ready:
      self._redraw_canvas()

  ############################################################################
  # Canvas drawing — routes to the correct view based on selection type
  #
  # Drawing pattern used throughout:
  #   dv = self.detail_view  (shorthand for drawing helper)
  #   m  = self.model        (shorthand for data access)
  #   y  = vertical layout cursor, incremented after each drawn element
  ############################################################################

  def _redraw_canvas(self):
    self.canvas_dirty = False
    dv = self.detail_view   # drawing helper shorthand
    m = self.model           # model shorthand
    dv.beginRedraw()         # clears text layers and button list

    w = max(self.canvas.width, 400)
    h = max(self.canvas.height, 200)

    # Common header: CDN health + active download status
    y = 4
    y = dv.drawCdnHealthHeader(m, y, w)
    y = dv.drawTransferStatus(m, y)

    # CDN validation mode takes over the entire detail view
    if self.cdn_validate_mode:
      self._draw_cdn_validate_view(y, w, h)
      self.canvas.markDirty()
      return

    # When an import config is selected, it takes over the entire detail view
    if self.selected_import_config:
      self._draw_import_config_full_view(self.selected_import_config, y, w, h)
      self.canvas.markDirty()
      return

    # Route to the appropriate view based on the type of outliner selection
    key = self.selected_key
    if not key:
      dv._texts["label"].addItem("Select a project, namespace, or asset", vec2(8, y))
      self.canvas.markDirty()
      return

    kt = m.key_type(key)
    if kt == "asset":
      fqid = m.key_to_fqid(key)
      self._draw_asset_view(fqid, y, w, h)
    elif kt == "namespace":
      ns = m.key_to_ns(key)
      self._draw_namespace_view(ns, y, w, h)
    elif kt == "project":
      self._draw_project_view(key, y, w, h)

    self.canvas.markDirty()

  ############################################################################
  # Project view — summary of all namespaces, manifests, and import configs
  ############################################################################

  def _draw_project_view(self, project, y_start, w, h):
    dv = self.detail_view
    m = self.model
    y = y_start
    ns_list = m.project_namespaces.get(project, [])

    dv.drawSectionHeader("Project", y, w, h)
    y += 18
    dv.drawPropRow("Name", project, y)
    y += 16
    dv.drawPropRow("Namespaces", str(len(ns_list)), y)
    y += 16

    # Aggregate chunk/size stats across all namespaces in the project
    total_assets = 0
    total_chunks = 0
    present_chunks = 0
    total_sz = 0
    local_sz = 0
    for ns in ns_list:
      for fqid in m.ns_assets.get(ns, []):
        total_assets += 1
        cs = m.chunk_status.get(fqid, [])
        total_chunks += len(cs)
        present_chunks += sum(1 for x in cs if x)
        entry = m.asset_entries.get(fqid)
        if entry:
          total_sz += entry.archive_size
          if cs and all(cs):
            local_sz += entry.archive_size

    dv.drawPropRow("Total Assets", str(total_assets), y)
    y += 16
    dv.drawPropRow("Chunks Local", f"{present_chunks} / {total_chunks}", y)
    y += 16
    dv.drawPropRow("Size Local", f"{format_size(local_sz)} / {format_size(total_sz)}", y)
    y += 16

    if ns_list:
      manifests = m.catalog.manifestsForNamespace(ns_list[0])
      if manifests and manifests[0].source_file:
        manifest_dir = os.path.dirname(manifests[0].source_file)
        dv.drawPropRow("Manifest Dir", manifest_dir, y)
        y += 16
    y += 4

    # Manifest files
    mfiles = m.project_manifests.get(project, [])
    if mfiles:
      dv.drawSectionHeader("Manifests", y, w, h)
      y += 18
      for mf in mfiles:
        if y > h - 40:
          dv._texts["label"].addItem(f"... {len(mfiles)} total", vec2(8, y))
          y += 16
          break
        dv._texts["label"].addItem(mf, vec2(8, y))
        y += 16

    y += 4

    # Namespace summary grid — color-coded by completeness (ok/warn/plain)
    dv.drawSectionHeader("Namespaces", y, w, h)
    dv.addButton("NS_NEW", w - 56, y, 48, 16, "NEW")
    y += 18
    for ns in ns_list:
      if y > h - 20:
        dv._texts["label"].addItem(f"... {len(ns_list)} total", vec2(8, y))
        break
      assets = m.ns_assets.get(ns, [])
      n_total = 0
      n_present = 0
      for fqid in assets:
        cs = m.chunk_status.get(fqid, [])
        n_total += len(cs)
        n_present += sum(1 for x in cs if x)
      label = f"{ns}  ({len(assets)} assets, {n_present}/{n_total} chunks)"
      if n_total > 0 and n_present == n_total:
        dv._texts["status_ok"].addItem(label, vec2(8, y))
      elif n_present > 0:
        dv._texts["status_warn"].addItem(label, vec2(8, y))
      else:
        dv._texts["label"].addItem(label, vec2(8, y))
      y += 16

  ############################################################################
  # Namespace view — config, status, action buttons, and asset grid
  ############################################################################

  def _draw_namespace_view(self, ns, y_start, w, h):
    dv = self.detail_view
    m = self.model
    y = y_start

    # build_namespace_varmap returns a dict of display-ready property values
    d = build_namespace_varmap(m, ns)
    dv.drawSectionHeader("Namespace", y, w, h)
    dv.addButton("NS_EDIT", w - 56, y, 48, 16, "EDIT")
    y += 18
    for key in ["Name", "Asset Count"]:
      if key in d:
        dv.drawPropRow(key, d[key], y)
        y += 16
    y += 4
    dv.drawSectionHeader("Config", y, w, h)
    y += 18
    for key in ["Encryption Key", "Remote Location", "Download URL", "Upload URL",
                 "TLS Verify", "API Key (read)", "API Key (write)"]:
      if key in d:
        dv.drawPropRow(key, d[key], y)
        y += 16
    y += 4
    dv.drawSectionHeader("Status", y, w, h)
    y += 18
    for key in ["Chunks Local", "Size Local"]:
      if key in d:
        dv.drawPropRow(key, d[key], y)
        y += 16
    if "CDN Endpoint" in d:
      y += 4
      dv.drawSectionHeader("CDN", y, w, h)
      y += 18
      for key in ["CDN Endpoint", "CDN Reachable", "CDN Latency"]:
        if key in d:
          dv.drawPropRow(key, d[key], y)
          y += 16

    # Manifest files
    ns_mfiles = m.ns_manifests.get(ns, [])
    if ns_mfiles:
      y += 4
      dv.drawSectionHeader("Manifests", y, w, h)
      y += 18
      for mf in ns_mfiles:
        dv.drawPropRow("File", mf, y)
        y += 16

    # Import configs
    ns_ics = m.ns_import_configs.get(ns, [])
    y += 4
    dv.drawSectionHeader("Import Configs", y, w, h)
    dv.addButton("IC_NEW", w - 56, y, 48, 16, "NEW")
    y += 18
    for ic in ns_ics:
      btn_name = f"IC:{ic}"
      dv.addButton(btn_name, 8, y, 20, 16, ">")
      dv._texts["label"].addItem(ic, vec2(32, y))
      y += 18
    if not ns_ics:
      dv._texts["label"].addItem("(none)", vec2(8, y))
      y += 16
    y += 8

    # Determine button ghost state: disable fetch if all local, upload if all on CDN
    can_fetch = m.ns_can_fetch(ns)
    can_upload = m.ns_can_upload(ns)
    all_local = True
    all_cdn = True
    for fqid in m.ns_assets.get(ns, []):
      cs = m.chunk_status.get(fqid, [])
      if not cs or not all(cs):
        all_local = False
      cdn = m.cdn_status.get(fqid, [])
      if not cdn or not all(cdn):
        all_cdn = False

    bx = 8
    dv.addButton("FETCH ALL", bx, y, 80, 22, "FETCH ALL", ghost=all_local or not can_fetch)
    bx += 88
    dv.addButton("UPLOAD", bx, y, 64, 22, "UPLOAD", ghost=all_cdn or not can_upload)
    y += 30

    # Asset grid — compact multi-column layout, color-coded by chunk presence
    assets = m.ns_assets.get(ns, [])
    if not assets:
      dv._texts["label"].addItem("(no assets)", vec2(8, y))
      return

    dv.drawSectionHeader("Assets", y, w, h)
    y += 18

    # Compute grid dimensions to fit available canvas area
    cell_w = 200
    cell_h = 16
    max_name = 14
    cols = max(1, (w - 16) // cell_w)
    rows_avail = max(1, (h - y - 40) // cell_h)
    total_slots = cols * rows_avail
    show_all = len(assets) <= total_slots
    assets_to_show = assets if show_all else assets[:total_slots - 1]

    for idx, fqid in enumerate(assets_to_show):
      col = idx % cols
      row = idx // cols
      cx = 8 + col * cell_w
      cy = y + row * cell_h

      name = fqid.split("|", 1)[1] if "|" in fqid else fqid
      if len(name) > max_name:
        name = name[:max_name - 2] + ".."

      cs = m.chunk_status.get(fqid, [])
      ntotal = len(cs)
      npresent = sum(1 for x in cs if x) if cs else 0

      if ntotal > 0:
        label = f"{name}  {npresent}/{ntotal}"
        if fqid in m.active_fetches:
          dv._texts["status_warn"].addItem(label, vec2(cx, cy))
        elif npresent == ntotal:
          dv._texts["status_ok"].addItem(label, vec2(cx, cy))
        elif npresent == 0:
          dv._texts["status_err"].addItem(label, vec2(cx, cy))
        else:
          pct = int(npresent / ntotal * 100)
          label = f"{name}  {npresent}/{ntotal} ({pct}%)"
          dv._texts["status_warn"].addItem(label, vec2(cx, cy))
      else:
        dv._texts["label"].addItem(f"{name}  --", vec2(cx, cy))

    if not show_all:
      remaining = len(assets) - len(assets_to_show)
      col = len(assets_to_show) % cols
      row = len(assets_to_show) // cols
      cx = 8 + col * cell_w
      cy = y + row * cell_h
      dv._texts["status_warn"].addItem(f"+{remaining} more", vec2(cx, cy))

    grid_rows = (len(assets_to_show) + cols - 1) // cols
    if not show_all:
      grid_rows = rows_avail
    y += grid_rows * cell_h + 8

    # Summary
    total_chunks, present_chunks = 0, 0
    total_sz, local_sz = 0, 0
    for fqid in assets:
      cs = m.chunk_status.get(fqid, [])
      total_chunks += len(cs)
      present_chunks += sum(1 for x in cs if x)
      entry = m.asset_entries.get(fqid)
      if entry:
        total_sz += entry.archive_size
        if cs and all(cs):
          local_sz += entry.archive_size
    summary = f"Local: {present_chunks}/{total_chunks} chunks  |  {format_size(local_sz)} / {format_size(total_sz)}"
    dv._texts["value"].addItem(summary, vec2(8, y))

  ############################################################################
  # Asset view — full detail: properties, chunk grid, progress, actions
  ############################################################################

  def _draw_asset_view(self, fqid, y_start, w, h):
    dv = self.detail_view
    m = self.model
    y = y_start
    entry = m.asset_entries.get(fqid)
    if not entry:
      dv._texts["label"].addItem("No entry", vec2(8, y))
      return

    # build_asset_varmap returns a dict of display-ready property values
    d = build_asset_varmap(m, fqid)
    dv.drawSectionHeader("Asset", y, w, h)
    y += 18
    for key in ["ID", "Manifest", "Type", "Platforms", "Priority"]:
      if key in d:
        dv.drawPropRow(key, d[key], y)
        y += 16
    dv.addButton("DELETE_FROM_MANIFEST", 8, y, 160, 18, "DELETE FROM MANIFEST", danger=True)
    y += 22
    y += 4
    dv.drawSectionHeader("Sizes", y, w, h)
    y += 18
    for key in ["Archive Size", "Compressed Size", "Encrypted Size", "Compression"]:
      if key in d:
        dv.drawPropRow(key, d[key], y)
        y += 16
    y += 4
    dv.drawSectionHeader("Hashes", y, w, h)
    y += 18
    for key in ["Content Hash", "Storage Hash", "Hash Algorithm",
                 "File Hash (expected)", "Encrypted File"]:
      if key in d:
        dv.drawPropRow(key, d[key], y)
        y += 16

    # Verify hash button + result
    verify_result = m.hash_verify_results.get(fqid, "")
    verifying = verify_result == "verifying..."
    dv.addButton("VERIFY HASH", 8, y, 96, 18, "VERIFY HASH", ghost=verifying)
    if verify_result:
      role = "status_ok" if verify_result == "OK" else "status_warn" if verifying else "status_err"
      dv._texts[role].addItem(verify_result, vec2(112, y + 1))
    else:
      dv._texts["label"].addItem("(not checked)", vec2(112, y + 1))
    y += 22
    y += 4
    dv.drawSectionHeader("Chunks", y, w, h)
    y += 18
    for key in ["Total Chunks", "Chunk Size", "Chunks Local", "Chunks Valid"]:
      if key in d:
        dv.drawPropRow(key, d[key], y)
        y += 16
    if any(k in d for k in ["Local Location", "Local Path", "Remote URL"]):
      y += 4
      dv.drawSectionHeader("Location", y, w, h)
      y += 18
      for key in ["Local Location", "Local Path", "Local Exists", "Remote URL"]:
        if key in d:
          dv.drawPropRow(key, d[key], y)
          y += 16
    y += 8

    # Action buttons — ghosted based on local/CDN completeness and fetch state
    ns = fqid.split("|")[0]
    can_fetch = m.ns_can_fetch(ns)
    can_upload = m.ns_can_upload(ns)
    cs = m.chunk_status.get(fqid, [])
    cdn = m.cdn_status.get(fqid, [])
    fetch_ghost = (bool(cs) and all(cs)) or not can_fetch
    upload_ghost = (bool(cdn) and all(cdn)) or not can_upload
    is_fetching = fqid in m.active_fetches
    is_uploading = fqid in m.active_uploads

    bx = 8
    dv.addButton("FETCH", bx, y, 56, 22, "FETCH", ghost=fetch_ghost or is_fetching)
    bx += 64
    dv.addButton("CANCEL", bx, y, 64, 22, "CANCEL", ghost=not is_fetching, danger=is_fetching)
    bx += 72
    dv.addButton("UPLOAD", bx, y, 64, 22, "UPLOAD", ghost=upload_ghost or is_uploading)
    bx += 72
    dv.addButton("CANCEL_UPLOAD", bx, y, 64, 22, "CANCEL", ghost=not is_uploading, danger=is_uploading)
    bx += 72
    dv.addButton("CLEAR LOCAL", bx, y, 90, 22, "CLEAR LOCAL", danger=True)
    y += 30

    # Live download progress bar
    fetch_req = m.active_fetches.get(fqid)
    if fetch_req:
      dl_total = fetch_req.bytes_total
      dl_done = fetch_req.bytes_downloaded
      dl_chunks_done = fetch_req.chunks_completed
      dl_chunks_total = fetch_req.chunks_total
      dl_pct = fetch_req.progress

      bar_x, bar_w, bar_h = 8, w - 16, 20
      dv.drawProgressBar(bar_x, y, bar_w, bar_h, dl_pct,
                         vec4(0.2, 0.4, 0.8, 1), vec4(0.15, 0.15, 0.18, 1), h)
      pct_i = int(dl_pct * 100)
      dl_text = f"Downloading: {format_size(dl_done)} / {format_size(dl_total)}  ({pct_i}%)  chunks {dl_chunks_done}/{dl_chunks_total}"
      text_w = len(dl_text) * 8
      tx = bar_x + (bar_w - text_w) // 2
      ty = y + (bar_h - 14) // 2
      dv._texts["value"].addItem(dl_text, vec2(tx, ty))
      y += bar_h + 4

    # Live upload progress bar
    upload_req = m.active_uploads.get(fqid)
    if upload_req:
      import time as _time
      ul_total = upload_req.bytes_total
      ul_done = upload_req.bytes_uploaded
      ul_chunks_done = upload_req.chunks_completed
      ul_chunks_total = upload_req.chunks_total
      ul_pct = upload_req.progress

      # Compute upload rate: sample over 10 seconds, hold until next sample
      now = _time.time()
      ul_rate_key = f"_ul_rate_{fqid}"
      prev = getattr(self, ul_rate_key, None)  # (sample_time, sample_bytes, rate)
      if prev:
        dt = now - prev[0]
        if dt >= 10.0:
          # New sample period — compute rate and reset
          db = ul_done - prev[1]
          rate_mbs = db / dt / 1048576.0
          setattr(self, ul_rate_key, (now, ul_done, rate_mbs))
        else:
          rate_mbs = prev[2]  # hold previous rate
      else:
        rate_mbs = 0.0
        setattr(self, ul_rate_key, (now, ul_done, 0.0))

      bar_x, bar_w, bar_h = 8, w - 16, 20
      dv.drawProgressBar(bar_x, y, bar_w, bar_h, ul_pct,
                         vec4(0.6, 0.4, 0.2, 1), vec4(0.15, 0.15, 0.18, 1), h)
      pct_i = int(ul_pct * 100)
      ul_text = f"Uploading: {format_size(ul_done)} / {format_size(ul_total)}  ({pct_i}%)  chunks {ul_chunks_done}/{ul_chunks_total}  {rate_mbs:.1f} MiB/s"
      text_w = len(ul_text) * 8
      tx = bar_x + (bar_w - text_w) // 2
      ty = y + (bar_h - 14) // 2
      dv._texts["value"].addItem(ul_text, vec2(tx, ty))
      y += bar_h + 4

    vs = m.chunk_valid.get(fqid, [])
    cdn = m.cdn_status.get(fqid, [])
    total = len(cs)
    if total == 0:
      dv._texts["label"].addItem("No chunks", vec2(8, y))
      return

    present = sum(1 for x in cs if x)
    chunk_sz = ""
    if hasattr(entry, 'chunk_manifest') and entry.chunk_manifest:
      cm = entry.chunk_manifest
      chunk_sz = format_size(cm.chunk_size) if hasattr(cm, 'chunk_size') else ""

    dv._texts["label"].addItem(
      f"Chunk Status ({total} chunks, {chunk_sz} each):", vec2(8, y))
    y += 20

    # Chunk grid — one cell per chunk, LOC row shows local presence, CDN row shows remote
    cell_w = 12
    cells_per_row = max(1, (w - 60) // cell_w)
    grid_x = 50

    y = dv.drawChunkGrid(cs, vs, cdn, fqid in m.active_fetches,
                         grid_x, y, cell_w, h, cells_per_row, "LOC")

    if cdn:
      y = dv.drawChunkGrid(cs, vs, cdn, False,
                           grid_x, y, cell_w, h, cells_per_row, "CDN")

    y += 4
    cdn_present = sum(1 for x in cdn if x) if cdn else 0
    summary = f"Local: {present}/{total}"
    if cdn:
      summary += f"     CDN: {cdn_present}/{total}"
    dv._texts["value"].addItem(summary, vec2(8, y))
    y += 20

    # Overall progress bar
    frac = present / total if total > 0 else 0
    dv.drawProgressBar(8, y, w - 16, 16, frac,
                       vec4(0.2, 0.6, 0.2, 1) if frac >= 1.0 else vec4(0.3, 0.5, 0.3, 1),
                       vec4(0.15, 0.15, 0.18, 1), h)
    pct = int(frac * 100)
    dv._texts["value"].addItem(f"{pct}%", vec2(8 + (w - 16) // 2 - 10, y))

  ############################################################################
  # CDN Validation view
  # Shows all namespaces with compact chunk grids comparing LOC vs CDN status.
  ############################################################################

  def _draw_cdn_validate_view(self, y_start, w, h):
    dv = self.detail_view
    m = self.model
    y = y_start

    # Header with back and refresh buttons
    dv.addButton("CDN_VALIDATE_BACK", 8, y, 56, 22, "BACK")
    dv.addButton("CDN_VALIDATE_REFRESH", 72, y, 80, 22, "REFRESH")
    dv._texts["cdn_hdr"].addItem("CDN Validation", vec2(160, y + 3))
    y += 30

    # ── Pass 1: collect namespace data ──
    ns_entries = []  # [(project, ns, n_assets, ns_loc, ns_cdn), ...]
    for project in m.projects:
      for ns in m.project_namespaces.get(project, []):
        assets = m.ns_assets.get(ns, [])
        if not assets:
          continue
        ns_loc = []
        ns_cdn = []
        for fqid in assets:
          cs = m.chunk_status.get(fqid, [])
          cdn = m.cdn_status.get(fqid, [])
          ns_loc.extend(cs)
          if cdn:
            ns_cdn.extend(cdn)
          else:
            ns_cdn.extend([None] * len(cs))
        if not ns_loc:
          continue
        ns_entries.append((project, ns, len(assets), ns_loc, ns_cdn))

    if not ns_entries:
      dv._texts["label"].addItem("No assets found", vec2(8, y))
      return

    # ── Pass 2: compute cell size to fit everything ──
    # Layout per namespace: 18px header + grid rows * cell_h + 4px gap
    # Plus 26px for grand total footer
    cell_margin = 1
    gap = 8
    label_w = 32
    half_w = (w - gap) // 2
    grid_w = half_w - label_w - 8
    avail_h = h - y - 26  # space for all namespaces (reserve footer)
    header_cost = len(ns_entries) * 22  # 18px header + 4px gap per ns
    grid_h_budget = max(1, avail_h - header_cost)

    # Try cell sizes from 8 down to 2, pick largest that fits
    cell_w = 2  # minimum fallback
    for try_cw in range(8, 1, -1):
      cells_per_row = max(1, grid_w // try_cw)
      total_grid_h = 0
      for _, _, _, ns_loc, _ in ns_entries:
        rows = (len(ns_loc) + cells_per_row - 1) // cells_per_row
        total_grid_h += rows * try_cw
      if total_grid_h <= grid_h_budget:
        cell_w = try_cw
        break

    cell_h = cell_w
    loc_label_x = 8
    loc_grid_x = loc_label_x + label_w
    cdn_label_x = half_w + gap
    cdn_grid_x = cdn_label_x + label_w
    cells_per_row = max(1, grid_w // cell_w)

    # ── Pass 3: draw ──
    grand_total = 0
    grand_local = 0
    grand_cdn = 0
    grand_missing = 0

    for project, ns, n_assets, ns_loc, ns_cdn in ns_entries:
      total = len(ns_loc)
      local_count = sum(1 for x in ns_loc if x)
      cdn_count = sum(1 for x in ns_cdn if x)
      missing = sum(1 for loc, cdn in zip(ns_loc, ns_cdn) if loc and cdn is not None and not cdn)
      unchecked = sum(1 for x in ns_cdn if x is None)

      grand_total += total
      grand_local += local_count
      grand_cdn += cdn_count
      grand_missing += missing

      # Section header: [project] namespace (n assets, n chunks)  stats
      hdr = f"[{project}] {ns}  ({n_assets} assets, {total} chunks)"
      dv.drawSectionHeader(hdr, y, w, h)
      stats = f"LOC {local_count}/{total}  CDN {cdn_count}/{total}"
      if missing > 0:
        stats += f"  MISS {missing}"
        dv._texts["status_err"].addItem(stats, vec2(w - len(stats) * 8 - 8, y))
      elif unchecked > 0:
        dv._texts["status_warn"].addItem(stats + " ...", vec2(w - (len(stats) + 4) * 8 - 8, y))
      else:
        dv._texts["status_ok"].addItem(stats, vec2(w - len(stats) * 8 - 8, y))
      y += 18

      # LOC and CDN labels
      dv._texts["label"].addItem("LOC:", vec2(loc_label_x, y))
      dv._texts["label"].addItem("CDN:", vec2(cdn_label_x, y))

      # Draw both grids side by side
      grid_rows = (total + cells_per_row - 1) // cells_per_row
      for i in range(total):
        row = i // cells_per_row
        col = i % cells_per_row
        cy = y + row * cell_h

        # LOC cell
        cx_loc = loc_grid_x + col * cell_w
        qd = lev2.ui.QuadData()
        qd.setPosition(cx_loc, cy + cell_margin)
        qd.setSize(cell_w - 2 * cell_margin, cell_h - 2 * cell_margin)
        if ns_loc[i]:
          qd.setColor(vec4(0.2, 0.6, 0.2, 1))
        else:
          qd.setColor(vec4(0.2, 0.2, 0.2, 1))
        dv.bg_prim.addQuad(qd)

        # CDN cell
        cx_cdn = cdn_grid_x + col * cell_w
        qd2 = lev2.ui.QuadData()
        qd2.setPosition(cx_cdn, cy + cell_margin)
        qd2.setSize(cell_w - 2 * cell_margin, cell_h - 2 * cell_margin)
        cdn_val = ns_cdn[i]
        if cdn_val is None:
          qd2.setColor(vec4(0.15, 0.15, 0.2, 1))   # dim: not yet checked
        elif cdn_val:
          qd2.setColor(vec4(0.2, 0.6, 0.2, 1))     # green: present on CDN
        else:
          qd2.setColor(vec4(0.8, 0.15, 0.15, 1))   # red: missing from CDN
        dv.bg_prim.addQuad(qd2)

      y += grid_rows * cell_h + 4

    # Grand total footer
    dv.drawSectionHeader("Total", y, w, h)
    total_summary = f"Chunks: {grand_total}  Local: {grand_local}  CDN: {grand_cdn}"
    if grand_missing > 0:
      total_summary += f"  MISSING FROM CDN: {grand_missing}"
      dv._texts["status_err"].addItem(total_summary, vec2(w - len(total_summary) * 8 - 8, y))
    else:
      dv._texts["status_ok"].addItem(total_summary, vec2(w - len(total_summary) * 8 - 8, y))

  ############################################################################
  # Import Config views
  # When selected_import_config is set, _draw_import_config_full_view takes
  # over the entire detail canvas with a back button to return.
  ############################################################################

  def _draw_import_config_detail(self, ic_name, y, w, h):
    """Draw expanded import config details inline. Returns updated y."""
    dv = self.detail_view
    m = self.model
    data = m.load_import_config(ic_name)
    if not data:
      dv._texts["status_err"].addItem("  (could not load)", vec2(8, y))
      return y + 16
    indent = 24

    if m.import_config_needs_autocorrect(ic_name):
      btn_name = f"AUTOCORRECT:{ic_name}"
      dv.addButton(btn_name, indent, y, 96, 16, "AUTOCORRECT", danger=True)
      dv._texts["status_err"].addItem("(paths need correction)", vec2(indent + 104, y + 1))
      y += 20

    display_names = {
      "namespace": "namespace",
      "source_dir": "project dir",
      "local_loc": "local_loc",
      "manifest": "manifest",
      "encryption_key": "encryption_key",
    }
    for key in ["namespace", "source_dir", "local_loc", "manifest", "encryption_key"]:
      val = data.get(key)
      if val:
        label = display_names.get(key, key)
        dv._texts["label"].addItem(f"{label}:", vec2(indent, y))
        if key in ("source_dir", "manifest") and os.path.isabs(val) and not os.path.exists(val):
          dv._texts["status_err"].addItem(str(val), vec2(indent + 120, y))
        else:
          dv._texts["value"].addItem(str(val), vec2(indent + 120, y))
        y += 14
    if "platforms" in data:
      dv._texts["label"].addItem("platforms:", vec2(indent, y))
      dv._texts["value"].addItem(", ".join(data["platforms"]), vec2(indent + 120, y))
      y += 14
    assets = data.get("assets", [])
    if assets:
      dv._texts["label"].addItem("assets:", vec2(indent, y))
      y += 14
      for a in assets:
        if isinstance(a, dict):
          aid = a.get("id", "?")
          inc = a.get("include", "")
          dv._texts["value"].addItem(f"{aid}  ({inc})", vec2(indent + 8, y))
        else:
          dv._texts["value"].addItem(str(a), vec2(indent + 8, y))
        y += 14
    y += 4
    return y

  def _draw_import_config_full_view(self, ic_name, y_start, w, h):
    """Full-screen import config detail with back button, properties, assets, and actions."""
    dv = self.detail_view
    m = self.model
    y = y_start
    data = m.load_import_config(ic_name)
    path = m.import_config_paths.get(ic_name, "")

    # Back button returns to the normal selection-based detail view
    dv.addButton("IC_BACK", 8, y, 56, 22, "BACK")
    dv._texts["cdn_hdr"].addItem(f"Import Config: {ic_name}", vec2(72, y + 3))
    y += 30

    if not data:
      dv._texts["status_err"].addItem("Could not load config", vec2(8, y))
      return

    dv.drawSectionHeader("Config File", y, w, h)
    y += 18
    dv.drawPropRow("Path", path, y)
    y += 20

    # Properties
    dv.drawSectionHeader("Properties", y, w, h)
    y += 18
    display_map = {
      "namespace": "Namespace",
      "source_dir": "Project Dir",
      "local_loc": "Local Location",
      "manifest": "Manifest",
      "encryption_key": "Encryption Key",
    }
    for key, label in display_map.items():
      val = data.get(key, "")
      if val:
        dv._texts["label"].addItem(f"{label}:", vec2(8, y))
        if key in ("source_dir", "manifest") and os.path.isabs(val) and not os.path.exists(val):
          dv._texts["status_err"].addItem(str(val), vec2(140, y))
        else:
          dv._texts["value"].addItem(str(val), vec2(140, y))
        y += 16
    if "platforms" in data:
      dv._texts["label"].addItem("Platforms:", vec2(8, y))
      dv._texts["value"].addItem(", ".join(data["platforms"]), vec2(140, y))
      y += 16
    if "priority" in data:
      dv._texts["label"].addItem("Priority:", vec2(8, y))
      dv._texts["value"].addItem(str(data["priority"]), vec2(140, y))
      y += 16
    y += 4

    if m.import_config_needs_autocorrect(ic_name):
      dv.addButton(f"AUTOCORRECT:{ic_name}", 8, y, 110, 22, "AUTOCORRECT", danger=True)
      dv._texts["status_err"].addItem("(absolute paths need ${VAR} form)", vec2(126, y + 3))
      y += 28

    # Assets section
    assets = data.get("assets", [])
    dv.drawSectionHeader(f"Assets ({len(assets)})", y, w, h)
    y += 18
    for a in assets:
      if y > h - 60:
        dv._texts["label"].addItem(f"... {len(assets)} total", vec2(8, y))
        y += 16
        break
      if isinstance(a, dict):
        aid = a.get("id", "?")
        inc = a.get("include", "")
        if isinstance(inc, list):
          inc = ", ".join(inc)
        exc = a.get("exclude", [])
        dv._texts["value"].addItem(aid, vec2(8, y))
        dv._texts["label"].addItem(inc, vec2(140, y))
        y += 16
        if exc:
          dv._texts["status_err"].addItem(f"  exclude: {', '.join(exc)}", vec2(140, y))
          y += 16
      else:
        dv._texts["value"].addItem(str(a), vec2(8, y))
        y += 16
    y += 8

    # Action buttons — EDIT opens ImportConfigEditor overlay
    dv.addButton("IC_EDIT", 8, y, 56, 22, "EDIT")
    bx = 72
    dv.addButton("IC_IMPORT", bx, y, 80, 22, "IMPORT")
    bx += 88
    dv.addButton("IC_DRY_RUN", bx, y, 80, 22, "DRY RUN")
    bx += 88
    dv.addButton("IC_LIST", bx, y, 80, 22, "LIST FILES")
    y += 30

    # Output — shows results of IMPORT / DRY RUN / LIST actions
    if m.import_output_lines:
      dv.drawSectionHeader("Output", y, w, h)
      y += 18
      for line in m.import_output_lines:
        if y > h - 16:
          dv._texts["label"].addItem("...", vec2(8, y))
          break
        dv._texts["value"].addItem(line, vec2(8, y))
        y += 14

################################################################################

if __name__ == "__main__":
  app = CatalogTool()
  app.ezapp.mainThreadLoop()
