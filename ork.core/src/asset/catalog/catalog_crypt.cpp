////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/crypt.h>
#include <ork/util/tar.h>
#include <boost/filesystem.hpp>
#include <regex>
#include <thread>
#include <chrono>
#include <cstdio>
#include "catalog_impl.h"

namespace ork::asset::catalog {
////////////////////////////////////////////////////////////////
// Codec Management
////////////////////////////////////////////////////////////////

void AssetCatalog::registerCodec(const namespaceid_t& namespace_id, encryptioncodec_ptr_t codec) {
  auto impl = _impl.getShared<CatalogImpl>();

  // Direct mutation with proper locking
  impl->_state.atomicOp([&](CatalogImpl::CatalogState& state) { state._codecs_by_namespace[namespace_id] = codec; });

}

/////////////////////////////////////////////////////////////////////////////////

void AssetCatalog::registerCodecWithPassword(const namespaceid_t& namespace_id, const std::string& password) {
  // Create codec from password using the factory function
  auto libsodium_codec = util::crypt::createCodec(password, namespace_id);
  // Cast to base type for registration
  encryptioncodec_ptr_t codec = std::static_pointer_cast<EncryptionCodec>(libsodium_codec);
  registerCodec(namespace_id, codec);
}

/////////////////////////////////////////////////////////////////////////////////

encryptioncodec_ptr_t AssetCatalog::codecForNamespace(const namespaceid_t& namespace_id) const {
  auto impl                    = _impl.getShared<CatalogImpl>();
  encryptioncodec_ptr_t result = nullptr;
  impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
    
    // Check direct match first
    auto it = state._codecs_by_namespace.find(namespace_id);
    if (it != state._codecs_by_namespace.end()) {
      result = it->second;
    } else {
      printf("[DEBUG] No codec found for namespace: %s\n", namespace_id.c_str());
    }

    // TODO: Check parent namespaces for inheritance
  });
  return result;
}

/////////////////////////////////////////////////////////////////////////////////

void AssetCatalog::clearCodecs() {
  auto impl = _impl.getShared<CatalogImpl>();

  // Direct mutation with proper locking
  impl->_state.atomicOp([&](CatalogImpl::CatalogState& state) { state._codecs_by_namespace.clear(); });

}

} //  namespace ork::asset::catalog {
