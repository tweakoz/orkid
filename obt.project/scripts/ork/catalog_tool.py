################################################################################
# Catalog Tool — Model (business logic, no UI imports)
#
# This is the Model layer of an MVC-structured catalog tool for managing
# orkid asset catalogs. It contains:
#
#   - Utility functions for display formatting and path resolution
#   - VarMap builders that produce display-ready property dicts for UI views
#   - CatalogModel class — core business logic with no UI dependencies
#
# Data hierarchy:
#   Project  ->  Namespace(s)  ->  Asset(s)  ->  Chunk(s)
#
# Projects are derived from manifest file paths (not stored explicitly in
# manifests). Namespaces group related assets. Each asset has a chunk
# manifest describing how its encrypted archive is split into downloadable
# pieces. Chunks are stored locally under <cache>/enc/chunks/.
#
# The companion View/Controller modules (catalog_tool_qt.py, etc.) import
# this module and register callbacks on CatalogModel to drive their UI.
################################################################################

import os, re, time, threading, json, requests, urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
from orkengine import core
from ork import assets as ork_assets

################################################################################
# Utilities
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

def derive_project_name(source_file):
  """Extract project name from manifest source_file path.

  Walks the path looking for the 'asset_manifests' directory, then uses
  the parent (or grandparent for obt.project/ork.data) as the project name.
  e.g. '/foo/myproj/obt.project/asset_manifests/ns.json' -> 'myproj'
  """
  parts = source_file.replace("\\", "/").split("/")
  for i, p in enumerate(parts):
    if p == "asset_manifests" and i >= 2:
      parent = parts[i - 1]
      grandparent = parts[i - 2]
      # obt.project and ork.data are infrastructure dirs, go one level up
      if parent in ("obt.project", "ork.data"):
        return grandparent
      return parent
  return "unknown"

def sanitize_path(abspath):
  """Replace longest-matching env var prefix with ${VAR}.

  Makes paths portable across machines by substituting project-specific
  env vars (e.g. ${OBT_STAGE}) instead of hard-coded absolute paths.
  Generic shell vars (HOME, PATH, etc.) are excluded to avoid confusion.
  """
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

def resolve_bad_path(bad_path, manifest_dir):
  """Try to resolve a bad absolute path (from another machine) to a local path,
     then sanitize with env var prefix.

  Import configs may contain absolute paths baked from a different user's
  machine. This function tries progressively shorter suffixes of the bad path
  against the local project root until a match is found.
  """
  if not bad_path or not os.path.isabs(bad_path):
    return bad_path
  if os.path.exists(bad_path):
    return sanitize_path(bad_path)
  # Walk up from manifest_dir to find the project root
  project_root = manifest_dir
  for _ in range(3):
    parent = os.path.dirname(project_root)
    if os.path.basename(project_root) in ("obt.project", "asset_manifests"):
      project_root = parent
    else:
      break
  # Try progressively shorter suffixes: e.g. "a/b/c/d" -> "b/c/d" -> "c/d" -> "d"
  parts = bad_path.replace("\\", "/").rstrip("/").split("/")
  for i in range(len(parts) - 1, 0, -1):
    suffix = os.path.join(*parts[i:])
    candidate = os.path.join(project_root, suffix)
    if os.path.exists(candidate):
      return sanitize_path(candidate)
  return sanitize_path(project_root)

################################################################################
# VarMap builders (display-ready property dicts)
#
# These functions produce flat string-keyed dicts that UI views can display
# directly in property sheets. They read from CatalogModel state and resolve
# CDN/encryption info into human-readable values.
################################################################################

def build_namespace_varmap(model, ns_id):
  """Build a flat dict of namespace properties for the property sheet view."""
  d = {}
  d["Name"] = ns_id
  d["Asset Count"] = str(len(model.ns_assets.get(ns_id, [])))

  # Encryption and remote location from the merged config space
  merged = model.cfgspc.merged_config
  ek = merged.getEncryptionKeyForNamespace(ns_id) if merged else None
  d["Encryption Key"] = "Yes" if ek else "No"

  remote_loc = model.cfgspc.getNamespaceRemoteLocation(ns_id) or ""
  d["Remote Location"] = remote_loc

  # Resolve remote location to get concrete URLs and auth info
  if remote_loc and merged:
    resolved = merged.resolveRemoteLocation(remote_loc)
    if resolved:
      d["Download URL"] = str(resolved.download_url)
      d["Upload URL"] = str(resolved.upload_url)
      d["TLS Verify"] = "Disabled" if resolved.disable_cert_check else "Enabled"
      akr = resolved.api_key_read if hasattr(resolved, 'api_key_read') else None
      if akr == "<PasswordAuthentication>":
        d["API Key (read)"] = "password auth"
      elif akr:
        d["API Key (read)"] = "Yes"
      else:
        d["API Key (read)"] = "No"
      akw = resolved.api_key_write if hasattr(resolved, 'api_key_write') else None
      if akw == "<PasswordAuthentication>":
        d["API Key (write)"] = "password auth"
      elif akw:
        d["API Key (write)"] = "Yes"
      else:
        d["API Key (write)"] = "No"

  # Aggregate chunk presence and size stats across all assets in namespace
  total, present = 0, 0
  total_sz, local_sz = 0, 0
  for fqid in model.ns_assets.get(ns_id, []):
    cs = model.chunk_status.get(fqid, [])
    total += len(cs)
    present += sum(1 for x in cs if x)
    entry = model.asset_entries.get(fqid)
    if entry:
      total_sz += entry.archive_size
      if cs and all(cs):
        local_sz += entry.archive_size
  d["Chunks Local"] = f"{present} / {total}"
  d["Size Local"] = f"{format_size(local_sz)} / {format_size(total_sz)}"

  # Include CDN health info if we've pinged this endpoint
  cdn_url = d.get("Download URL", "")
  if cdn_url:
    from urllib.parse import urlparse
    host = urlparse(cdn_url).hostname or ""
    if host in model.cdn_health:
      h = model.cdn_health[host]
      d["CDN Endpoint"] = h.get("url", "")
      d["CDN Reachable"] = "Yes" if h.get("reachable") else "No"
      ms = h.get("latency_ms", 0)
      d["CDN Latency"] = f"{ms:.0f}ms" if h.get("reachable") else "N/A"
    else:
      d["CDN Reachable"] = "unreachable"
      d["CDN Latency"] = "N/A"

  return d


def build_asset_varmap(model, fqid):
  """Build a flat dict of asset properties for the property sheet view."""
  d = {}
  entry = model.asset_entries.get(fqid)
  if not entry:
    d["ID"] = fqid
    return d

  d["ID"] = fqid
  d["Manifest"] = model.asset_manifest_file.get(fqid, "unknown")
  d["Type"] = entry.type or "unknown"
  d["Platforms"] = ", ".join(entry.platforms) if entry.platforms else "all"
  d["Priority"] = str(entry.priority)
  d["Archive Size"] = format_size(entry.archive_size)
  d["Compressed Size"] = format_size(entry.compressed_size)
  d["Encrypted Size"] = format_size(entry.encrypted_size)
  if entry.archive_size > 0:
    r = 100.0 - (entry.compressed_size / entry.archive_size * 100.0)
    d["Compression"] = f"{r:.1f}%"  # space savings from compression

  # Hash and integrity metadata
  if hasattr(entry, 'content_hash') and entry.content_hash:
    d["Content Hash"] = str(entry.content_hash)
  if hasattr(entry, 'storage_hash') and entry.storage_hash:
    d["Storage Hash"] = str(entry.storage_hash)
  if hasattr(entry, 'hash_algorithm') and entry.hash_algorithm:
    d["Hash Algorithm"] = str(entry.hash_algorithm)

  # Chunk manifest: describes how the encrypted archive is split into pieces
  cm = entry.chunk_manifest if hasattr(entry, 'chunk_manifest') else None

  if cm and hasattr(cm, 'file_hash') and cm.file_hash:
    d["File Hash (expected)"] = f"{cm.file_hash:016x}"
  if hasattr(entry, 'storage_hash') and entry.storage_hash:
    cache_dir = model.catalog.cache_dir
    enc_path = os.path.join(cache_dir, 'enc', f"{entry.storage_hash}.enc")
    d["Encrypted File"] = "Yes" if os.path.exists(enc_path) else "No"
  cm = entry.chunk_manifest if hasattr(entry, 'chunk_manifest') else None
  if cm:
    d["Total Chunks"] = str(len(cm.chunks))
    if hasattr(cm, 'chunk_size'):
      d["Chunk Size"] = format_size(cm.chunk_size)

  # Local chunk presence and hash-validity counts
  cs = model.chunk_status.get(fqid, [])
  total = len(cs)
  present = sum(1 for x in cs if x)
  d["Chunks Local"] = f"{present} / {total}"

  vs = model.chunk_valid.get(fqid, [])
  valid = sum(1 for x in vs if x)
  d["Chunks Valid"] = f"{valid} / {len(vs)}"

  # Resolved local file path (where dearchived asset lives)
  if hasattr(entry, 'local_loc') and entry.local_loc:
    d["Local Location"] = str(entry.local_loc)
    merged = model.cfgspc.merged_config
    if merged:
      resolved_path = merged.resolveLocalPath(entry.local_loc)
      if resolved_path:
        d["Local Path"] = str(resolved_path)
        d["Local Exists"] = "Yes" if os.path.exists(str(resolved_path)) else "No"
  elif hasattr(entry, 'resolved_local_path') and entry.resolved_local_path:
    d["Local Path"] = str(entry.resolved_local_path)
    d["Local Exists"] = "Yes" if os.path.exists(str(entry.resolved_local_path)) else "No"

  # fqid format is "namespace|asset_id" — split to get the namespace
  ns = fqid.split("|")[0]
  remote_loc = model.cfgspc.getNamespaceRemoteLocation(ns)
  if remote_loc:
    merged = model.cfgspc.merged_config
    resolved = merged.resolveRemoteLocation(remote_loc) if merged else None
    if resolved:
      d["Remote URL"] = str(resolved.download_url)

  return d

################################################################################
# CatalogModel
################################################################################

class CatalogModel:
  """Frontend-agnostic catalog data and operations.

  All state lives here. Frontends read properties and call actions.
  Callbacks fire on state changes so any frontend can react.

  Callback system:
    Frontends register callbacks by assigning callables to the on_* attrs.
    The model fires them via _fire() when state changes. Callbacks may fire
    from background threads (e.g. CDN health checks, import runs), so
    frontends must handle thread safety (e.g. Qt signals, tkinter after()).

  Threading model:
    _bg() launches daemon threads for long-running operations (CDN pings,
    imports). poll() is meant to be called from the UI's update/timer loop
    to check async fetch progress without blocking the main thread.
  """

  def __init__(self):
    # --- Callbacks (set by frontend) ---
    # Frontends assign callables here; model calls them on state changes.
    # All are optional (None = no-op).
    self.on_scan_complete = None       # () -> called after full rescan
    self.on_chunk_status_changed = None # (fqid) -> chunk files changed on disk
    self.on_fetch_started = None       # (fqid) -> async download kicked off
    self.on_fetch_progress = None      # (fqid) -> download progress updated
    self.on_fetch_complete = None      # (fqid, success) -> download finished
    self.on_cdn_health_updated = None  # () -> CDN ping results ready
    self.on_import_started = None      # (ic_name) -> import process started
    self.on_import_output = None       # (ic_name, text) -> live import output
    self.on_import_complete = None     # (ic_name, success) -> import finished
    self.on_verify_complete = None     # (fqid, result_str) -> hash verify done
    self.on_error = None               # (message) -> error occurred
    self.on_dirty = None               # () -> generic "something changed" signal

    # --- Data: project/namespace/asset hierarchy ---
    self.namespaces = []               # sorted list of all namespace IDs
    self.projects = []                 # sorted list of derived project names
    self.project_namespaces = {}       # project -> [namespace IDs]
    self.project_manifests = {}        # project -> [manifest basenames]
    self.project_import_configs = {}   # project -> [import config basenames]
    self.ns_project = {}               # namespace -> project name
    self.ns_manifests = {}             # namespace -> [manifest basenames]
    self.ns_import_configs = {}        # namespace -> [import config basenames]
    self.ns_assets = {}                # namespace -> [fqid strings]

    # --- Data: per-asset state ---
    self.asset_entries = {}            # fqid -> AssetEntry object (from C++)
    self.asset_manifest_file = {}      # fqid -> manifest basename it came from
    self.chunk_status = {}             # fqid -> [bool] per-chunk presence on disk
    self.chunk_valid = {}              # fqid -> [bool] per-chunk hash validity
    self.cdn_status = {}               # fqid -> [bool] per-chunk presence on CDN
    self.cdn_health = {}               # hostname -> {reachable, latency_ms, ...}
    self.active_fetches = {}           # fqid -> FetchRequest (in-progress downloads)
    self.hash_verify_results = {}      # fqid -> result string ("OK", "MISMATCH", ...)

    # --- Data: import configs ---
    self.import_output_lines = []      # live output lines from current import
    self.import_config_paths = {}      # basename -> absolute file path
    self.import_config_data = {}       # basename -> parsed JSON (cached)

    # --- Internal state ---
    self.http_session = requests.Session()  # reused for CDN requests
    self.cdn_ping_time = 0             # timestamp of last CDN health check
    self.cdn_ping_interval = 1.0       # seconds between automatic re-pings
    self.cdn_ping_running = False      # guard against overlapping ping threads
    self._last_fetch_snap = None       # snapshot for detecting fetch progress changes

    # --- Catalog engine objects (initialized by init()) ---
    self.cfgspc = None                 # ConfigSpace: merged config + env overrides
    self.catalog = None                # AssetCatalog: C++ catalog engine

  def _fire(self, cb, *args):
    """Invoke a callback if registered. Safe to call with None callbacks."""
    if cb:
      cb(*args)

  def _mark_dirty(self):
    """Signal that model state changed — frontends should refresh their views."""
    self._fire(self.on_dirty)

  def _bg(self, fn):
    """Run fn on a daemon thread. NOTE: callbacks fired from fn will execute
    on the background thread, not the main/UI thread."""
    t = threading.Thread(target=fn, daemon=True)
    t.start()

  ############################################################################
  # Initialization
  ############################################################################

  def init(self):
    """Initialize catalog engine, scan all manifests, and start CDN health check."""
    self.cfgspc, self.catalog = ork_assets.default_cfg_and_catalog()
    self.scan()
    self._bg(self.check_cdn_health)  # non-blocking CDN ping on startup
    # Suppress noisy per-chunk download log messages
    logger = core.Logger.instance()
    dl_chan = logger.getChannel("DOWNLOAD")
    if dl_chan:
      dl_chan.enabled = False

  ############################################################################
  # Scanning
  ############################################################################

  def scan(self):
    """Full rescan: re-read all manifests, rebuild hierarchy, check all chunks.

    Populates the project -> namespace -> asset -> chunk hierarchy from
    the catalog engine's manifest files. Distinguishes "real" manifests
    (containing assets with storage_hash) from import configs (JSON files
    that define import operations but have no actual asset data yet).
    """
    self.namespaces = sorted(self.catalog.list_namespaces("*"))
    self.ns_assets = {}
    self.asset_entries = {}
    self.chunk_status = {}
    self.chunk_valid = {}
    self.project_namespaces = {}
    self.project_manifests = {}
    self.project_import_configs = {}
    self.import_config_paths = {}
    self.import_config_data = {}
    self.ns_project = {}
    self.ns_manifests = {}
    self.ns_import_configs = {}
    self.asset_manifest_file = {}

    # Phase 1: Build project/namespace hierarchy from manifest source files
    for ns in self.namespaces:
      manifests = self.catalog.manifestsForNamespace(ns)
      project = "unknown"
      ns_files = []
      ns_import_configs = []
      for m in manifests:
        sf = m.source_file
        if sf:
          if project == "unknown":
            project = derive_project_name(sf)
          basename = os.path.basename(sf)
          assets = m.assets
          # A "real" manifest has assets with storage hashes (already imported)
          is_real = assets and any(hasattr(assets[k], 'storage_hash') and assets[k].storage_hash for k in assets)
          if is_real:
            ns_files.append(basename)
            for asset_id in assets.keys():
              self.asset_manifest_file[f"{ns}|{asset_id}"] = basename
          else:
            # No storage hashes = this is an import config, not a data manifest
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

    # Phase 2: Enumerate assets per namespace and scan their chunk status
    for ns in self.namespaces:
      fqids = sorted(self.catalog.list_assets(f"{ns}|*"))  # glob for all assets
      self.ns_assets[ns] = fqids
      for fqid in fqids:
        entry = self.catalog.findAssetEntry(fqid)
        self.asset_entries[fqid] = entry
        self.scan_chunks(fqid, entry)

    self._fire(self.on_scan_complete)
    self._mark_dirty()

  def scan_chunks(self, fqid, entry=None):
    """Deep scan single asset — checks both file presence and hash validity.

    Chunk files are named: <storage_hash>.chunk.<NNNN> and live in
    <cache_dir>/enc/chunks/. Each chunk's xxhash64 is compared against
    the expected hash in the chunk manifest.
    """
    if entry is None:
      entry = self.asset_entries.get(fqid)
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

  def scan_chunks_fast(self, fqid, entry=None):
    """Presence-only check — no hash verification. Used during active downloads
    to quickly update progress without the overhead of reading+hashing each chunk."""
    if entry is None:
      entry = self.asset_entries.get(fqid)
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
  # CDN health
  ############################################################################

  def check_cdn_health(self):
    """Ping each unique CDN host, dedup by hostname."""
    from urllib.parse import urlparse
    import socket
    health = {}
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
    self._fire(self.on_cdn_health_updated)
    self._mark_dirty()

  ############################################################################
  # Key parsing
  #
  # Tree widget keys encode the hierarchy level:
  #   "myproject"              -> project
  #   "myproject/mynamespace"  -> namespace (project/ns)
  #   "myproject/ns|assetname" -> asset (project/ns|asset = project/fqid)
  #
  # The "|" separator distinguishes asset keys from namespace keys.
  # The "/" separator separates project from the rest.
  ############################################################################

  def key_type(self, key):
    """Return 'project', 'namespace', or 'asset'."""
    if not key:
      return None
    if "|" in key:
      return "asset"
    if "/" in key:
      return "namespace"
    return "project"

  def key_to_ns(self, key):
    """Extract namespace id from a namespace or asset key."""
    if "|" in key:
      return key.split("/", 1)[1].split("|")[0] if "/" in key else key.split("|")[0]
    if "/" in key:
      return key.split("/", 1)[1]
    return None

  def key_to_fqid(self, key):
    """Extract fqid (ns|asset) from an asset key."""
    if "|" in key and "/" in key:
      return key.split("/", 1)[1]
    if "|" in key:
      return key
    return None

  def key_to_project(self, key):
    """Extract project name from any key."""
    if "/" in key:
      return key.split("/", 1)[0]
    if "|" not in key:
      return key
    return None

  ############################################################################
  # Capability queries
  #
  # These check whether operations are possible for a namespace, based on
  # the presence of encryption keys, API keys, and CDN reachability.
  # Used by UI to enable/disable action buttons.
  ############################################################################

  def ns_cdn_reachable(self, ns):
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

  def ns_can_fetch(self, ns):
    """Check if namespace has encryption key and CDN is reachable."""
    merged = self.cfgspc.merged_config
    if not merged:
      return False
    if not merged.getEncryptionKeyForNamespace(ns):
      return False
    return self.ns_cdn_reachable(ns)

  def ns_can_upload(self, ns):
    """Check if namespace has encryption key, write API key, and CDN is reachable."""
    merged = self.cfgspc.merged_config
    if not merged:
      return False
    if not merged.getEncryptionKeyForNamespace(ns):
      return False
    if not self.ns_cdn_reachable(ns):
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
  # Import config operations
  #
  # Import configs are JSON files that define how to import raw source
  # files into the asset catalog. They specify:
  #   - namespace, source_dir, local_loc, manifest output path
  #   - encryption_key, platforms, priority
  #   - assets: list of {id, include glob, exclude globs}
  #
  # The actual import is performed by catalog_import.py; this model
  # manages loading/saving/creating the config files and running imports.
  ############################################################################

  def load_import_config(self, basename):
    """Load and cache import config JSON data."""
    if basename in self.import_config_data:
      return self.import_config_data[basename]
    path = self.import_config_paths.get(basename)
    if not path or not os.path.exists(path):
      return None
    try:
      with open(path, 'r') as f:
        data = json.load(f)
      self.import_config_data[basename] = data
      return data
    except Exception:
      return None

  def save_import_config(self, basename, data):
    """Write JSON to disk and invalidate the cache so next load re-reads."""
    path = self.import_config_paths.get(basename)
    if not path:
      return
    with open(path, 'w') as f:
      json.dump(data, f, indent=2)
      f.write('\n')
    self.import_config_data.pop(basename, None)  # invalidate cache

  def create_import_config(self, project):
    """Create a new template import config in the project's manifest dir.
    Returns the basename of the created file, or None on failure."""
    if project not in self.project_namespaces:
      return None
    ns_list = self.project_namespaces.get(project, [])
    if not ns_list:
      return None
    manifests = self.catalog.manifestsForNamespace(ns_list[0])
    if not manifests or not manifests[0].source_file:
      return None
    manifest_dir = os.path.dirname(manifests[0].source_file)
    base = f"{project}_import"
    idx = 0
    while True:
      suffix = f"_{idx}" if idx > 0 else ""
      filename = f"{base}{suffix}.json"
      filepath = os.path.join(manifest_dir, filename)
      if not os.path.exists(filepath):
        break
      idx += 1
    template = {
      "namespace": ns_list[0] if len(ns_list) == 1 else project,
      "source_dir": sanitize_path(os.path.dirname(manifest_dir.rstrip("/"))),
      "local_loc": "<stage>/assetcache/" + project,
      "manifest": sanitize_path(os.path.join(manifest_dir, f"{project}.json")),
      "encryption_key": "${YOUR_ENC_KEY}",
      "platforms": ["mac", "linux"],
      "assets": [
        {"id": "example_asset", "include": "data/*"}
      ]
    }
    with open(filepath, 'w') as f:
      json.dump(template, f, indent=2)
      f.write('\n')
    basename = os.path.basename(filepath)
    self.import_config_paths[basename] = filepath
    self.project_import_configs.setdefault(project, []).append(basename)
    self.project_import_configs[project] = sorted(self.project_import_configs[project])
    self._mark_dirty()
    return basename

  def import_config_needs_autocorrect(self, ic_name):
    """Check if import config has hard-coded absolute paths that should be
    converted to portable ${ENV_VAR} form via autocorrect."""
    data = self.load_import_config(ic_name)
    if not data:
      return False
    for key in ["source_dir", "manifest"]:
      val = data.get(key, "")
      if val and os.path.isabs(val):
        return True
    return False

  def autocorrect_import_config(self, ic_name):
    """Rewrite absolute paths in import config using env var substitution."""
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
        new_val = sanitize_path(val)
        if new_val != val:
          data[key] = new_val
          changed = True
      else:
        new_val = resolve_bad_path(val, manifest_dir)
        if new_val != val:
          data[key] = new_val
          changed = True
    if changed:
      with open(path, 'w') as f:
        json.dump(data, f, indent=2)
      self.import_config_data.pop(ic_name, None)
      self._mark_dirty()

  def run_import(self, ic_name, dry_run=False, list_only=False):
    """Run the import operation on a background thread with live output.

    A LiveStream adapter captures output from catalog_import and fires
    on_import_output callbacks so the UI can display progress in real time.
    On success (non-dry-run), automatically rescans the catalog.
    """
    from ork import catalog_import
    path = self.import_config_paths.get(ic_name)
    if not path:
      return

    model = self
    class LiveStream:
      def write(self, msg):
        for line in msg.splitlines():
          if line:
            model.import_output_lines.append(line)
        model._fire(model.on_import_output, ic_name, msg)
        model._mark_dirty()
      def flush(self):
        pass

    self.import_output_lines = ["Running..."]
    self._fire(self.on_import_started, ic_name)
    self._mark_dirty()
    stream = LiveStream()

    def run_thread():
      try:
        config = catalog_import.load_config(path)
        result = catalog_import.run_import(
          config, upload=False, dry_run=dry_run,
          list_only=list_only, verbose=True, output=stream)
        if not dry_run and not list_only and result.failed_count == 0:
          stream.write("\nImport succeeded. Rescanning catalog...\n")
          self.scan()
          stream.write("Rescan complete.\n")
        elif result.failed_count > 0:
          stream.write(f"\nFailed: {result.failed_count} error(s)\n")
          for err in result.errors:
            stream.write(f"  {err}\n")
      except Exception as e:
        stream.write(f"\nImport error: {e}\n")
      stream.write("\nDone.\n")
      self._fire(self.on_import_complete, ic_name, True)
      self._mark_dirty()

    self._bg(run_thread)

  def list_import_assets(self, config_data):
    """Dry-run asset matching — returns list of (pak_id, files) tuples showing
    which source files each asset definition would include."""
    from ork import catalog_import
    assets_list = []
    for a in config_data.get("assets", []):
      inc = a.get("include", "")
      exc = a.get("exclude", [])
      assets_list.append(catalog_import.AssetDefinition(
        id=a["id"], include=inc, exclude=exc))
    config = catalog_import.ImportConfig(
      namespace=config_data.get("namespace", ""),
      source_dir=config_data.get("source_dir", ""),
      local_loc=config_data.get("local_loc", ""),
      manifest=config_data.get("manifest", ""),
      encryption_key=config_data.get("encryption_key"),
      platforms=config_data.get("platforms", ["mac", "linux"]),
      priority=config_data.get("priority", 0),
      assets=assets_list)
    importer = catalog_import.AssetImporter(config)
    importer.resolve_paths()
    results = importer.list_assets()
    source_dir = str(importer._resolved_source_dir) if importer._resolved_source_dir else ""
    return results, source_dir

  ############################################################################
  # Fetch / Download
  ############################################################################

  def fetch(self, fqid):
    """Start async download of a single asset's chunks from CDN."""
    fetch_req = self.catalog.fetchAsync(fqid)
    self.active_fetches[fqid] = fetch_req  # tracked by poll()
    self._last_fetch_snap = None
    self._fire(self.on_fetch_started, fqid)
    self._mark_dirty()

  def fetch_namespace(self, ns):
    """Fetch all in namespace."""
    for fqid in self.ns_assets.get(ns, []):
      fetch_req = self.catalog.fetchAsync(fqid)
      self.active_fetches[fqid] = fetch_req
    self._last_fetch_snap = None
    self._mark_dirty()

  def cancel_fetch(self, fqid):
    """Cancel active download."""
    req = self.active_fetches.get(fqid)
    if req:
      req.cancel()
      del self.active_fetches[fqid]
      self.catalog.invalidateRequest(fqid)
      entry = self.asset_entries.get(fqid)
      if entry:
        self.scan_chunks(fqid, entry)
      self._last_fetch_snap = None
      self._fire(self.on_fetch_complete, fqid, False)
      self._mark_dirty()

  ############################################################################
  # Upload
  ############################################################################

  def upload(self, fqid):
    """Upload single asset."""
    try:
      self.catalog.uploadAsset(fqid)
    except Exception as e:
      self._fire(self.on_error, f"Upload error: {e}")
    self._mark_dirty()

  def upload_namespace(self, ns):
    """Upload all in namespace."""
    try:
      self.catalog.uploadNamespace(ns)
    except Exception as e:
      self._fire(self.on_error, f"Upload error: {e}")
    self._mark_dirty()

  ############################################################################
  # Verification
  ############################################################################

  def verify_hashes(self, fqid):
    """Verify the whole-file hash by concatenating all chunks and comparing
    against the expected file_hash in the chunk manifest. This is a stronger
    check than per-chunk hashes — it catches ordering or truncation issues."""
    from orkengine.core import xxhash64_chunk
    self.hash_verify_results[fqid] = "verifying..."
    self._mark_dirty()
    entry = self.asset_entries.get(fqid)
    if not entry:
      self.hash_verify_results[fqid] = "no entry"
      self._mark_dirty()
      return
    cm = entry.chunk_manifest if hasattr(entry, 'chunk_manifest') else None
    if not cm or not hasattr(cm, 'file_hash') or not cm.file_hash:
      self.hash_verify_results[fqid] = "no file hash in manifest"
      self._mark_dirty()
      return
    cache_dir = self.catalog.cache_dir
    chunks_dir = os.path.join(cache_dir, 'enc', 'chunks')
    # Reassemble all chunks in order and hash the result
    all_data = bytearray()
    for i in range(len(cm.chunks)):
      cp = os.path.join(chunks_dir, f"{entry.storage_hash}.chunk.{i:04d}")
      if not os.path.exists(cp):
        self.hash_verify_results[fqid] = f"incomplete (chunk {i} missing)"
        self._mark_dirty()
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
    self._fire(self.on_verify_complete, fqid, self.hash_verify_results[fqid])
    self._mark_dirty()

  def verify_cdn(self, fqid):
    """Check CDN chunk presence for a single asset via the verify API.

    Sends a batch POST to the CDN's /api/<endpoint>/verify endpoint with
    chunk filenames and expected hashes. The CDN responds with a 'present'
    boolean per chunk. Results are stored in self.cdn_status[fqid].
    """
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
    # Derive the verify API URL from the download URL pattern
    # e.g. https://cdn.example.com/repo/myendpoint/download
    #   -> https://cdn.example.com/repo/api/myendpoint/verify
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
    self._mark_dirty()

  def verify_cdn_batch(self, fqids):
    """Verify multiple assets in a single POST per namespace.

    Groups assets by namespace (each namespace has its own CDN endpoint),
    collects all chunk requests into one batch, and maps the flat response
    array back to per-asset cdn_status lists using boundary tracking.
    """
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

      all_chunks = []       # flat list of all chunk requests for this namespace
      boundaries = []       # (fqid, start_index, count) to slice results back
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

    self._mark_dirty()

  def validate_cdn_all(self):
    """Verify CDN presence for all assets that have local chunks.
    Calls verify_cdn_batch per namespace. Results populate self.cdn_status."""
    all_fqids = []
    for ns in self.namespaces:
      for fqid in self.ns_assets.get(ns, []):
        cs = self.chunk_status.get(fqid, [])
        # Only verify assets that have at least one local chunk
        if cs and any(cs):
          all_fqids.append(fqid)
    if all_fqids:
      self.verify_cdn_batch(all_fqids)

  ############################################################################
  # Local cache
  ############################################################################

  def clear_local(self, fqid):
    """Delete all local files for an asset: chunk files, encrypted archive,
    and the dearchived output directory/file."""
    import shutil
    entry = self.asset_entries.get(fqid)
    if not entry:
      return
    cache_dir = self.catalog.cache_dir

    if hasattr(entry, 'chunk_manifest') and entry.chunk_manifest:
      chunks_dir = os.path.join(cache_dir, 'enc', 'chunks')
      for i in range(len(entry.chunk_manifest.chunks)):
        chunk_file = f"{entry.storage_hash}.chunk.{i:04d}"
        chunk_path = os.path.join(chunks_dir, chunk_file)
        if os.path.exists(chunk_path):
          os.remove(chunk_path)

    if hasattr(entry, 'storage_hash') and entry.storage_hash:
      enc_path = os.path.join(cache_dir, 'enc', f"{entry.storage_hash}.enc")
      if os.path.exists(enc_path):
        os.remove(enc_path)

    if hasattr(entry, 'resolved_local_path') and entry.resolved_local_path:
      local_path = str(entry.resolved_local_path)
      if os.path.isdir(local_path):
        shutil.rmtree(local_path)
      elif os.path.isfile(local_path):
        os.remove(local_path)

    self.catalog.invalidateRequest(fqid)
    self.scan_chunks(fqid, entry)
    self._fire(self.on_chunk_status_changed, fqid)
    self._mark_dirty()

  ############################################################################
  # Refresh
  ############################################################################

  def refresh(self):
    """Reload manifests + rescan + re-ping."""
    core.AssetCatalog.reloadAllManifests(self.catalog)
    self.scan()
    self.check_cdn_health()

  ############################################################################
  # Polling (called from update loop)
  ############################################################################

  def poll(self):
    """Called from the UI's timer/update loop to drive async progress.

    This is the main mechanism for bridging async C++ fetch operations with
    the UI. It checks for completed downloads, detects progress changes via
    snapshot comparison, and triggers periodic CDN health re-pings.

    Returns True if any state changed (so the UI knows to refresh).
    """
    changed = False

    # 1. Check for completed fetches — do a full hash-verified scan
    done = []
    for fqid, req in self.active_fetches.items():
      if req.completed:
        done.append(fqid)
        entry = self.asset_entries.get(fqid)
        if entry:
          self.scan_chunks(fqid, entry)  # deep scan with hash verify
        changed = True
    for fqid in done:
      del self.active_fetches[fqid]
      self._fire(self.on_fetch_complete, fqid, True)

    # 2. Detect in-progress download changes via snapshot comparison
    if self.active_fetches:
      new_snap = {}
      for fqid, req in self.active_fetches.items():
        new_snap[fqid] = (req.chunks_completed, req.bytes_downloaded)
      if new_snap != self._last_fetch_snap:
        self._last_fetch_snap = new_snap
        for fqid in self.active_fetches:
          entry = self.asset_entries.get(fqid)
          if entry:
            self.scan_chunks_fast(fqid, entry)  # fast presence-only scan
        changed = True
    elif self._last_fetch_snap:
      self._last_fetch_snap = None
      changed = True

    # 3. Periodic CDN re-ping (non-blocking, runs on bg thread)
    if not self.cdn_ping_running:
      if time.time() - self.cdn_ping_time >= self.cdn_ping_interval:
        self.cdn_ping_running = True
        self._bg(self.check_cdn_health)

    if changed:
      self._mark_dirty()
    return changed
