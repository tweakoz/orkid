////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/kernel/mutex.h>
#include <ork/kernel/datablock.h>
#include <unordered_map>

namespace ork {

typedef std::unordered_map<uint64_t, datablock_ptr_t> datablockmap_t;

///////////////////////////////////////////////////////////////////////////////
/// DataBlockCache : on disk and memory repository of DataBlocks,
///  addressable by hash, part of the CAS (content addressable filesystem)
///  only 1 copy will ever be present in memory
///////////////////////////////////////////////////////////////////////////////

struct DataBlockCache {

  // the default (shared) on-disk cache namespace — <staging>/dblockcache.
  static constexpr const char* kDefaultCache = "dblockcache";

  static bool _enabled;

  ////////////////////////////////////////////////////////////////////////////
  // namespaced API: entries live under <staging>/<cacheName>/<hash>.bin, so a
  // subsystem can isolate its (large / cheap-to-regenerate) blobs in their own
  // directory and size-cap them independently (see evictToSize). Reads bump the
  // file's mtime (touch-on-hit) so evictToSize can act as a cross-run LRU.
  ////////////////////////////////////////////////////////////////////////////
  // NAMESPACED entries are NOT retained in the in-memory _blockmap (WS4/RSS fix
  // 2026-07-02): a cold terrain/hypermesh cook stores hundreds of 16-64MB planes and
  // the old retain-forever behavior pinned 40-80GB of host RSS for process lifetime.
  // Namespaced blobs are write-through to disk / read-from-disk (the OS page cache
  // absorbs intra-process re-reads); ONLY the legacy default namespace retains
  // (shaders/textures/xgm rely on repeated in-memory hits).
  static std::string     _generateCachePath(const std::string& cacheName, uint64_t key);
  static datablock_ptr_t findDataBlock(const std::string& cacheName, uint64_t key);
  // header-only read: first min(max_len, filesize) bytes, or nullptr if absent.
  // For hit/miss PLANNING (e.g. terrain cookProbe) — pulling a 64MB plane off disk
  // to inspect a 24-byte header made warm-bake planning read ~33GB it never used.
  // No touch-on-hit (planning must not perturb the eviction LRU; the real
  // findDataBlock at load time touches).
  static datablock_ptr_t findDataBlockPrefix(const std::string& cacheName, uint64_t key, size_t max_len);
  static void            setDataBlock(const std::string& cacheName, uint64_t key, datablock_ptr_t item, bool cacheable = true);
  static void            removeDataBlock(const std::string& cacheName, uint64_t key);
  // Evict oldest-by-mtime *.bin entries from <staging>/<cacheName> until the
  // directory's total size is <= maxBytes. No-op if the dir is missing/empty or
  // already under the cap. Intended to run once at process launch.
  static void            evictToSize(const std::string& cacheName, uint64_t maxBytes);

  ////////////////////////////////////////////////////////////////////////////
  // legacy API — operates on the default shared cache (kDefaultCache); behavior
  // is byte-for-byte unchanged (no touch-on-hit), so existing callers are intact.
  ////////////////////////////////////////////////////////////////////////////
  static std::string _generateCachePath(uint64_t key);
  static datablock_ptr_t findDataBlock(uint64_t key);
  static void setDataBlock(uint64_t key, datablock_ptr_t item, bool cacheable = true);
  static void removeDataBlock(uint64_t key);
  static size_t totalMemoryConsumed();
  static DataBlockCache& instance();

  //////////////////////////////////////////////////////////////////////////////
  DataBlockCache();
  ~DataBlockCache();
  //////////////////////////////////////////////////////////////////////////////
  LockedResource<datablockmap_t> _blockmap;
};
} // namespace ork
