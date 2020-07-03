///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>
#include <ork/object/ObjectClass.h>
#include <ork/file/path.h>
#include <set>

namespace ork {
class PieceString;
};

namespace ork::asset {

class AssetLoader {
public:
  virtual bool CheckAsset(const AssetPath&)    = 0;
  virtual bool LoadAsset(asset_ptr_t asset)    = 0;
  virtual void DestroyAsset(asset_ptr_t asset) = 0;

  virtual std::set<file::Path> EnumerateExisting() = 0;
};

using loader_ptr_t = std::shared_ptr<AssetLoader>;
////////////////////////////////////////////////////////////////////////////////
template <typename AssetType> //
inline void registerLoader(loader_ptr_t loader) {
  // todo support multiple loaders per asset type
  // todo support flyweighting
  auto assetclazz = dynamic_cast<object::ObjectClass*>(AssetType::GetClassStatic());
  object::ObjectClass::anno_t anno;
  anno.Set<asset::loader_ptr_t>(loader);
  assetclazz->annotate("ork.asset.loader", anno);
}
////////////////////////////////////////////////////////////////////////////////
template <typename AssetType> //
inline loader_ptr_t getLoader() {
  auto assetclazz = dynamic_cast<object::ObjectClass*>(AssetType::GetClassStatic());
  auto loaderanno = assetclazz->annotation("ork.asset.loader");
  auto loader     = loaderanno.Get<asset::loader_ptr_t>();
  return loader;
}
////////////////////////////////////////////////////////////////////////////////
inline loader_ptr_t getLoader(rtti::Class* clazz) {
  auto assetclazz = dynamic_cast<object::ObjectClass*>(clazz);
  auto loaderanno = assetclazz->annotation("ork.asset.loader");
  auto loader     = loaderanno.Get<asset::loader_ptr_t>();
  return loader;
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::asset
