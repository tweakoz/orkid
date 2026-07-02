////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/datacache.h>
#include <ork/file/fileenv.h>
#include <ork/file/path.h>
#include <ork/util/crc64.h>
#include <ork/kernel/environment.h>
#include <boost/filesystem.hpp>
#include <ork/util/logger.h>
#include <algorithm>
#include <vector>
#include <ctime>

namespace ork {

logchannel_ptr_t logchan_dcache = logger()->configureChannel("DCACHE", fvec3(0.5, 0.5, 0.5), true);

bool DataBlockCache::_enabled = true;
DataBlockCache::DataBlockCache() {
  if( genviron.has("ORKID_DISABLE_DBLOCK_CACHING") ){
    _enabled = false;
  }
}
DataBlockCache::~DataBlockCache() {
}
//////////////////////////////////////////////////////////////////////////////
// bump a cache file's mtime on read so evictToSize behaves as a cross-run LRU
// (most-recently-used survives). best-effort — failures are ignored.
static void _touchCacheFile(const std::string& cache_path) {
  using namespace boost::filesystem;
  boost::system::error_code ec;
  last_write_time(cache_path, std::time(nullptr), ec);
}
//////////////////////////////////////////////////////////////////////////////
// in-memory retention policy: ONLY the legacy default namespace retains blocks in
// _blockmap (shaders/textures/xgm rely on repeated in-memory hits). Namespaced
// caches (dflowcache) hold hundreds of 16-64MB cook planes — retain-forever pinned
// 40-80GB of host RSS across a cold terrain/hypermesh/eflow cook (WS4/RSS fix
// 2026-07-02); they are write-through/read-through and the OS page cache absorbs
// intra-process re-reads.
static bool _retainInMemory(const std::string& cacheName) {
  return cacheName == DataBlockCache::kDefaultCache;
}

// plain disk read of a cache entry (no map interaction). max_len 0 = whole file.
static datablock_ptr_t _readFromDisk(const std::string& cache_path, size_t max_len) {
  using namespace boost::filesystem;
  if (not exists(cache_path))
    return nullptr;
  FILE* fin = fopen(cache_path.c_str(), "rb");
  if (not fin) // guard: a stat'd-but-unopenable file (perms/race) is a miss, not a crash
    return nullptr;
  size_t len = file_size(cache_path);
  if (max_len and len > max_len)
    len = max_len;
  auto rval   = std::make_shared<DataBlock>();
  rval->_name = cache_path;
  rval->reserve(len);
  void* pdata    = malloc(len);
  size_t numread = fread(pdata, 1, len, fin);
  OrkAssert(numread == len);
  fclose(fin);
  rval->addData(pdata, len);
  free(pdata);
  return rval;
}

// shared find core. The in-memory _blockmap is keyed by the content hash alone
// (a CAS invariant: same hash => same bytes regardless of namespace), so only
// the on-disk path is namespaced. touchOnHit is enabled for the namespaced API
// (evictable caches) and off for the legacy default (preserves old behavior).
static datablock_ptr_t _findCore(const std::string& cacheName, uint64_t key, bool touchOnHit) {
  if (not DataBlockCache::_enabled)
    return nullptr;
  auto& inst           = DataBlockCache::instance();
  datablock_ptr_t rval = nullptr;
  auto cache_path      = DataBlockCache::_generateCachePath(cacheName, key);
  const bool retain    = _retainInMemory(cacheName);
  // the map may hold the block regardless of namespace (CAS: same hash = same bytes)
  inst._blockmap.atomicOp([&rval, key](datablockmap_t& m) {
    auto it = m.find(key);
    if (it != m.end())
      rval = it->second;
  });
  if (not rval) {
    rval = _readFromDisk(cache_path, 0);
    if (rval and retain)
      inst._blockmap.atomicOp([&rval, key](datablockmap_t& m) { m[key] = rval; });
  }
  if (rval and touchOnHit)
    _touchCacheFile(cache_path);
  return rval;
}
//////////////////////////////////////////////////////////////////////////////
std::string DataBlockCache::_generateCachePath(const std::string& cacheName, uint64_t key) {
  using namespace boost::filesystem;
  auto cache_dir = file::Path::stage_dir() / cacheName.c_str();
  if (false == exists(cache_dir.toBFS())) {
    logchan_dcache->log("Making cache_dir folder<%s>", cache_dir.c_str());
    create_directory(cache_dir.toBFS());
  }
  auto cache_path = cache_dir / FormatString("%zx.bin", key);
  return cache_path.toStdString();
}
std::string DataBlockCache::_generateCachePath(uint64_t key) {
  return _generateCachePath(kDefaultCache, key); // <staging>/dblockcache (unchanged)
}
//////////////////////////////////////////////////////////////////////////////
datablock_ptr_t DataBlockCache::findDataBlock(const std::string& cacheName, uint64_t key) {
  return _findCore(cacheName, key, /*touchOnHit*/ true);
}
datablock_ptr_t DataBlockCache::findDataBlockPrefix(const std::string& cacheName, uint64_t key, size_t max_len) {
  if (not _enabled)
    return nullptr;
  // never serves from / inserts into the in-memory map (a truncated block there
  // would poison a later full read) and never touches mtime (planning must not
  // perturb the eviction LRU).
  return _readFromDisk(_generateCachePath(cacheName, key), max_len);
}
datablock_ptr_t DataBlockCache::findDataBlock(uint64_t key) {
  return _findCore(kDefaultCache, key, /*touchOnHit*/ false);
}
//////////////////////////////////////////////////////////////////////////////
void DataBlockCache::setDataBlock(const std::string& cacheName, uint64_t key, datablock_ptr_t item, bool cacheable) {
  auto& inst      = instance();
  auto cache_path = _generateCachePath(cacheName, key);
  if (_retainInMemory(cacheName)) {
    inst._blockmap.atomicOp([item, key](datablockmap_t& m) { m[key] = item; });
  } // namespaced: write-through only — no in-memory retention (see _retainInMemory)
  if (cacheable) {
    logchan_dcache->log("writing to cache <%s>", cache_path.c_str());
    FILE* fout = fopen(cache_path.c_str(), "wb");
    if (fout) { // guard: disk-full / perms during a large bake should skip, not crash
      fwrite(item->data(), item->length(), 1, fout);
      fclose(fout);
    } else {
      logchan_dcache->log("WARNING: cache write failed (could not open) <%s>", cache_path.c_str());
    }
  }
}
void DataBlockCache::setDataBlock(uint64_t key, datablock_ptr_t item, bool cacheable) {
  setDataBlock(kDefaultCache, key, item, cacheable);
}
//////////////////////////////////////////////////////////////////////////////
void DataBlockCache::removeDataBlock(const std::string& cacheName, uint64_t key) {
  auto& inst = instance();
  auto cache_path = _generateCachePath(cacheName, key);
  inst._blockmap.atomicOp([key](datablockmap_t& m) {
    auto it = m.find(key);
    if (it != m.end())
      m.erase(it);
  });
  using namespace boost::filesystem;
  boost::system::error_code ec;
  remove(cache_path, ec); // also drop the on-disk entry in this namespace
}
void DataBlockCache::removeDataBlock(uint64_t key) {
  auto& inst = instance();
  inst._blockmap.atomicOp([key](datablockmap_t& m) {
    auto it = m.find(key);
    if (it != m.end())
      m.erase(it); // legacy: memory-only removal (unchanged)
  });
}
//////////////////////////////////////////////////////////////////////////////
void DataBlockCache::evictToSize(const std::string& cacheName, uint64_t maxBytes) {
  using namespace boost::filesystem;
  auto cache_dir = (file::Path::stage_dir() / cacheName.c_str()).toBFS();
  boost::system::error_code ec;
  if (not exists(cache_dir, ec))
    return;
  struct Entry {
    std::string _path;
    uint64_t    _size;
    std::time_t _mtime;
  };
  std::vector<Entry> entries;
  uint64_t total = 0;
  for (directory_iterator it(cache_dir, ec), end; it != end; ++it) {
    if (not is_regular_file(it->status()))
      continue;
    const auto& p = it->path();
    if (p.extension() != ".bin")
      continue;
    auto sz = file_size(p, ec);
    if (ec) continue;
    auto mt = last_write_time(p, ec);
    if (ec) continue;
    entries.push_back({p.string(), uint64_t(sz), mt});
    total += uint64_t(sz);
  }
  if (total <= maxBytes)
    return;
  // oldest (smallest mtime) first — those are the LRU eviction candidates.
  std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    return a._mtime < b._mtime;
  });
  uint64_t freed = 0, removed = 0;
  for (const auto& e : entries) {
    if (total <= maxBytes)
      break;
    boost::system::error_code rec;
    remove(e._path, rec);
    if (not rec) {
      total -= e._size;
      freed += e._size;
      removed++;
    }
  }
  logchan_dcache->log(
      "evictToSize<%s>: removed %llu files, freed %.1f MB, now %.1f MB (cap %.1f MB)",
      cacheName.c_str(),
      (unsigned long long)removed,
      double(freed) / (1024.0 * 1024.0),
      double(total) / (1024.0 * 1024.0),
      double(maxBytes) / (1024.0 * 1024.0));
}
//////////////////////////////////////////////////////////////////////////////
size_t DataBlockCache::totalMemoryConsumed() {
  auto& inst  = instance();
  size_t rval = 0;
  inst._blockmap.atomicOp([&rval](datablockmap_t& m) {
    for (auto i : m)
      rval += i.second->length();
  });
  return rval;
}
//////////////////////////////////////////////////////////////////////////////
DataBlockCache& DataBlockCache::instance() {
  static DataBlockCache _mgr;
  return _mgr;
}

} // namespace ork
