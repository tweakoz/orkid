////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#if defined(ORK_IOS)

#include <ork/asset/catalog/catalog.h>

namespace ork::asset::catalog {

void AssetCatalog::registerCodecWithPassword(const namespaceid_t& namespace_id, const std::string& password) {
  // iOS: Encryption not supported
}

encryptioncodec_ptr_t AssetCatalog::codecForNamespace(const namespaceid_t& namespace_id) const {
  // iOS: Encryption not supported
  return nullptr;
}

void AssetCatalog::clearCodecs() {
  // iOS: Encryption not supported
}

datablock_ptr_t AssetCatalog::_packFromLocal(assetfqid_ptr_t fqid) {
  // iOS: Tar packing not supported
  return nullptr;
}

} //  namespace ork::asset::catalog {

#endif // ORK_IOS
