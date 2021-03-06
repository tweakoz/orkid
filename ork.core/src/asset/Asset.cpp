
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/asset/Asset.h>
#include <ork/asset/AssetSetEntry.h>

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::asset::Asset, "Asset");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace asset {
///////////////////////////////////////////////////////////////////////////////

const vars_t novars() {
  static const vars_t gnovars;
  return gnovars;
}

void Asset::describeX(object::ObjectClass* clazz) {
}

Asset::Asset() {
}

///////////////////////////////////////////////////////////////////////////////

void Asset::setName(AssetPath name) {
  _name = name;
}

///////////////////////////////////////////////////////////////////////////////

AssetPath Asset::name() const {
  return _name;
}

///////////////////////////////////////////////////////////////////////////////

assetset_ptr_t Asset::assetSet() const {
  auto objclazz  = rtti::safe_downcast<object::ObjectClass*>(GetClass());
  auto aset_anno = objclazz->annotation("AssetSet");
  auto asset_set = aset_anno.Get<assetset_ptr_t>();
  return asset_set;
}

///////////////////////////////////////////////////////////////////////////////

std::string Asset::type() const {
  auto objclazz = rtti::safe_downcast<object::ObjectClass*>(GetClass());
  return objclazz->Name().c_str();
}

///////////////////////////////////////////////////////////////////////////////

bool Asset::Load() const {
  auto entry     = assetSetEntry(this);
  auto asset_set = assetSet();
  return entry->Load(asset_set->topLevel());
}

bool Asset::LoadUnManaged() const {
  AssetSetEntry* entry = assetSetEntry(this);
  auto asset_set       = assetSet();
  return entry->Load(asset_set->topLevel());
}

///////////////////////////////////////////////////////////////////////////////

bool Asset::IsLoaded() const {
  AssetSetEntry* entry = assetSetEntry(this);

  return entry && entry->IsLoaded();
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::asset
///////////////////////////////////////////////////////////////////////////////
