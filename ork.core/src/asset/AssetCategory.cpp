////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/asset/AssetCategory.h>
#include <ork/asset/VirtualAsset.h>
#include <ork/application/application.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/reflect/IObjectProperty.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace asset {
///////////////////////////////////////////////////////////////////////////////

const VarMap AssetCategory::novars() {
  return VarMap();
}

AssetCategory::AssetCategory(const rtti::RTTIData& data)
    : object::ObjectCategory(data) {
}

///////////////////////////////////////////////////////////////////////////////

bool AssetCategory::serializeObject(reflect::ISerializer& serializer, const rtti::ICastable* object) const {
  const Asset* asset = rtti::safe_downcast<const Asset*>(object);

  PoolString name = asset->GetName();
  PoolString type = asset->GetType();

  bool result = true;

  if (false == serializer.Serialize(ConstString(type)))
    result = false;

  if (name.c_str()) {
    if (false == serializer.Serialize(ConstString(name)))
      result = false;
  } else {
    if (false == serializer.Serialize(ConstString("none")))
      result = false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AssetCategory::deserializeObject(reflect::IDeserializer& deserializer, rtti::ICastable*& value) const {
  bool result = true;

  ArrayString<1024> buffer1;
  ArrayString<1024> buffer2;
  MutableString asset_type(buffer1);
  MutableString asset_name(buffer2);

  if (false == deserializer.Deserialize(asset_type))
    result = false;

  if (false == deserializer.Deserialize(asset_name))
    result = false;

  if (result) {
    Class* asset_clazz = Class::FindClass(asset_type);

    if (asset_name == "none") {
      value = 0;
    } else if (asset_clazz->IsSubclassOf(Asset::GetClassStatic())) {

      const auto& anno = deserializer._currentProperty->annotation(ConstString("asset.deserialize.vargen"));
      if (auto as_gen = anno.TryAs<vars_gen_t>()) {
        const auto& assetvars = as_gen.value()(deserializer._currentObject);
        auto declared         = DeclareAsset(asset_type, asset_name, assetvars);
        value                 = declared.get();
      } else {
        auto declared = DeclareAsset(asset_type, asset_name);
        value         = declared.get();
      }
    }
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////

void AssetCategory::AddTypeAlias(PoolString name, AssetClass* clazz) {
  mTypeAliasMap[name] = clazz;
}

///////////////////////////////////////////////////////////////////////////////

AssetClass* AssetCategory::FindAssetClass(PieceString type) const {
  PoolString asset_type = ork::FindPooledString(type);

  AssetClass* asset_class = rtti::autocast(rtti::Class::FindClass(asset_type));

  if (asset_class == NULL) {
    asset_class = OldStlSchoolFindValFromKey(mTypeAliasMap, FindPooledString(asset_type), NULL);
  }

  return asset_class;
}

///////////////////////////////////////////////////////////////////////////////

AssetCategory::asset_ptr_t AssetCategory::FindAsset(PieceString type, PieceString name) const {
  orkprintf("AssetCategory::FindAsset type<%s> name<%s>\n", ork::AddPooledString(type).c_str(), ork::AddPooledString(name).c_str());
  AssetClass* clazz = FindAssetClass(type);
  if (clazz) {
    return clazz->FindAsset(name);
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////

AssetCategory::asset_ptr_t AssetCategory::DeclareAsset(PieceString type, PieceString name, const VarMap& vmap) const {
  AssetClass* clazz = FindAssetClass(type);
  if (clazz) {
    return clazz->DeclareAsset(name, vmap);
  } else {
    auto result = std::make_shared<VirtualAsset>();
    result->SetType(ork::AddPooledString(type));
    result->SetName(ork::AddPooledString(name));
    return result;
  }
}
///////////////////////////////////////////////////////////////////////////////

AssetCategory::asset_ptr_t AssetCategory::LoadUnManagedAsset(PieceString type, PieceString name) const {
  AssetClass* clazz = FindAssetClass(type);

  if (clazz) {
    return clazz->LoadUnManagedAsset(name);
  } else {
    auto result = std::make_shared<VirtualAsset>();
    result->SetType(ork::AddPooledString(type));
    result->SetName(ork::AddPooledString(name));
    return result;
  }
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::asset
///////////////////////////////////////////////////////////////////////////////
