#pragma once
#include <ork/kernel/mutex.h>
#include <ork/kernel/datablock.inl>
#include <unordered_map>

namespace ork {

typedef std::unordered_map<uint64_t, datablockptr_t> datablockmap_t;

struct DataBlockCache {
  static bool _enabled;
  static std::string _generateCachePath(uint64_t key);
  static datablockptr_t findDataBlock(uint64_t key);
  static void setDataBlock(uint64_t key, datablockptr_t item, bool cacheable = true);
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
