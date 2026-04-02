#!/usr/bin/env ork.python
################################################################################
# Orkid Asset Catalog GUI Tool
################################################################################

import os, time
from orkengine import core
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.catalog_tool import (
  CatalogModel, format_size,
  build_namespace_varmap, build_asset_varmap,
)
from ork.catalog_tool_ui import (
  CatalogOutlinerModel, CanvasDetailView, ImportConfigEditor,
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
    self.model = CatalogModel()
    self.model.on_dirty = self._on_model_dirty
    self.selected_key = None
    self.selected_import_config = None
    self._selecting = False
    self.canvas_dirty = True
    self.canvas_ready = False
    self.detail_view = None
    self.ic_editor = None
    self.createEzApp(width=1200, height=800, fullscreen=False, name="Asset Catalog Tool")

  def _on_model_dirty(self):
    self.canvas_dirty = True

  ############################################################################
  # UI init
  ############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.12, 0.12, 0.14, 1)

    # Right panel — details
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
    self.canvas.onUiEvent = self._on_canvas_event

    # Left panel — outliner
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
    self.toolbar.button_hover_color = vec4(0.28, 0.28, 0.35, 1)
    self.toolbar.button_pressed_color = vec4(0.25, 0.45, 0.65, 1)
    self.toolbar.separator_color = vec4(0.30, 0.30, 0.35, 1)
    self.toolbar.icon_size = 28
    self.toolbar.button_padding = 8
    self.toolbar.item_spacing = 4
    self.toolbar.edge_padding = 8

    btn_refresh = self.toolbar.addTextButton("refresh", "REFRESH")
    btn_refresh.custom_width = 72
    btn_refresh.onPressed(lambda: self.model._bg(self.model.refresh))

    # Outliner
    self.outliner = self.left_vpack.makeChild(
      uiclass=lev2.ui.Outliner, args=["outliner"])
    self.outliner.bgcolor = vec4(0.10, 0.10, 0.12, 1)
    self.outliner.item_height = 22
    self.left_vpack.fill_widget = self.outliner

    self.outliner_model = CatalogOutlinerModel(self.model)
    self.outliner.model = self.outliner_model
    self.outliner.onSelect(self._on_select)

    # Wire model scan callback to reset outliner
    orig_on_scan = self.model.on_scan_complete
    def on_scan():
      self.outliner_model.notifyModelReset()
      if orig_on_scan:
        orig_on_scan()
    self.model.on_scan_complete = on_scan

  ############################################################################
  # GPU init
  ############################################################################

  def _onGpuInit(self, ctx):
    self.uicontext = self.ezapp.uicontext
    base_db = lev2.ui.createDefaultStyleDatabase()
    custom_db = lev2.ui.StyleDatabase.createChild(base_db)
    self.uicontext.theme_engine = lev2.ui.ThemeEngine(custom_db)

    self.detail_view = CanvasDetailView(self.canvas)
    self.detail_view.gpuInit(ctx)
    self.canvas_ready = True

    self.model.init()

  ############################################################################
  # Selection
  ############################################################################

  def _on_select(self, key):
    if self._selecting:
      return
    self._selecting = True
    self.selected_key = key
    self.canvas_dirty = True
    self._selecting = False
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

  ############################################################################
  # Canvas event handling
  ############################################################################

  def _on_canvas_event(self, ev):
    result = self.detail_view.handleCanvasEvent(self.canvas, ev)
    if result == "__hover_changed__":
      self.canvas_dirty = True
      return lev2.ui.HandlerResult()
    if result:
      self._on_button(result)
      res = lev2.ui.HandlerResult()
      res.setHandler(self.canvas)
      return res
    return lev2.ui.HandlerResult()

  def _on_button(self, name):
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
        m._bg(lambda: m.upload(fqid))
      elif kt == "namespace" and ns:
        m._bg(lambda: m.upload_namespace(ns))
    elif name == "CANCEL":
      if kt == "asset" and fqid:
        m.cancel_fetch(fqid)
    elif name == "VERIFY HASH":
      if kt == "asset" and fqid:
        m._bg(lambda: m.verify_hashes(fqid))
    elif name == "CLEAR LOCAL":
      if kt == "asset" and fqid:
        m._bg(lambda: m.clear_local(fqid))
    elif name == "IC_BACK":
      self.selected_import_config = None
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
    elif name == "IC_NEW":
      project = m.key_to_project(key) if key else None
      if project:
        basename = m.create_import_config(project)
        if basename:
          self.selected_import_config = basename
          self.canvas_dirty = True
    elif name.startswith("AUTOCORRECT:"):
      ic_name = name[12:]
      m.autocorrect_import_config(ic_name)
    elif name.startswith("IC:"):
      ic_name = name[3:]
      self.selected_import_config = ic_name
      m.import_output_lines = []
      self.canvas_dirty = True

  def _open_import_config_editor(self, ic_name):
    self.ic_editor = ImportConfigEditor(self.uicontext, self.model, self.ezapp)
    self.ic_editor.on_close = lambda: setattr(self, 'canvas_dirty', True)
    self.ic_editor.open(ic_name)

  ############################################################################
  # Update loop
  ############################################################################

  def _onUpdate(self, updinfo):
    self.model.poll()
    if self.canvas_dirty and self.canvas_ready:
      self._redraw_canvas()

  ############################################################################
  # Canvas drawing
  ############################################################################

  def _redraw_canvas(self):
    self.canvas_dirty = False
    dv = self.detail_view
    m = self.model
    dv.beginRedraw()

    w = max(self.canvas.width, 400)
    h = max(self.canvas.height, 200)

    y = 4
    y = dv.drawCdnHealthHeader(m, y, w)
    y = dv.drawDownloadManagerStatus(m, y)

    # Import config detail mode takes over the view
    if self.selected_import_config:
      self._draw_import_config_full_view(self.selected_import_config, y, w, h)
      self.canvas.markDirty()
      return

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
  # Project view
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

    # Import configs
    icfiles = m.project_import_configs.get(project, [])
    y += 4
    dv.drawSectionHeader("Import Configs", y, w, h)
    dv.addButton("IC_NEW", w - 56, y, 48, 16, "NEW")
    y += 18
    for ic in icfiles:
      if y > h - 40:
        dv._texts["label"].addItem(f"... {len(icfiles)} total", vec2(8, y))
        y += 16
        break
      btn_name = f"IC:{ic}"
      dv.addButton(btn_name, 8, y, 20, 16, ">")
      dv._texts["label"].addItem(ic, vec2(32, y))
      y += 18
    if not icfiles:
      dv._texts["label"].addItem("(none)", vec2(8, y))
      y += 16
    y += 4

    # Namespace summary grid
    dv.drawSectionHeader("Namespaces", y, w, h)
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
  # Namespace view
  ############################################################################

  def _draw_namespace_view(self, ns, y_start, w, h):
    dv = self.detail_view
    m = self.model
    y = y_start

    d = build_namespace_varmap(m, ns)
    dv.drawSectionHeader("Namespace", y, w, h)
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
    if ns_ics:
      y += 4
      dv.drawSectionHeader("Import Configs", y, w, h)
      y += 18
      for ic in ns_ics:
        btn_name = f"IC:{ic}"
        dv.addButton(btn_name, 8, y, 20, 16, ">")
        dv._texts["label"].addItem(ic, vec2(32, y))
        y += 18
    y += 8

    # Compute status for ghosting
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

    # Asset grid
    assets = m.ns_assets.get(ns, [])
    if not assets:
      dv._texts["label"].addItem("(no assets)", vec2(8, y))
      return

    dv.drawSectionHeader("Assets", y, w, h)
    y += 18

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
  # Asset view
  ############################################################################

  def _draw_asset_view(self, fqid, y_start, w, h):
    dv = self.detail_view
    m = self.model
    y = y_start
    entry = m.asset_entries.get(fqid)
    if not entry:
      dv._texts["label"].addItem("No entry", vec2(8, y))
      return

    d = build_asset_varmap(m, fqid)
    dv.drawSectionHeader("Asset", y, w, h)
    y += 18
    for key in ["ID", "Manifest", "Type", "Platforms", "Priority"]:
      if key in d:
        dv.drawPropRow(key, d[key], y)
        y += 16
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

    # Buttons
    ns = fqid.split("|")[0]
    can_fetch = m.ns_can_fetch(ns)
    can_upload = m.ns_can_upload(ns)
    cs = m.chunk_status.get(fqid, [])
    cdn = m.cdn_status.get(fqid, [])
    fetch_ghost = (bool(cs) and all(cs)) or not can_fetch
    upload_ghost = (bool(cdn) and all(cdn)) or not can_upload
    is_fetching = fqid in m.active_fetches

    bx = 8
    dv.addButton("FETCH", bx, y, 56, 22, "FETCH", ghost=fetch_ghost or is_fetching)
    bx += 64
    dv.addButton("CANCEL", bx, y, 64, 22, "CANCEL", ghost=not is_fetching, danger=is_fetching)
    bx += 72
    dv.addButton("UPLOAD", bx, y, 64, 22, "UPLOAD", ghost=upload_ghost)
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

    # Chunk grid
    cell_w = 12
    cells_per_row = max(1, (w - 60) // cell_w)
    grid_x = 50

    # LOC row
    y = dv.drawChunkGrid(cs, vs, cdn, fqid in m.active_fetches,
                         grid_x, y, cell_w, h, cells_per_row, "LOC")

    # CDN row
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
  # Import Config views
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
    dv = self.detail_view
    m = self.model
    y = y_start
    data = m.load_import_config(ic_name)
    path = m.import_config_paths.get(ic_name, "")

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

    # Action buttons
    dv.addButton("IC_EDIT", 8, y, 56, 22, "EDIT")
    bx = 72
    dv.addButton("IC_IMPORT", bx, y, 80, 22, "IMPORT")
    bx += 88
    dv.addButton("IC_DRY_RUN", bx, y, 80, 22, "DRY RUN")
    bx += 88
    dv.addButton("IC_LIST", bx, y, 80, 22, "LIST FILES")
    y += 30

    # Output
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
