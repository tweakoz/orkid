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

  static bool _enabled;
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
