#!/usr/bin/env ork.python
################################################################################
# Orkid Asset Catalog GUI Tool
################################################################################

import os, re, time, threading, requests, urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
from orkengine import core
from orkengine import lev2
from ork import assets as ork_assets
from ork.app.application import ComponentizedApplication

tokens = core.CrcStringProxy()
vec2 = core.vec2
vec3 = core.vec3
vec4 = core.vec4

################################################################################

def format_size(nbytes):
  if nbytes == 0:
    return "0 B"
  units = ['B', 'KB', 'MB', 'GB', 'TB']
  idx = 0
  size = float(nbytes)
  while size >= 1024.0 and idx < len(units) - 1:
    size /= 1024.0
    idx += 1
  return f"{int(size)} {units[idx]}" if idx == 0 else f"{size:.1f} {units[idx]}"

################################################################################
# Outliner model — namespace / asset tree
################################################################################

def _derive_project_name(source_file):
  """Extract project name from manifest source_file path."""
  # e.g. /Users/.../imp/cap4d/obt.project/asset_manifests/cap4d.json → cap4d
  # e.g. /Users/.../orkid/ork.data/asset_manifests/singularity.json → orkid
  parts = source_file.replace("\\", "/").split("/")
  for i, p in enumerate(parts):
    if p == "asset_manifests" and i >= 2:
      parent = parts[i - 1]
      grandparent = parts[i - 2]
      if parent in ("obt.project", "ork.data"):
        return grandparent
      return parent
  return "unknown"


class CatalogOutlinerModel(lev2.ui.OutlinerModel):
  def __init__(self, tool):
    super().__init__()
    self.tool = tool
    self.allow_rename = False
    self.allow_delete = False
    self.allow_add = False

  def getChildren(self, parent_key):
    if parent_key == "":
      return self.tool.projects
    if parent_key in self.tool.project_namespaces:
      # Project → list of "project/namespace" keys
      return [f"{parent_key}/{ns}" for ns in self.tool.project_namespaces[parent_key]]
    if "/" in parent_key and "|" not in parent_key:
      # "project/namespace" → list of "project/ns|asset" keys
      ns = parent_key.split("/", 1)[1]
      return [f"{parent_key.split('/')[0]}/{fqid}" for fqid in self.tool.ns_assets.get(ns, [])]
    return []

  def getDisplayName(self, key):
    if "|" in key:
      # "project/ns|asset" → asset name
      return key.split("|", 1)[1]
    if "/" in key:
      # "project/namespace" → namespace name
      return key.split("/", 1)[1]
    return key

  def hasChildren(self, key):
    return "|" not in key

################################################################################
# PropertySheet helpers — use VarMapPropertyModel (C++ impl, no trampoline)
################################################################################

def _build_namespace_varmap(tool, ns_id):
  """Build a flat dict of namespace properties for VarMapPropertyModel."""
  d = {}
  d["Name"] = ns_id
  d["Asset Count"] = str(len(tool.ns_assets.get(ns_id, [])))

  merged = tool.cfgspc.merged_config
  ek = merged.getEncryptionKeyForNamespace(ns_id) if merged else None
  d["Encryption Key"] = "Yes" if ek else "No"

  remote_loc = tool.cfgspc.getNamespaceRemoteLocation(ns_id) or ""
  d["Remote Location"] = remote_loc

  if remote_loc and merged:
    resolved = merged.resolveRemoteLocation(remote_loc)
    if resolved:
      d["Download URL"] = str(resolved.download_url)
      d["Upload URL"] = str(resolved.upload_url)
      d["TLS Verify"] = "Disabled" if resolved.disable_cert_check else "Enabled"
      # API key read status
      akr = resolved.api_key_read if hasattr(resolved, 'api_key_read') else None
      if akr == "<PasswordAuthentication>":
        d["API Key (read)"] = "password auth"
      elif akr:
        d["API Key (read)"] = "Yes"
      else:
        d["API Key (read)"] = "No"
      # API key write status
      akw = resolved.api_key_write if hasattr(resolved, 'api_key_write') else None
      if akw == "<PasswordAuthentication>":
        d["API Key (write)"] = "password auth"
      elif akw:
        d["API Key (write)"] = "Yes"
      else:
        d["API Key (write)"] = "No"

  total, present = 0, 0
  total_sz, local_sz = 0, 0
  for fqid in tool.ns_assets.get(ns_id, []):
    cs = tool.chunk_status.get(fqid, [])
    total += len(cs)
    present += sum(1 for x in cs if x)
    entry = tool.asset_entries.get(fqid)
    if entry:
      total_sz += entry.archive_size
      if cs and all(cs):
        local_sz += entry.archive_size
  d["Chunks Local"] = f"{present} / {total}"
  d["Size Local"] = f"{format_size(local_sz)} / {format_size(total_sz)}"

  # Look up CDN health by hostname
  cdn_url = d.get("Download URL", "")
  if cdn_url:
    from urllib.parse import urlparse
    host = urlparse(cdn_url).hostname or ""
    if host in tool.cdn_health:
      h = tool.cdn_health[host]
      d["CDN Endpoint"] = h.get("url", "")
      d["CDN Reachable"] = "Yes" if h.get("reachable") else "No"
      ms = h.get("latency_ms", 0)
      d["CDN Latency"] = f"{ms:.0f}ms" if h.get("reachable") else "N/A"
    else:
      d["CDN Reachable"] = "unreachable"
      d["CDN Latency"] = "N/A"

  return d


def _build_asset_varmap(tool, fqid):
  """Build a flat dict of asset properties for VarMapPropertyModel."""
  d = {}
  entry = tool.asset_entries.get(fqid)
  if not entry:
    d["ID"] = fqid
    return d

  d["ID"] = fqid
  d["Manifest"] = tool.asset_manifest_file.get(fqid, "unknown")
  d["Type"] = entry.type or "unknown"
  d["Platforms"] = ", ".join(entry.platforms) if entry.platforms else "all"
  d["Priority"] = str(entry.priority)
  d["Archive Size"] = format_size(entry.archive_size)
  d["Compressed Size"] = format_size(entry.compressed_size)
  d["Encrypted Size"] = format_size(entry.encrypted_size)
  if entry.archive_size > 0:
    r = 100.0 - (entry.compressed_size / entry.archive_size * 100.0)
    d["Compression"] = f"{r:.1f}%"

  if hasattr(entry, 'content_hash') and entry.content_hash:
    d["Content Hash"] = str(entry.content_hash)
  if hasattr(entry, 'storage_hash') and entry.storage_hash:
    d["Storage Hash"] = str(entry.storage_hash)
  if hasattr(entry, 'hash_algorithm') and entry.hash_algorithm:
    d["Hash Algorithm"] = str(entry.hash_algorithm)

  cm = entry.chunk_manifest if hasattr(entry, 'chunk_manifest') else None

  if cm and hasattr(cm, 'file_hash') and cm.file_hash:
    d["File Hash (expected)"] = f"{cm.file_hash:016x}"
  if hasattr(entry, 'storage_hash') and entry.storage_hash:
    cache_dir = tool.catalog.cache_dir
    enc_path = os.path.join(cache_dir, 'enc', f"{entry.storage_hash}.enc")
    d["Encrypted File"] = "Yes" if os.path.exists(enc_path) else "No"
  cm = entry.chunk_manifest if hasattr(entry, 'chunk_manifest') else None
  if cm:
    d["Total Chunks"] = str(len(cm.chunks))
    if hasattr(cm, 'chunk_size'):
      d["Chunk Size"] = format_size(cm.chunk_size)

  cs = tool.chunk_status.get(fqid, [])
  total = len(cs)
  present = sum(1 for x in cs if x)
  d["Chunks Local"] = f"{present} / {total}"

  vs = tool.chunk_valid.get(fqid, [])
  valid = sum(1 for x in vs if x)
  d["Chunks Valid"] = f"{valid} / {len(vs)}"

  if hasattr(entry, 'local_loc') and entry.local_loc:
    d["Local Location"] = str(entry.local_loc)
    merged = tool.cfgspc.merged_config
    if merged:
      resolved_path = merged.resolveLocalPath(entry.local_loc)
      if resolved_path:
        d["Local Path"] = str(resolved_path)
        d["Local Exists"] = "Yes" if os.path.exists(str(resolved_path)) else "No"
  elif hasattr(entry, 'resolved_local_path') and entry.resolved_local_path:
    d["Local Path"] = str(entry.resolved_local_path)
    d["Local Exists"] = "Yes" if os.path.exists(str(entry.resolved_local_path)) else "No"

  ns = fqid.split("|")[0]
  remote_loc = tool.cfgspc.getNamespaceRemoteLocation(ns)
  if remote_loc:
    merged = tool.cfgspc.merged_config
    resolved = merged.resolveRemoteLocation(remote_loc) if merged else None
    if resolved:
      d["Remote URL"] = str(resolved.download_url)

  return d

################################################################################
# Main application
################################################################################

class CatalogTool(ComponentizedApplication):

  def __init__(self):
    super().__init__(profiler_channels=[])
    # data model
    self.namespaces = []
    self.ns_assets = {}
    self.asset_entries = {}
    self.chunk_status = {}
    self.chunk_valid = {}
    self.cdn_status = {}
    self.cdn_health = {}
    self.active_fetches = {}
    self.hash_verify_results = {}  # fqid -> "OK" / "MISMATCH ..." / "verifying..."
    self.http_session = requests.Session()
    self.cdn_ping_time = 0  # last ping timestamp
    self.cdn_ping_interval = 1.0
    self.cdn_ping_running = False
    self.selected_key = None
    self.selected_import_config = None  # basename of selected import config
    self._selecting = False
    self.canvas_dirty = True
    self.canvas_ready = False
    self.buttons = {}
    self.hover_button = None
    self.createEzApp(width=1200, height=800, fullscreen=False, name="Asset Catalog Tool")

  ############################################################################
  # UI init
  ############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.12, 0.12, 0.14, 1)

    # Main content area — start with a fill panel for the right side
    right_dock_item = lg.makeChild(
      fill=True, margin=2,
      uiclass=lev2.ui.DockablePanel, args=["details_dock"])
    self.right_dock = right_dock_item.widget
    self.right_dock.titlebar_color = vec4(0.18, 0.18, 0.22, 1)
    self.right_dock.title_color = vec4(0.7, 0.7, 0.8, 1)
    self.right_dock.title_override = "Details"
    self.right_dock.title_center = True

    # Single PrimCanvas fills the right dock — renders both details and status
    self.canvas = self.right_dock.createChild(
      uiclass=lev2.ui.PrimCanvas, args=["detail_canvas"])
    self.canvas.supersample = 0
    self.canvas.bg_color = vec4(0.08, 0.08, 0.10, 1)
    self.canvas.draw_background = True
    self.canvas.onUiEvent = self._on_canvas_event

    # Left panel: split from right — Outliner
    left_dock_item = lg.split(
      layout=right_dock_item.layout,
      proportion=0.30, placement=tokens.LEFT, margin=2,
      uiclass=lev2.ui.DockablePanel, args=["catalog_dock"])
    self.left_dock = left_dock_item.widget
    self.left_dock.titlebar_color = vec4(0.18, 0.15, 0.20, 1)
    self.left_dock.title_color = vec4(0.8, 0.7, 0.9, 1)
    self.left_dock.title_override = "Catalog"
    self.left_dock.title_center = True

    # Left dock contents: VPack with toolbar + outliner
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
    btn_refresh.onPressed(lambda: self._bg(self._do_refresh))

    # Outliner
    self.outliner = self.left_vpack.makeChild(
      uiclass=lev2.ui.Outliner, args=["outliner"])
    self.outliner.bgcolor = vec4(0.10, 0.10, 0.12, 1)
    self.outliner.item_height = 22
    self.left_vpack.fill_widget = self.outliner

    self.outliner_model = CatalogOutlinerModel(self)
    self.outliner.model = self.outliner_model
    self.outliner.onSelect(self._on_select)

  ############################################################################
  # GPU init
  ############################################################################

  def _onGpuInit(self, ctx):
    self.uicontext = self.ezapp.uicontext
    base_db = lev2.ui.createDefaultStyleDatabase()
    custom_db = lev2.ui.StyleDatabase.createChild(base_db)
    self.uicontext.theme_engine = lev2.ui.ThemeEngine(custom_db)

    # Canvas GPU resources
    self.canvas.gpuInit(ctx)
    self.canvas_font = lev2.FontManager.fontForId("i14")
    self.canvas_font_sm = lev2.FontManager.fontForId("i12")
    self.canvas_layer = self.canvas.createLayer("status")

    # Quad primitives
    self.bg_prim = lev2.ui.QuadPrimitive(pipeline=self.canvas.pipelineSolid)
    self.canvas_layer.addPrimitive(self.bg_prim)

    # Text primitives by color role
    self._texts = {}
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

    # Small text for chunk grid
    self.text_sm = lev2.ui.TextPrimitive(font=self.canvas_font_sm, color=vec4(0.7, 0.7, 0.7, 1))
    self.canvas_layer.addPrimitive(self.text_sm)

    self.canvas_ready = True

    # Initialize catalog
    self.cfgspc, self.catalog = ork_assets.default_cfg_and_catalog()
    self._full_scan()
    self._bg(self._check_cdn_health)

    # Disable DOWNLOAD log channel — progress is shown visually
    logger = core.Logger.instance()
    dl_chan = logger.getChannel("DOWNLOAD")
    if dl_chan:
      dl_chan.enabled = False

  ############################################################################
  # Data scanning
  ############################################################################

  def _full_scan(self):
    self.namespaces = sorted(self.catalog.list_namespaces("*"))
    self.ns_assets = {}
    self.asset_entries = {}
    self.chunk_status = {}
    self.chunk_valid = {}
    # Build project → namespace mapping from manifest source files
    self.project_namespaces = {}    # project_name -> sorted list of ns_ids
    self.project_manifests = {}     # project_name -> sorted list of manifest file basenames
    self.project_import_configs = {} # project_name -> sorted list of import config basenames
    self.import_config_paths = {}   # basename -> full path
    self.import_config_data = {}    # basename -> parsed dict (loaded on demand)
    self.ns_project = {}            # ns_id -> project_name
    self.ns_manifests = {}          # ns_id -> sorted list of manifest file basenames
    self.ns_import_configs = {}     # ns_id -> sorted list of import config basenames
    self.asset_manifest_file = {}   # fqid -> manifest source file basename
    for ns in self.namespaces:
      manifests = self.catalog.manifestsForNamespace(ns)
      project = "unknown"
      ns_files = []
      ns_import_configs = []
      for m in manifests:
        sf = m.source_file
        if sf:
          if project == "unknown":
            project = _derive_project_name(sf)
          basename = os.path.basename(sf)
          # Real manifests have assets with storage hashes
          assets = m.assets
          is_real = assets and any(hasattr(assets[k], 'storage_hash') and assets[k].storage_hash for k in assets)
          if is_real:
            ns_files.append(basename)
            for asset_id in assets.keys():
              self.asset_manifest_file[f"{ns}|{asset_id}"] = basename
          else:
            ns_import_configs.append(basename)
            self.import_config_paths[basename] = sf
      self.ns_project[ns] = project
      self.ns_manifests[ns] = sorted(set(ns_files))
      self.ns_import_configs[ns] = sorted(set(ns_import_configs))
      self.project_namespaces.setdefault(project, []).append(ns)
      self.project_manifests.setdefault(project, set()).update(ns_files)
      self.project_import_configs.setdefault(project, set()).update(ns_import_configs)
    for proj in self.project_namespaces:
      self.project_namespaces[proj].sort()
      self.project_manifests[proj] = sorted(self.project_manifests[proj])
      self.project_import_configs[proj] = sorted(self.project_import_configs.get(proj, set()))
    self.projects = sorted(self.project_namespaces.keys())

    for ns in self.namespaces:
      fqids = sorted(self.catalog.list_assets(f"{ns}|*"))
      self.ns_assets[ns] = fqids
      for fqid in fqids:
        entry = self.catalog.findAssetEntry(fqid)
        self.asset_entries[fqid] = entry
        self._scan_chunks(fqid, entry)
    self.outliner_model.notifyModelReset()

  def _scan_chunks(self, fqid, entry):
    if not entry or not hasattr(entry, 'chunk_manifest') or not entry.chunk_manifest:
      self.chunk_status[fqid] = []
      self.chunk_valid[fqid] = []
      return
    cm = entry.chunk_manifest
    cache_dir = self.catalog.cache_dir
    chunks_dir = os.path.join(cache_dir, 'enc', 'chunks')
    present = []
    valid = []
    for i, chunk in enumerate(cm.chunks):
      chunk_file = f"{entry.storage_hash}.chunk.{i:04d}"
      chunk_path = os.path.join(chunks_dir, chunk_file)
      exists = os.path.exists(chunk_path)
      present.append(exists)
      hash_ok = False
      if exists:
        try:
          from orkengine.core import xxhash64_chunk
          with open(chunk_path, 'rb') as f:
            data = f.read()
          computed = xxhash64_chunk(data)
          hash_ok = (computed == chunk.hash)
        except Exception:
          pass
      valid.append(hash_ok)
    self.chunk_status[fqid] = present
    self.chunk_valid[fqid] = valid

  def _scan_chunks_fast(self, fqid, entry):
    """Presence-only check — no hash verification. Used during active downloads."""
    if not entry or not hasattr(entry, 'chunk_manifest') or not entry.chunk_manifest:
      return
    cm = entry.chunk_manifest
    chunks_dir = os.path.join(self.catalog.cache_dir, 'enc', 'chunks')
    present = []
    for i in range(len(cm.chunks)):
      chunk_path = os.path.join(chunks_dir, f"{entry.storage_hash}.chunk.{i:04d}")
      present.append(os.path.exists(chunk_path))
    self.chunk_status[fqid] = present

  ############################################################################
  # CDN health check
  ############################################################################

  def _check_cdn_health(self):
    """Ping each unique CDN host, dedup by hostname."""
    from urllib.parse import urlparse
    health = {}  # hostname -> entry
    seen_hosts = set()
    for ns in self.namespaces:
      remote_loc = self.cfgspc.getNamespaceRemoteLocation(ns)
      if not remote_loc:
        continue
      merged = self.cfgspc.merged_config
      if not merged:
        continue
      resolved = merged.resolveRemoteLocation(remote_loc)
      if not resolved:
        continue
      url = str(resolved.download_url)
      host = urlparse(url).hostname or url
      if host in seen_hosts:
        continue
      seen_hosts.add(host)
      # Resolve IP safely
      import socket
      try:
        resolved_ip = socket.gethostbyname(host)
      except Exception:
        resolved_ip = "unresolvable"
      try:
        t0 = time.time()
        r = self.http_session.head(url, timeout=5, verify=not resolved.disable_cert_check)
        latency = (time.time() - t0) * 1000
        entry = {
          "reachable": r.status_code < 500,
          "latency_ms": latency,
          "url": url,
          "host": host,
          "location": remote_loc,
          "ip": resolved_ip,
        }
      except Exception:
        entry = {"reachable": False, "latency_ms": 0, "url": url, "host": host, "location": remote_loc, "ip": resolved_ip}
      health[host] = entry
    self.cdn_health = health
    self.cdn_ping_time = time.time()
    self.cdn_ping_running = False
    self.canvas_dirty = True

  ############################################################################
  # Key parsing — keys are "project", "project/ns", or "project/ns|asset"
  ############################################################################

  def _key_type(self, key):
    """Return 'project', 'namespace', or 'asset'."""
    if not key:
      return None
    if "|" in key:
      return "asset"
    if "/" in key:
      return "namespace"
    return "project"

  def _key_to_ns(self, key):
    """Extract namespace id from a namespace or asset key."""
    if "|" in key:
      # "project/ns|asset" → "ns"
      return key.split("/", 1)[1].split("|")[0] if "/" in key else key.split("|")[0]
    if "/" in key:
      # "project/ns" → "ns"
      return key.split("/", 1)[1]
    return None

  def _key_to_fqid(self, key):
    """Extract fqid (ns|asset) from an asset key."""
    if "|" in key and "/" in key:
      # "project/ns|asset" → "ns|asset"
      return key.split("/", 1)[1]
    if "|" in key:
      return key
    return None

  def _key_to_project(self, key):
    """Extract project name from any key."""
    if "/" in key:
      return key.split("/", 1)[0]
    if "|" not in key:
      return key
    return None

  def _get_import_config(self, basename):
    """Load and cache import config JSON data."""
    if basename in self.import_config_data:
      return self.import_config_data[basename]
    path = self.import_config_paths.get(basename)
    if not path or not os.path.exists(path):
      return None
    try:
      import json
      with open(path, 'r') as f:
        data = json.load(f)
      self.import_config_data[basename] = data
      return data
    except Exception:
      return None

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
    kt = self._key_type(key)
    if kt == "asset":
      fqid = self._key_to_fqid(key)
      if fqid and fqid not in self.cdn_status:
        self._bg(lambda: self._verify_cdn_asset(fqid))
    elif kt == "namespace":
      ns = self._key_to_ns(key)
      if ns:
        unchecked = [f for f in self.ns_assets.get(ns, []) if f not in self.cdn_status]
        if unchecked:
          self._bg(lambda fqids=unchecked: self._verify_cdn_batch(fqids))

  ############################################################################
  # Background helper
  ############################################################################

  def _bg(self, fn):
    t = threading.Thread(target=fn, daemon=True)
    t.start()

  ############################################################################
  # Refresh
  ############################################################################

  def _do_refresh(self):
    self._full_scan()
    self._check_cdn_health()
    self.canvas_dirty = True

  ############################################################################
  # Canvas event handling
  ############################################################################

  def _on_canvas_event(self, ev):
    handled = False
    if ev.code == tokens.PUSH.hashed:
      mx, my = self.canvas.rootToLocal(ev.x, ev.y)
      for name, (bx, by, bw, bh) in self.buttons.items():
        if bx <= mx < bx + bw and by <= my < by + bh:
          self._on_button(name)
          handled = True
          break
    elif ev.code == tokens.MOVE.hashed:
      mx, my = self.canvas.rootToLocal(ev.x, ev.y)
      old_hover = self.hover_button
      self.hover_button = None
      for name, (bx, by, bw, bh) in self.buttons.items():
        if bx <= mx < bx + bw and by <= my < by + bh:
          self.hover_button = name
          break
      if self.hover_button != old_hover:
        self.canvas_dirty = True

    if handled:
      res = lev2.ui.HandlerResult()
      res.setHandler(self.canvas)
      return res
    return lev2.ui.HandlerResult()

  def _on_button(self, name):
    key = self.selected_key
    if not key:
      return
    kt = self._key_type(key)
    fqid = self._key_to_fqid(key)
    ns = self._key_to_ns(key)
    if name == "FETCH":
      if kt == "asset" and fqid:
        self._do_fetch(fqid)
    elif name == "FETCH ALL":
      if kt == "namespace" and ns:
        self._do_fetch_namespace(ns)
    elif name == "UPLOAD":
      if kt == "asset" and fqid:
        self._bg(lambda: self._do_upload_asset(fqid))
      elif kt == "namespace" and ns:
        self._bg(lambda: self._do_upload_namespace(ns))
    elif name == "CANCEL":
      if kt == "asset" and fqid:
        self._do_cancel_fetch(fqid)
    elif name == "VERIFY HASH":
      if kt == "asset" and fqid:
        self._bg(lambda: self._do_verify_hashes(fqid))
    elif name == "CLEAR LOCAL":
      if kt == "asset" and fqid:
        self._bg(lambda: self._do_clear_local(fqid))
    elif name.startswith("AUTOCORRECT:"):
      ic_name = name[12:]
      self._do_autocorrect_import_config(ic_name)
    elif name.startswith("IC:"):
      ic_name = name[3:]
      # Toggle — click again to collapse
      if self.selected_import_config == ic_name:
        self.selected_import_config = None
      else:
        self.selected_import_config = ic_name
      self.canvas_dirty = True

  ############################################################################
  # Actions
  ############################################################################

  def _do_cancel_fetch(self, fqid):
    req = self.active_fetches.get(fqid)
    if req:
      req.cancel()
      # Remove immediately so a new fetch can start cleanly
      del self.active_fetches[fqid]
      # Invalidate so a fresh fetch can be started later
      self.catalog.invalidateRequest(fqid)
      # Rescan chunks (keep whatever was downloaded)
      entry = self.asset_entries.get(fqid)
      if entry:
        self._scan_chunks(fqid, entry)
      self._last_fetch_snap = None
      self.canvas_dirty = True

  def _do_fetch(self, fqid):
    fetch_req = self.catalog.fetchAsync(fqid)
    self.active_fetches[fqid] = fetch_req
    self._last_fetch_snap = None
    self.canvas_dirty = True

  def _do_fetch_namespace(self, ns):
    for fqid in self.ns_assets.get(ns, []):
      fetch_req = self.catalog.fetchAsync(fqid)
      self.active_fetches[fqid] = fetch_req
    self._last_fetch_snap = None
    self.canvas_dirty = True

  def _do_verify_hashes(self, fqid):
    """Verify assembled file hash from chunks on disk."""
    from orkengine.core import xxhash64_chunk
    self.hash_verify_results[fqid] = "verifying..."
    self.canvas_dirty = True
    entry = self.asset_entries.get(fqid)
    if not entry:
      self.hash_verify_results[fqid] = "no entry"
      self.canvas_dirty = True
      return
    cm = entry.chunk_manifest if hasattr(entry, 'chunk_manifest') else None
    if not cm or not hasattr(cm, 'file_hash') or not cm.file_hash:
      self.hash_verify_results[fqid] = "no file hash in manifest"
      self.canvas_dirty = True
      return
    cache_dir = self.catalog.cache_dir
    chunks_dir = os.path.join(cache_dir, 'enc', 'chunks')
    all_data = bytearray()
    for i in range(len(cm.chunks)):
      cp = os.path.join(chunks_dir, f"{entry.storage_hash}.chunk.{i:04d}")
      if not os.path.exists(cp):
        self.hash_verify_results[fqid] = f"incomplete (chunk {i} missing)"
        self.canvas_dirty = True
        return
      with open(cp, 'rb') as f:
        all_data.extend(f.read())
    try:
      computed = xxhash64_chunk(bytes(all_data))
      if computed == cm.file_hash:
        self.hash_verify_results[fqid] = "OK"
      else:
        self.hash_verify_results[fqid] = f"MISMATCH (got {computed:016x})"
    except Exception as e:
      self.hash_verify_results[fqid] = f"error: {e}"
    self.canvas_dirty = True

  def _do_clear_local(self, fqid):
    """Delete local cached chunks and dearchived files for an asset."""
    import shutil
    entry = self.asset_entries.get(fqid)
    if not entry:
      return
    cache_dir = self.catalog.cache_dir

    # Delete chunks
    if hasattr(entry, 'chunk_manifest') and entry.chunk_manifest:
      chunks_dir = os.path.join(cache_dir, 'enc', 'chunks')
      for i in range(len(entry.chunk_manifest.chunks)):
        chunk_file = f"{entry.storage_hash}.chunk.{i:04d}"
        chunk_path = os.path.join(chunks_dir, chunk_file)
        if os.path.exists(chunk_path):
          os.remove(chunk_path)

    # Delete encrypted file
    if hasattr(entry, 'storage_hash') and entry.storage_hash:
      enc_path = os.path.join(cache_dir, 'enc', f"{entry.storage_hash}.enc")
      if os.path.exists(enc_path):
        os.remove(enc_path)

    # Delete dearchived local path
    if hasattr(entry, 'resolved_local_path') and entry.resolved_local_path:
      local_path = str(entry.resolved_local_path)
      if os.path.isdir(local_path):
        shutil.rmtree(local_path)
      elif os.path.isfile(local_path):
        os.remove(local_path)

    # Invalidate cached fetch request so it can be re-fetched
    self.catalog.invalidateRequest(fqid)
    # Rescan and refresh
    self._scan_chunks(fqid, entry)
    self.canvas_dirty = True

  def _do_upload_asset(self, fqid):
    try:
      self.catalog.uploadAsset(fqid)
    except Exception as e:
      print(f"Upload error: {e}")
    self.canvas_dirty = True

  def _do_upload_namespace(self, ns):
    try:
      self.catalog.uploadNamespace(ns)
    except Exception as e:
      print(f"Upload error: {e}")
    self.canvas_dirty = True

  def _verify_cdn_asset(self, fqid):
    entry = self.asset_entries.get(fqid)
    if not entry or not hasattr(entry, 'chunk_manifest') or not entry.chunk_manifest:
      return
    ns = fqid.split("|")[0]
    remote_loc = self.cfgspc.getNamespaceRemoteLocation(ns)
    if not remote_loc:
      return
    merged = self.cfgspc.merged_config
    resolved = merged.resolveRemoteLocation(remote_loc)
    if not resolved:
      return
    cm = entry.chunk_manifest
    chunk_requests = []
    for i, chunk in enumerate(cm.chunks):
      chunk_requests.append({
        'file': f"{entry.storage_hash}.chunk.{i:04d}",
        'expected_hash': f"{chunk.hash:016x}",
      })
    download_url = str(resolved.download_url)
    endpoint_match = re.search(r'/([^/]+)/download/?$', download_url)
    if not endpoint_match:
      return
    endpoint = endpoint_match.group(1)
    base_url = download_url.rsplit('/', 2)[0]
    verify_url = f"{base_url}/api/{endpoint}/verify"
    headers = {'Content-Type': 'application/json'}
    if hasattr(resolved, 'api_key_read') and resolved.api_key_read:
      headers['X-API-Key'] = resolved.api_key_read
    try:
      response = self.http_session.post(
        verify_url, headers=headers,
        json={'chunks': chunk_requests},
        timeout=30, verify=not resolved.disable_cert_check)
      if response.status_code == 200:
        data = response.json()
        self.cdn_status[fqid] = [r['present'] for r in data['results']]
      else:
        self.cdn_status[fqid] = [False] * len(cm.chunks)
    except Exception:
      self.cdn_status[fqid] = [False] * len(cm.chunks)
    self.canvas_dirty = True

  def _verify_cdn_batch(self, fqids):
    """Verify multiple assets in a single POST per namespace (they share endpoint)."""
    # Group by namespace
    by_ns = {}
    for fqid in fqids:
      ns = fqid.split("|")[0]
      by_ns.setdefault(ns, []).append(fqid)

    for ns, ns_fqids in by_ns.items():
      remote_loc = self.cfgspc.getNamespaceRemoteLocation(ns)
      if not remote_loc:
        continue
      merged = self.cfgspc.merged_config
      resolved = merged.resolveRemoteLocation(remote_loc)
      if not resolved:
        continue
      download_url = str(resolved.download_url)
      endpoint_match = re.search(r'/([^/]+)/download/?$', download_url)
      if not endpoint_match:
        continue
      endpoint = endpoint_match.group(1)
      base_url = download_url.rsplit('/', 2)[0]
      verify_url = f"{base_url}/api/{endpoint}/verify"
      headers = {'Content-Type': 'application/json'}
      if hasattr(resolved, 'api_key_read') and resolved.api_key_read:
        headers['X-API-Key'] = resolved.api_key_read

      # Build one big chunk list, track boundaries per asset
      all_chunks = []
      boundaries = []  # (fqid, start_idx, count)
      for fqid in ns_fqids:
        entry = self.asset_entries.get(fqid)
        if not entry or not hasattr(entry, 'chunk_manifest') or not entry.chunk_manifest:
          self.cdn_status[fqid] = []
          continue
        cm = entry.chunk_manifest
        start = len(all_chunks)
        for i, chunk in enumerate(cm.chunks):
          all_chunks.append({
            'file': f"{entry.storage_hash}.chunk.{i:04d}",
            'expected_hash': f"{chunk.hash:016x}",
          })
        boundaries.append((fqid, start, len(cm.chunks)))

      if not all_chunks:
        continue

      try:
        response = self.http_session.post(
          verify_url, headers=headers,
          json={'chunks': all_chunks},
          timeout=60, verify=not resolved.disable_cert_check)
        if response.status_code == 200:
          results = response.json()['results']
          for fqid, start, count in boundaries:
            self.cdn_status[fqid] = [results[start + i]['present'] for i in range(count)]
        else:
          for fqid, start, count in boundaries:
            self.cdn_status[fqid] = [False] * count
      except Exception:
        for fqid, start, count in boundaries:
          self.cdn_status[fqid] = [False] * count

    self.canvas_dirty = True

  ############################################################################
  # Update loop
  ############################################################################

  def _onUpdate(self, updinfo):
    # Poll active fetches
    done = []
    any_changed = False
    for fqid, req in self.active_fetches.items():
      if req.completed:
        done.append(fqid)
        entry = self.asset_entries.get(fqid)
        if entry:
          self._scan_chunks(fqid, entry)
        any_changed = True
    for fqid in done:
      del self.active_fetches[fqid]
    if any_changed:
      self.canvas_dirty = True
    # Redraw when fetch progress changes, rescan chunk presence
    if self.active_fetches:
      new_snap = {}
      for fqid, req in self.active_fetches.items():
        new_snap[fqid] = (req.chunks_completed, req.bytes_downloaded)
      if new_snap != getattr(self, '_last_fetch_snap', None):
        self._last_fetch_snap = new_snap
        for fqid in self.active_fetches:
          entry = self.asset_entries.get(fqid)
          if entry:
            self._scan_chunks_fast(fqid, entry)
        self.canvas_dirty = True
    elif hasattr(self, '_last_fetch_snap') and self._last_fetch_snap:
      self._last_fetch_snap = None
      self.canvas_dirty = True

    # Periodic CDN health re-ping
    if self.canvas_ready and not self.cdn_ping_running:
      if time.time() - self.cdn_ping_time >= self.cdn_ping_interval:
        self.cdn_ping_running = True
        self._bg(self._check_cdn_health)

    if self.canvas_dirty and self.canvas_ready:
      self._redraw_canvas()

  def _ns_cdn_reachable(self, ns):
    """Check if namespace's CDN endpoint is reachable."""
    merged = self.cfgspc.merged_config
    if not merged:
      return False
    remote_loc = self.cfgspc.getNamespaceRemoteLocation(ns)
    if not remote_loc:
      return False
    resolved = merged.resolveRemoteLocation(remote_loc)
    if not resolved:
      return False
    from urllib.parse import urlparse
    host = urlparse(str(resolved.download_url)).hostname or ""
    h = self.cdn_health.get(host)
    return bool(h and h.get("reachable"))

  def _ns_can_fetch(self, ns):
    """Check if namespace has encryption key and CDN is reachable."""
    merged = self.cfgspc.merged_config
    if not merged:
      return False
    if not merged.getEncryptionKeyForNamespace(ns):
      return False
    return self._ns_cdn_reachable(ns)

  def _ns_can_upload(self, ns):
    """Check if namespace has encryption key, write API key, and CDN is reachable."""
    merged = self.cfgspc.merged_config
    if not merged:
      return False
    if not merged.getEncryptionKeyForNamespace(ns):
      return False
    if not self._ns_cdn_reachable(ns):
      return False
    remote_loc = self.cfgspc.getNamespaceRemoteLocation(ns)
    if not remote_loc:
      return False
    resolved = merged.resolveRemoteLocation(remote_loc)
    if not resolved:
      return False
    akw = resolved.api_key_write if hasattr(resolved, 'api_key_write') else None
    return bool(akw)

  ############################################################################
  # Canvas drawing
  ############################################################################

  def _redraw_canvas(self):
    self.canvas_dirty = False
    self.buttons = {}

    # Clear all primitives
    self.bg_prim.clearQuads()
    for tp in self._texts.values():
      tp.clearItems()
    self.text_sm.clearItems()

    w = max(self.canvas.width, 400)
    h = max(self.canvas.height, 200)

    # CDN health header bar — equal-width columns
    y = 4
    x = 8
    self._texts["cdn_hdr"].addItem("CDN:", vec2(x, y))
    cdn_x0 = 40
    n_cdn = len(self.cdn_health)
    if n_cdn > 0:
      col_w = (w - cdn_x0 - 8) // n_cdn
      for idx, (host, info) in enumerate(self.cdn_health.items()):
        cx = cdn_x0 + idx * col_w
        loc_name = info.get("location", host)
        label = f"{loc_name} ({host})"
        if info.get("reachable"):
          txt = f"{label} {info['latency_ms']:.0f}ms"
          self._texts["status_ok"].addItem(txt, vec2(cx, y))
        else:
          txt = f"{label} offline"
          self._texts["status_err"].addItem(txt, vec2(cx, y))
        # Second line: resolved IP
        ip = info.get("ip", "")
        if ip == "unresolvable":
          self._texts["status_err"].addItem(f"  ip: {ip}", vec2(cx, y + 14))
        elif ip:
          self._texts["label"].addItem(f"  ip: {ip}", vec2(cx, y + 14))

    y += 30

    # DownloadManager status
    dm = self.catalog.download_manager
    if dm:
      active = dm.active_download_count()
      pending = dm.pending_count
      completed = dm.completed_count
      failed = dm.failed_count
      total_mb = dm.total_bytes_downloaded / 1048576.0
      dm_txt = f"DL: active={active}  pending={pending}  completed={completed}  failed={failed}  total={total_mb:.1f}MB"
      self._texts["label"].addItem(dm_txt, vec2(8, y))
    y += 16

    key = self.selected_key
    if not key:
      self._texts["label"].addItem("Select a project, namespace, or asset", vec2(8, y))
      self.canvas.markDirty()
      return

    kt = self._key_type(key)
    if kt == "asset":
      fqid = self._key_to_fqid(key)
      self._draw_asset_view(fqid, y, w, h)
    elif kt == "namespace":
      ns = self._key_to_ns(key)
      self._draw_namespace_view(ns, y, w, h)
    elif kt == "project":
      self._draw_project_view(key, y, w, h)

    self.canvas.markDirty()

  def _add_button_quad(self, name, x, y, bw, bh, label, ghost=False, danger=False):
    if not ghost:
      self.buttons[name] = (x, y, bw, bh)
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
    qd.setPosition(x, max(self.canvas.height, 200) - y - bh)
    qd.setSize(bw, bh)
    qd.setColor(color)
    self.bg_prim.addQuad(qd)
    txt_role = "label" if ghost else "button"
    self._texts[txt_role].addItem(label, vec2(x + 6, y + 3))

  ############################################################################
  # Namespace view
  ############################################################################

  def _draw_prop_row(self, label, value, y, val_x=140):
    """Draw a label: value row."""
    self._texts["label"].addItem(f"{label}:", vec2(8, y))
    self._texts["value"].addItem(value, vec2(val_x, y))

  def _draw_section_header(self, title, y, w, h):
    """Draw a section header with background bar."""
    qd = lev2.ui.QuadData()
    qd.setPosition(4, h - y - 16)
    qd.setSize(w - 8, 16)
    qd.setColor(vec4(0.15, 0.15, 0.20, 1))
    self.bg_prim.addQuad(qd)
    self._texts["cdn_hdr"].addItem(title, vec2(8, y))

  ############################################################################
  # Project view
  ############################################################################

  def _draw_project_view(self, project, y_start, w, h):
    y = y_start
    ns_list = self.project_namespaces.get(project, [])

    self._draw_section_header("Project", y, w, h)
    y += 18
    self._draw_prop_row("Name", project, y)
    y += 16
    self._draw_prop_row("Namespaces", str(len(ns_list)), y)
    y += 16

    # Count totals
    total_assets = 0
    total_chunks = 0
    present_chunks = 0
    total_sz = 0
    local_sz = 0
    for ns in ns_list:
      for fqid in self.ns_assets.get(ns, []):
        total_assets += 1
        cs = self.chunk_status.get(fqid, [])
        total_chunks += len(cs)
        present_chunks += sum(1 for x in cs if x)
        entry = self.asset_entries.get(fqid)
        if entry:
          total_sz += entry.archive_size
          if cs and all(cs):
            local_sz += entry.archive_size

    self._draw_prop_row("Total Assets", str(total_assets), y)
    y += 16
    self._draw_prop_row("Chunks Local", f"{present_chunks} / {total_chunks}", y)
    y += 16
    self._draw_prop_row("Size Local", f"{format_size(local_sz)} / {format_size(total_sz)}", y)
    y += 16

    # Manifest source directory
    if ns_list:
      manifests = self.catalog.manifestsForNamespace(ns_list[0])
      if manifests and manifests[0].source_file:
        src = manifests[0].source_file
        manifest_dir = os.path.dirname(src)
        self._draw_prop_row("Manifest Dir", manifest_dir, y)
        y += 16
    y += 4

    # Manifest files
    mfiles = self.project_manifests.get(project, [])
    if mfiles:
      self._draw_section_header("Manifests", y, w, h)
      y += 18
      for mf in mfiles:
        if y > h - 40:
          self._texts["label"].addItem(f"... {len(mfiles)} total", vec2(8, y))
          y += 16
          break
        self._texts["label"].addItem(mf, vec2(8, y))
        y += 16
    # Import configs
    icfiles = self.project_import_configs.get(project, [])
    if icfiles:
      y += 4
      self._draw_section_header("Import Configs", y, w, h)
      y += 18
      for ic in icfiles:
        if y > h - 40:
          self._texts["label"].addItem(f"... {len(icfiles)} total", vec2(8, y))
          y += 16
          break
        # -> button to view details
        btn_name = f"IC:{ic}"
        selected = self.selected_import_config == ic
        self._add_button_quad(btn_name, 8, y, 20, 16, ">")
        self._texts["label"].addItem(ic, vec2(32, y))
        y += 18
        # Show details inline if selected
        if selected:
          y = self._draw_import_config_detail(ic, y, w, h)
    y += 4

    # Namespace summary grid
    self._draw_section_header("Namespaces", y, w, h)
    y += 18
    for ns in ns_list:
      if y > h - 20:
        self._texts["label"].addItem(f"... {len(ns_list)} total", vec2(8, y))
        break
      assets = self.ns_assets.get(ns, [])
      n_total = 0
      n_present = 0
      for fqid in assets:
        cs = self.chunk_status.get(fqid, [])
        n_total += len(cs)
        n_present += sum(1 for x in cs if x)
      label = f"{ns}  ({len(assets)} assets, {n_present}/{n_total} chunks)"
      if n_total > 0 and n_present == n_total:
        self._texts["status_ok"].addItem(label, vec2(8, y))
      elif n_present > 0:
        self._texts["status_warn"].addItem(label, vec2(8, y))
      else:
        self._texts["label"].addItem(label, vec2(8, y))
      y += 16

  ############################################################################
  # Namespace view
  ############################################################################

  def _import_config_needs_autocorrect(self, ic_name):
    """Check if import config has absolute paths (should use ${ENV_VAR} form)."""
    data = self._get_import_config(ic_name)
    if not data:
      return False
    for key in ["source_dir", "manifest"]:
      val = data.get(key, "")
      if val and os.path.isabs(val):
        return True
    return False

  def _sanitize_path(self, abspath):
    """Replace longest-matching env var prefix with ${VAR}."""
    abspath = os.path.abspath(abspath)
    best_var = None
    best_len = 0
    excluded = {'PWD', 'OLDPWD', 'HOME', 'PATH', 'SHELL', 'USER', 'LOGNAME',
                'TERM', 'LANG', 'DISPLAY', 'EDITOR', 'TMPDIR', '_',
                'PYTHONPATH', 'PYTHONHOME', 'SHLVL', 'COLORTERM'}
    for var, val in os.environ.items():
      if var in excluded or not val or not os.path.isabs(val):
        continue
      val = val.rstrip("/")
      if len(val) <= best_len:
        continue
      if abspath == val or abspath.startswith(val + "/"):
        best_var = var
        best_len = len(val)
    if best_var:
      remainder = abspath[best_len:]
      return f"${{{best_var}}}{remainder}"
    return abspath

  def _resolve_bad_path(self, bad_path, manifest_dir):
    """Try to resolve a bad absolute path (from another machine) to a local path,
       then sanitize with env var prefix."""
    if not bad_path or not os.path.isabs(bad_path):
      return bad_path
    if os.path.exists(bad_path):
      return self._sanitize_path(bad_path)
    # Derive project root from manifest_dir
    project_root = manifest_dir
    for _ in range(3):
      parent = os.path.dirname(project_root)
      if os.path.basename(project_root) in ("obt.project", "asset_manifests"):
        project_root = parent
      else:
        break
    # Try progressively shorter suffixes of bad_path against project_root
    parts = bad_path.replace("\\", "/").rstrip("/").split("/")
    for i in range(len(parts) - 1, 0, -1):
      suffix = os.path.join(*parts[i:])
      candidate = os.path.join(project_root, suffix)
      if os.path.exists(candidate):
        return self._sanitize_path(candidate)
    # Fallback: sanitize the project root itself
    return self._sanitize_path(project_root)

  def _do_autocorrect_import_config(self, ic_name):
    """Rewrite absolute paths in import config using env var substitution."""
    import json
    path = self.import_config_paths.get(ic_name)
    if not path:
      return
    manifest_dir = os.path.dirname(path)
    with open(path, 'r') as f:
      data = json.load(f)
    changed = False
    for key in ["source_dir", "manifest"]:
      val = data.get(key, "")
      if not val or not os.path.isabs(val):
        continue
      if os.path.exists(val):
        # Path exists but is absolute — sanitize with env var
        new_val = self._sanitize_path(val)
        if new_val != val:
          data[key] = new_val
          changed = True
      else:
        # Path doesn't exist — resolve then sanitize
        new_val = self._resolve_bad_path(val, manifest_dir)
        if new_val != val:
          data[key] = new_val
          changed = True
    if changed:
      with open(path, 'w') as f:
        json.dump(data, f, indent=2)
      self.import_config_data.pop(ic_name, None)
      self.canvas_dirty = True

  def _draw_import_config_detail(self, ic_name, y, w, h):
    """Draw expanded import config details inline. Returns updated y."""
    data = self._get_import_config(ic_name)
    if not data:
      self._texts["status_err"].addItem("  (could not load)", vec2(8, y))
      return y + 16
    indent = 24

    # Autocorrect button if paths look wrong
    if self._import_config_needs_autocorrect(ic_name):
      btn_name = f"AUTOCORRECT:{ic_name}"
      self._add_button_quad(btn_name, indent, y, 96, 16, "AUTOCORRECT", danger=True)
      self._texts["status_err"].addItem("(paths need correction)", vec2(indent + 104, y + 1))
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
        self._texts["label"].addItem(f"{label}:", vec2(indent, y))
        # Highlight bad paths in red
        if key in ("source_dir", "manifest") and os.path.isabs(val) and not os.path.exists(val):
          self._texts["status_err"].addItem(str(val), vec2(indent + 120, y))
        else:
          self._texts["value"].addItem(str(val), vec2(indent + 120, y))
        y += 14
    if "platforms" in data:
      self._texts["label"].addItem("platforms:", vec2(indent, y))
      self._texts["value"].addItem(", ".join(data["platforms"]), vec2(indent + 120, y))
      y += 14
    assets = data.get("assets", [])
    if assets:
      self._texts["label"].addItem("assets:", vec2(indent, y))
      y += 14
      for a in assets:
        if isinstance(a, dict):
          aid = a.get("id", "?")
          inc = a.get("include", "")
          self._texts["value"].addItem(f"{aid}  ({inc})", vec2(indent + 8, y))
        else:
          self._texts["value"].addItem(str(a), vec2(indent + 8, y))
        y += 14
    y += 4
    return y

  def _draw_namespace_view(self, ns, y_start, w, h):
    y = y_start

    # Detail properties
    d = _build_namespace_varmap(self, ns)
    self._draw_section_header("Namespace", y, w, h)
    y += 18
    for key in ["Name", "Asset Count"]:
      if key in d:
        self._draw_prop_row(key, d[key], y)
        y += 16
    y += 4
    self._draw_section_header("Config", y, w, h)
    y += 18
    for key in ["Encryption Key", "Remote Location", "Download URL", "Upload URL",
                 "TLS Verify", "API Key (read)", "API Key (write)"]:
      if key in d:
        self._draw_prop_row(key, d[key], y)
        y += 16
    y += 4
    self._draw_section_header("Status", y, w, h)
    y += 18
    for key in ["Chunks Local", "Size Local"]:
      if key in d:
        self._draw_prop_row(key, d[key], y)
        y += 16
    if "CDN Endpoint" in d:
      y += 4
      self._draw_section_header("CDN", y, w, h)
      y += 18
      for key in ["CDN Endpoint", "CDN Reachable", "CDN Latency"]:
        if key in d:
          self._draw_prop_row(key, d[key], y)
          y += 16
    # Manifest files for this namespace
    ns_mfiles = self.ns_manifests.get(ns, [])
    if ns_mfiles:
      y += 4
      self._draw_section_header("Manifests", y, w, h)
      y += 18
      for mf in ns_mfiles:
        self._draw_prop_row("File", mf, y)
        y += 16
    ns_ics = self.ns_import_configs.get(ns, [])
    if ns_ics:
      y += 4
      self._draw_section_header("Import Configs", y, w, h)
      y += 18
      for ic in ns_ics:
        btn_name = f"IC:{ic}"
        selected = self.selected_import_config == ic
        self._add_button_quad(btn_name, 8, y, 20, 16, ">")
        self._texts["label"].addItem(ic, vec2(32, y))
        y += 18
        if selected:
          y = self._draw_import_config_detail(ic, y, w, h)
    y += 8

    # Compute local/CDN completeness and key availability for ghosting
    can_fetch = self._ns_can_fetch(ns)
    can_upload = self._ns_can_upload(ns)
    all_local = True
    all_cdn = True
    for fqid in self.ns_assets.get(ns, []):
      cs = self.chunk_status.get(fqid, [])
      if not cs or not all(cs):
        all_local = False
      cdn = self.cdn_status.get(fqid, [])
      if not cdn or not all(cdn):
        all_cdn = False

    # Buttons — ghost if missing keys or already complete
    bx = 8
    self._add_button_quad("FETCH ALL", bx, y, 80, 22, "FETCH ALL", ghost=all_local or not can_fetch)
    bx += 88
    self._add_button_quad("UPLOAD", bx, y, 64, 22, "UPLOAD", ghost=all_cdn or not can_upload)
    y += 30

    # Asset grid — compact: name + chunk fraction, color = status
    assets = self.ns_assets.get(ns, [])
    if not assets:
      self._texts["label"].addItem("(no assets)", vec2(8, y))
      return

    self._draw_section_header("Assets", y, w, h)
    y += 18

    # Grid layout
    cell_w = 200
    cell_h = 16
    max_name = 14
    cols = max(1, (w - 16) // cell_w)
    rows_avail = max(1, (h - y - 40) // cell_h)
    total_slots = cols * rows_avail
    show_all = len(assets) <= total_slots
    assets_to_show = assets if show_all else assets[:total_slots - 1]

    # Accumulate summary
    total_chunks, present_chunks = 0, 0
    total_sz, local_sz = 0, 0
    for fqid in assets:
      cs = self.chunk_status.get(fqid, [])
      total_chunks += len(cs)
      present_chunks += sum(1 for x in cs if x)
      entry = self.asset_entries.get(fqid)
      if entry:
        total_sz += entry.archive_size
        if cs and all(cs):
          local_sz += entry.archive_size

    for idx, fqid in enumerate(assets_to_show):
      col = idx % cols
      row = idx // cols
      cx = 8 + col * cell_w
      cy = y + row * cell_h

      name = fqid.split("|", 1)[1] if "|" in fqid else fqid
      if len(name) > max_name:
        name = name[:max_name - 2] + ".."

      cs = self.chunk_status.get(fqid, [])
      ntotal = len(cs)
      npresent = sum(1 for x in cs if x) if cs else 0

      # Format: "name  N/M" — color by status
      if ntotal > 0:
        label = f"{name}  {npresent}/{ntotal}"
        if fqid in self.active_fetches:
          self._texts["status_warn"].addItem(label, vec2(cx, cy))
        elif npresent == ntotal:
          self._texts["status_ok"].addItem(label, vec2(cx, cy))
        elif npresent == 0:
          self._texts["status_err"].addItem(label, vec2(cx, cy))
        else:
          pct = int(npresent / ntotal * 100)
          label = f"{name}  {npresent}/{ntotal} ({pct}%)"
          self._texts["status_warn"].addItem(label, vec2(cx, cy))
      else:
        self._texts["label"].addItem(f"{name}  --", vec2(cx, cy))

    if not show_all:
      remaining = len(assets) - len(assets_to_show)
      col = len(assets_to_show) % cols
      row = len(assets_to_show) // cols
      cx = 8 + col * cell_w
      cy = y + row * cell_h
      self._texts["status_warn"].addItem(f"+{remaining} more", vec2(cx, cy))

    grid_rows = (len(assets_to_show) + cols - 1) // cols
    if not show_all:
      grid_rows = rows_avail
    y += grid_rows * cell_h + 8

    # Summary
    summary = f"Local: {present_chunks}/{total_chunks} chunks  |  {format_size(local_sz)} / {format_size(total_sz)}"
    self._texts["value"].addItem(summary, vec2(8, y))

  ############################################################################
  # Asset view
  ############################################################################

  def _draw_asset_view(self, fqid, y_start, w, h):
    y = y_start
    entry = self.asset_entries.get(fqid)
    if not entry:
      self._texts["label"].addItem("No entry", vec2(8, y))
      return

    # Detail properties
    d = _build_asset_varmap(self, fqid)
    self._draw_section_header("Asset", y, w, h)
    y += 18
    for key in ["ID", "Manifest", "Type", "Platforms", "Priority"]:
      if key in d:
        self._draw_prop_row(key, d[key], y)
        y += 16
    y += 4
    self._draw_section_header("Sizes", y, w, h)
    y += 18
    for key in ["Archive Size", "Compressed Size", "Encrypted Size", "Compression"]:
      if key in d:
        self._draw_prop_row(key, d[key], y)
        y += 16
    y += 4
    self._draw_section_header("Hashes", y, w, h)
    y += 18
    for key in ["Content Hash", "Storage Hash", "Hash Algorithm",
                 "File Hash (expected)", "Encrypted File"]:
      if key in d:
        self._draw_prop_row(key, d[key], y)
        y += 16
    # Verify hash button + result inline
    verify_result = self.hash_verify_results.get(fqid, "")
    verifying = verify_result == "verifying..."
    self._add_button_quad("VERIFY HASH", 8, y, 96, 18, "VERIFY HASH", ghost=verifying)
    if verify_result:
      self._texts["status_ok" if verify_result == "OK" else "status_warn" if verifying else "status_err"].addItem(
        verify_result, vec2(112, y + 1))
    else:
      self._texts["label"].addItem("(not checked)", vec2(112, y + 1))
    y += 22
    y += 4
    self._draw_section_header("Chunks", y, w, h)
    y += 18
    for key in ["Total Chunks", "Chunk Size", "Chunks Local", "Chunks Valid"]:
      if key in d:
        self._draw_prop_row(key, d[key], y)
        y += 16
    if any(k in d for k in ["Local Location", "Local Path", "Remote URL"]):
      y += 4
      self._draw_section_header("Location", y, w, h)
      y += 18
      for key in ["Local Location", "Local Path", "Local Exists", "Remote URL"]:
        if key in d:
          self._draw_prop_row(key, d[key], y)
          y += 16
    y += 8

    # Buttons — ghost based on status and key availability
    ns = fqid.split("|")[0]
    can_fetch = self._ns_can_fetch(ns)
    can_upload = self._ns_can_upload(ns)
    cs = self.chunk_status.get(fqid, [])
    cdn = self.cdn_status.get(fqid, [])
    fetch_ghost = (bool(cs) and all(cs)) or not can_fetch
    upload_ghost = (bool(cdn) and all(cdn)) or not can_upload

    is_fetching = fqid in self.active_fetches

    bx = 8
    self._add_button_quad("FETCH", bx, y, 56, 22, "FETCH", ghost=fetch_ghost or is_fetching)
    bx += 64
    self._add_button_quad("CANCEL", bx, y, 64, 22, "CANCEL", ghost=not is_fetching, danger=is_fetching)
    bx += 72
    self._add_button_quad("UPLOAD", bx, y, 64, 22, "UPLOAD", ghost=upload_ghost)
    bx += 72
    self._add_button_quad("CLEAR LOCAL", bx, y, 90, 22, "CLEAR LOCAL", danger=True)
    y += 30

    # Live download progress bar
    fetch_req = self.active_fetches.get(fqid)
    if fetch_req:
      dl_total = fetch_req.bytes_total
      dl_done = fetch_req.bytes_downloaded
      dl_chunks_done = fetch_req.chunks_completed
      dl_chunks_total = fetch_req.chunks_total
      dl_pct = fetch_req.progress

      # Progress bar
      bar_x, bar_w, bar_h = 8, w - 16, 20
      qd = lev2.ui.QuadData()
      qd.setPosition(bar_x, h - y - bar_h - 1)
      qd.setSize(bar_w, bar_h)
      qd.setColor(vec4(0.15, 0.15, 0.18, 1))
      self.bg_prim.addQuad(qd)
      fill_w = int(bar_w * dl_pct)
      if fill_w > 0:
        qd2 = lev2.ui.QuadData()
        qd2.setPosition(bar_x, h - y - bar_h - 1)
        qd2.setSize(fill_w, bar_h)
        qd2.setColor(vec4(0.2, 0.4, 0.8, 1))
        self.bg_prim.addQuad(qd2)
      pct_i = int(dl_pct * 100)
      dl_text = f"Downloading: {format_size(dl_done)} / {format_size(dl_total)}  ({pct_i}%)  chunks {dl_chunks_done}/{dl_chunks_total}"
      text_w = len(dl_text) * 8
      tx = bar_x + (bar_w - text_w) // 2
      ty = y + (bar_h - 14) // 2
      self._texts["value"].addItem(dl_text, vec2(tx, ty))
      y += bar_h + 4

    vs = self.chunk_valid.get(fqid, [])
    cdn = self.cdn_status.get(fqid, [])
    total = len(cs)
    if total == 0:
      self._texts["label"].addItem("No chunks", vec2(8, y))
      return

    present = sum(1 for x in cs if x)
    chunk_sz = ""
    if hasattr(entry, 'chunk_manifest') and entry.chunk_manifest:
      cm = entry.chunk_manifest
      chunk_sz = format_size(cm.chunk_size) if hasattr(cm, 'chunk_size') else ""

    self._texts["label"].addItem(
      f"Chunk Status ({total} chunks, {chunk_sz} each):", vec2(8, y))
    y += 20

    # Chunk grid
    cell_w = 12
    cells_per_row = max(1, (w - 60) // cell_w)
    grid_x = 50

    # LOC row
    self._texts["label"].addItem("LOC:", vec2(8, y))
    for i in range(total):
      row = i // cells_per_row
      col = i % cells_per_row
      cx = grid_x + col * cell_w
      cy = y + row * 16
      qd = lev2.ui.QuadData()
      qd.setPosition(cx, h - cy - 12)
      qd.setSize(cell_w - 2, 12)
      if not cs[i]:
        qd.setColor(vec4(0.5, 0.15, 0.15, 1))  # red = missing
      elif i < len(vs) and not vs[i]:
        qd.setColor(vec4(0.7, 0.7, 0.2, 1))     # yellow = hash mismatch
      elif fqid in self.active_fetches:
        qd.setColor(vec4(0.2, 0.3, 0.7, 1))     # blue = downloading
      else:
        qd.setColor(vec4(0.2, 0.6, 0.2, 1))     # green = present+valid
      self.bg_prim.addQuad(qd)

    loc_rows = (total + cells_per_row - 1) // cells_per_row
    y += loc_rows * 16 + 4

    # CDN row (if verified)
    if cdn:
      self._texts["label"].addItem("CDN:", vec2(8, y))
      for i in range(total):
        row = i // cells_per_row
        col = i % cells_per_row
        cx = grid_x + col * cell_w
        cy = y + row * 16
        qd = lev2.ui.QuadData()
        qd.setPosition(cx, h - cy - 12)
        qd.setSize(cell_w - 2, 12)
        if i < len(cdn) and cdn[i]:
          qd.setColor(vec4(0.2, 0.6, 0.2, 1))
        else:
          qd.setColor(vec4(0.5, 0.15, 0.15, 1))
        self.bg_prim.addQuad(qd)
      cdn_rows = (total + cells_per_row - 1) // cells_per_row
      y += cdn_rows * 16 + 4

    y += 4
    # Summary counts
    cdn_present = sum(1 for x in cdn if x) if cdn else 0
    summary = f"Local: {present}/{total}"
    if cdn:
      summary += f"     CDN: {cdn_present}/{total}"
    self._texts["value"].addItem(summary, vec2(8, y))
    y += 20

    # Overall progress bar
    bar_x = 8
    bar_w = w - 16
    qd = lev2.ui.QuadData()
    qd.setPosition(bar_x, h - y - 18)
    qd.setSize(bar_w, 16)
    qd.setColor(vec4(0.15, 0.15, 0.18, 1))
    self.bg_prim.addQuad(qd)

    frac = present / total if total > 0 else 0
    fill_w = int(bar_w * frac)
    if fill_w > 0:
      qd2 = lev2.ui.QuadData()
      qd2.setPosition(bar_x, h - y - 18)
      qd2.setSize(fill_w, 16)
      qd2.setColor(vec4(0.2, 0.6, 0.2, 1) if frac >= 1.0 else vec4(0.3, 0.5, 0.3, 1))
      self.bg_prim.addQuad(qd2)

    pct = int(frac * 100)
    self._texts["value"].addItem(f"{pct}%", vec2(bar_x + bar_w // 2 - 10, y))

################################################################################

if __name__ == "__main__":
  app = CatalogTool()
  app.ezapp.mainThreadLoop()
