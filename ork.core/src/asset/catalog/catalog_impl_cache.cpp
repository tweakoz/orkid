////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/asset/catalog/config.h>
#include <ork/asset/catalog/request.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/crypt.h>
#include <ork/util/tar.h>
#include <ork/util/logger.h>
#include <ork/util/md5.h>
#include <ork/util/xxhash.inl>
#include <ork/util/password_provider.h>
#include <boost/filesystem.hpp>
#include <regex>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "catalog_impl.h"

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////
// Cache Helper Functions
////////////////////////////////////////////////////////////////

file::Path CatalogImpl::getCachePathForAsset(assetfqid_ptr_t fqid) const {
  auto ainfo = fqid->_asset_info;
  return _catalog->_cache_dir / "enc" / (ainfo->_storage_hash + ".enc");
}

////////////////////////////////////////////////////////////////

file::Path CatalogImpl::getCachePathForChunk(assetfqid_ptr_t fqid, size_t chunk_index) const {
  auto ainfo = fqid->_asset_info;
  std::string chunk_filename = FormatString("%s.enc.chunk.%04zu", ainfo->_storage_hash.c_str(), chunk_index);
  return _catalog->_cache_dir / "enc" / "chunks" / chunk_filename;
}

////////////////////////////////////////////////////////////////

bool CatalogImpl::verifyCachedFileHash(const file::Path& cache_path, const std::string& expected_hash) const {
  // Read file and compute MD5 hash
  if (!cache_path.doesPathExist()) {
    return false;
  }

  try {
    File file(cache_path, EFM_READ);
    size_t file_size = 0;
    file.GetLength(file_size);

    std::vector<uint8_t> data;
    data.resize(file_size);
    file.Read(data.data(), file_size);
    file.Close();

    // Compute MD5 hash
    CMD5 hasher;
    hasher.update(data.data(), data.size());
    hasher.finalize();
    std::string computed_hash = hasher.Result().hex_digest();

    bool matches = (computed_hash == expected_hash);
    if (!matches) {
      logchan_catalog->log(
          "Cache hash mismatch for %s: expected %s, got %s", cache_path.c_str(), expected_hash.c_str(), computed_hash.c_str());
    }
    return matches;
  } catch (...) {
    return false;
  }
}

////////////////////////////////////////////////////////////////

bool CatalogImpl::verifyCachedChunkHash(const file::Path& cache_path, chunk_hash_t expected_hash) const {
  // Read file and compute XXHash64
  if (!cache_path.doesPathExist()) {
    return false;
  }

  try {
    File file(cache_path, EFM_READ);
    size_t file_size = 0;
    file.GetLength(file_size);

    std::vector<uint8_t> data;
    data.resize(file_size);
    file.Read(data.data(), file_size);
    file.Close();

    // Compute XXHash64
    auto xxhasher = std::make_shared<XXH64HASH>();
    xxhasher->init();
    xxhasher->accumulate(data.data(), data.size());
    xxhasher->finish();
    chunk_hash_t computed_hash = xxhasher->result();

    bool matches = (computed_hash == expected_hash);
    if (!matches) {
      logchan_catalog->log(
          "Chunk hash mismatch for %s: expected %016llx, got %016llx",
          cache_path.c_str(),
          (unsigned long long)expected_hash,
          (unsigned long long)computed_hash);
    }
    return matches;
  } catch (...) {
    return false;
  }
}


////////////////////////////////////////////////////////////////

bool CatalogImpl::saveToCacheFile(const datablock_ptr_t& data, const file::Path& cache_path) const {
  try {
    // Ensure cache directory exists
    file::Path cache_dir = cache_path;
    cache_dir.setFile(""); // Remove filename to get directory
    cache_dir.ensureDirectoryExists();

    // Write data to cache file
    File file(cache_path, EFM_WRITE);
    file.Write(data->data(), data->length());
    file.Close();

    //logchan_catalog->log("Cached file saved: %s (%zu bytes)", cache_path.c_str(), data->length());
    return true;
  } catch (const std::exception& e) {
    logchan_catalog->log("ERROR: Failed to save cache file %s: %s", cache_path.c_str(), e.what());
    return false;
  }
}

} //namespace ork::asset::catalog {
