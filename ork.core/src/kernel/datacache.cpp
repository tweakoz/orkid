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

namespace ork {

logchannel_ptr_t logchan_dcache = logger()->createChannel("DCACHE", fvec3(0.5, 0.5, 0.5), true);

bool DataBlockCache::_enabled = true;
DataBlockCache::DataBlockCache() {
  if( genviron.has("ORKID_DISABLE_DBLOCK_CACHING") ){
    _enabled = false;
  }
}
DataBlockCache::~DataBlockCache() {
}
//////////////////////////////////////////////////////////////////////////////
std::string DataBlockCache::_generateCachePath(uint64_t key) {
  using namespace boost::filesystem;
  auto cache_dir = file::Path::dblockcache_dir();
  if (false == exists(cache_dir.toBFS())) {
    logchan_dcache->log("Making cache_dir folder<%s>", cache_dir.c_str());
    create_directory(cache_dir.toBFS());
  }
  auto cache_path = cache_dir / FormatString("%zx.bin", key);
  return cache_path.toStdString();
}
//////////////////////////////////////////////////////////////////////////////
datablock_ptr_t DataBlockCache::findDataBlock(uint64_t key) {
  if (not _enabled) {
    return nullptr;
  }
  auto& inst          = instance();
  datablock_ptr_t rval = nullptr;
  auto cache_path     = _generateCachePath(key);
  inst._blockmap.atomicOp([&rval, key, cache_path](datablockmap_t& m) {
    auto it = m.find(key);
    if (it == m.end()) {
      using namespace boost::filesystem;
      if (exists(cache_path)) {
        rval        = std::make_shared<DataBlock>();
        size_t len  = file_size(cache_path);
        rval->_name = cache_path;
        rval->reserve(len);
        FILE* fin      = fopen(cache_path.c_str(), "rb");
        void* pdata    = malloc(len);
        size_t numread = fread(pdata, 1, len, fin);
        OrkAssert(numread == len);
        fclose(fin);
        rval->addData(pdata, len);
        free(pdata);
        m[key] = rval;
      }
      else{
        //printf( "not found in cache <%s>\n", cache_path.c_str() );
      }
    } else {
      rval = it->second;
    }
  });
  return rval;
}
//////////////////////////////////////////////////////////////////////////////
void DataBlockCache::setDataBlock(uint64_t key, datablock_ptr_t item, bool cacheable) {
  auto& inst      = instance();
  auto cache_path = _generateCachePath(key);
  inst._blockmap.atomicOp([item, key, cache_path, cacheable](datablockmap_t& m) {
    auto it = m.find(key);
    // assert(it == m.end());
    m[key] = item;
    using namespace boost::filesystem;
    if (cacheable) {
      logchan_dcache->log( "writing to cache <%s>", cache_path.c_str() );
      FILE* fout = fopen(cache_path.c_str(), "wb");
      fwrite(item->data(), item->length(), 1, fout);
      fclose(fout);
    }
  });
}
//////////////////////////////////////////////////////////////////////////////
void DataBlockCache::removeDataBlock(uint64_t key) {
  auto& inst = instance();
  inst._blockmap.atomicOp([key](datablockmap_t& m) {
    auto it = m.find(key);
    if (it != m.end())
      m.erase(it);
  });
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
