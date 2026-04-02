################################################################################
# Catalog Tool — Model (business logic, no UI imports)
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
  """Extract project name from manifest source_file path."""
  parts = source_file.replace("\\", "/").split("/")
  for i, p in enumerate(parts):
    if p == "asset_manifests" and i >= 2:
      parent = parts[i - 1]
      grandparent = parts[i - 2]
      if parent in ("obt.project", "ork.data"):
        return grandparent
      return parent
  return "unknown"

def sanitize_path(abspath):
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

def resolve_bad_path(bad_path, manifest_dir):
  """Try to resolve a bad absolute path (from another machine) to a local path,
     then sanitize with env var prefix."""
  if not bad_path or not os.path.isabs(bad_path):
    return bad_path
  if os.path.exists(bad_path):
    return sanitize_path(bad_path)
  project_root = manifest_dir
  for _ in range(3):
    parent = os.path.dirname(project_root)
    if os.path.basename(project_root) in ("obt.project", "asset_manifests"):
      project_root = parent
    else:
      break
  parts = bad_path.replace("\\", "/").rstrip("/").split("/")
  for i in range(len(parts) - 1, 0, -1):
    suffix = os.path.join(*parts[i:])
    candidate = os.path.join(project_root, suffix)
    if os.path.exists(candidate):
      return sanitize_path(candidate)
  return sanitize_path(project_root)

################################################################################
# VarMap builders (display-ready property dicts)
################################################################################

def build_namespace_varmap(model, ns_id):
  """Build a flat dict of namespace properties."""
  d = {}
  d["Name"] = ns_id
  d["Asset Count"] = str(len(model.ns_assets.get(ns_id, [])))

  merged = model.cfgspc.merged_config
  ek = merged.getEncryptionKeyForNamespace(ns_id) if merged else None
  d["Encryption Key"] = "Yes" if ek else "No"

  remote_loc = model.cfgspc.getNamespaceRemoteLocation(ns_id) or ""
  d["Remote Location"] = remote_loc

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
  """Build a flat dict of asset properties."""
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
    cache_dir = model.catalog.cache_dir
    enc_path = os.path.join(cache_dir, 'enc', f"{entry.storage_hash}.enc")
    d["Encrypted File"] = "Yes" if os.path.exists(enc_path) else "No"
  cm = entry.chunk_manifest if hasattr(entry, 'chunk_manifest') else None
  if cm:
    d["Total Chunks"] = str(len(cm.chunks))
    if hasattr(cm, 'chunk_size'):
      d["Chunk Size"] = format_size(cm.chunk_size)

  cs = model.chunk_status.get(fqid, [])
  total = len(cs)
  present = sum(1 for x in cs if x)
  d["Chunks Local"] = f"{present} / {total}"

  vs = model.chunk_valid.get(fqid, [])
  valid = sum(1 for x in vs if x)
  d["Chunks Valid"] = f"{valid} / {len(vs)}"

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
  """

  def __init__(self):
    # Callbacks (set by frontend)
    self.on_scan_complete = None
    self.on_chunk_status_changed = None
    self.on_fetch_started = None
    self.on_fetch_progress = None
    self.on_fetch_complete = None
    self.on_cdn_health_updated = None
    self.on_import_started = None
    self.on_import_output = None
    self.on_import_complete = None
    self.on_verify_complete = None
    self.on_error = None
    self.on_dirty = None  # generic "something changed" signal

    # Data properties
    self.namespaces = []
    self.projects = []
    self.project_namespaces = {}
    self.project_manifests = {}
    self.project_import_configs = {}
    self.ns_project = {}
    self.ns_manifests = {}
    self.ns_import_configs = {}
    self.ns_assets = {}
    self.asset_entries = {}
    self.asset_manifest_file = {}
    self.chunk_status = {}
    self.chunk_valid = {}
    self.cdn_status = {}
    self.cdn_health = {}
    self.active_fetches = {}
    self.hash_verify_results = {}
    self.import_output_lines = []
    self.import_config_paths = {}
    self.import_config_data = {}

    # Internal
    self.http_session = requests.Session()
    self.cdn_ping_time = 0
    self.cdn_ping_interval = 1.0
    self.cdn_ping_running = False
    self._last_fetch_snap = None

    # Catalog (initialized by init())
    self.cfgspc = None
    self.catalog = None

  def _fire(self, cb, *args):
    if cb:
      cb(*args)

  def _mark_dirty(self):
    self._fire(self.on_dirty)

  def _bg(self, fn):
    t = threading.Thread(target=fn, daemon=True)
    t.start()

  ############################################################################
  # Initialization
  ############################################################################

  def init(self):
    """Initialize catalog and perform initial scan."""
    self.cfgspc, self.catalog = ork_assets.default_cfg_and_catalog()
    self.scan()
    self._bg(self.check_cdn_health)
    # Disable DOWNLOAD log channel
    logger = core.Logger.instance()
    dl_chan = logger.getChannel("DOWNLOAD")
    if dl_chan:
      dl_chan.enabled = False

  ############################################################################
  # Scanning
  ############################################################################

  def scan(self):
    """Full rescan of catalog + chunks."""
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
        self.scan_chunks(fqid, entry)

    self._fire(self.on_scan_complete)
    self._mark_dirty()

  def scan_chunks(self, fqid, entry=None):
    """Deep scan single asset (with hash verify)."""
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
    """Presence-only check — no hash verification."""
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
    """Write JSON to disk."""
    path = self.import_config_paths.get(basename)
    if not path:
      return
    with open(path, 'w') as f:
      json.dump(data, f, indent=2)
      f.write('\n')
    self.import_config_data.pop(basename, None)

  def create_import_config(self, project):
    """Create a new empty import config in the project's manifest dir. Returns basename."""
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
    """Check if import config has absolute paths (should use ${ENV_VAR} form)."""
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
    """Run the import operation with live output."""
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
    """Test match — returns list of (pak_id, files) tuples."""
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
    """Async fetch single asset."""
    fetch_req = self.catalog.fetchAsync(fqid)
    self.active_fetches[fqid] = fetch_req
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
    """Verify assembled file hash from chunks on disk."""
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
    """Check CDN chunk presence for a single asset."""
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
    self._mark_dirty()

  def verify_cdn_batch(self, fqids):
    """Verify multiple assets in a single POST per namespace."""
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

      all_chunks = []
      boundaries = []
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

  ############################################################################
  # Local cache
  ############################################################################

  def clear_local(self, fqid):
    """Delete local chunks + encrypted + dearchived."""
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
    """Poll active fetches, periodic CDN re-ping. Returns True if something changed."""
    changed = False

    # Poll completed fetches
    done = []
    for fqid, req in self.active_fetches.items():
      if req.completed:
        done.append(fqid)
        entry = self.asset_entries.get(fqid)
        if entry:
          self.scan_chunks(fqid, entry)
        changed = True
    for fqid in done:
      del self.active_fetches[fqid]
      self._fire(self.on_fetch_complete, fqid, True)

    # Check fetch progress
    if self.active_fetches:
      new_snap = {}
      for fqid, req in self.active_fetches.items():
        new_snap[fqid] = (req.chunks_completed, req.bytes_downloaded)
      if new_snap != self._last_fetch_snap:
        self._last_fetch_snap = new_snap
        for fqid in self.active_fetches:
          entry = self.asset_entries.get(fqid)
          if entry:
            self.scan_chunks_fast(fqid, entry)
        changed = True
    elif self._last_fetch_snap:
      self._last_fetch_snap = None
      changed = True

    # Periodic CDN re-ping
    if not self.cdn_ping_running:
      if time.time() - self.cdn_ping_time >= self.cdn_ping_interval:
        self.cdn_ping_running = True
        self._bg(self.check_cdn_health)

    if changed:
      self._mark_dirty()
    return changed
