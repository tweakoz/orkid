////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/asset/AssetLoader.h>
#include <ork/util/RingLink.hpp>

namespace ork::asset {
///////////////////////////////////////////////////////////////////////////////

LockedResource<AssetLoader::loader_by_ext_map_t> AssetLoader::_loaders_by_ext;

void AssetLoader::registerLoaderForExtension(std::string extension, assetloader_ptr_t loader){
  _loaders_by_ext.atomicOp([&](loader_by_ext_map_t& unlocked) {
    unlocked[extension] = loader;
  });
}


} //namespace ork::asset {

