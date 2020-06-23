////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/asset/Asset.h>
#include <ork/asset/AssetClass.h>
#include <ork/asset/AssetLoader.h>
#include <ork/asset/AssetSet.h>
#include <ork/asset/AssetSetLevel.h>
#include <ork/asset/AssetSetEntry.h>
#include <ork/asset/FileAssetNamer.h>
#include <ork/asset/VirtualAsset.h>
#include <ork/kernel/string/ArrayString.h>

#include <ork/application/application.h> // For Add/FindPooledString

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::asset::AssetClass, "AssetClass")

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace asset {
///////////////////////////////////////////////////////////////////////////////

const VarMap AssetClass::novars() {
  return VarMap();
}

void AssetClass::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

AssetClass::AssetClass(const rtti::RTTIData& data)
    : object::ObjectClass(data)
    , mAssetNamer(NULL) {
  _assetset = std::make_shared<AssetSet>();
}

std::set<file::Path> AssetClass::EnumerateExisting() const {
  std::set<file::Path> rval;

  for (auto& l : mLoaders) {
    auto s = l->EnumerateExisting();
    for (auto i : s) {
      // printf( "enumexist loader<%p> asset<%s>\n", l, i.c_str() );
      rval.insert(i);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void AssetClass::AddLoader(AssetLoader* loader) {
  mLoaders.insert(loader);
}

///////////////////////////////////////////////////////////////////////////////

class Renamer {
  ArrayString<128> mRenamed;

public:
  Renamer(FileAssetNamer* renamer, PieceString& name);
};

///////////////////////////////////////////////////////////////////////////////

FileAssetNamer* AssetClass::GetAssetNamer() const {
  return mAssetNamer;
}

///////////////////////////////////////////////////////////////////////////////

AssetLoader* AssetClass::FindLoader(PieceString name) {
  Renamer fix_name(GetAssetNamer(), name);

  for (auto it : mLoaders) {
    if (it->CheckAsset(name))
      return it;
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////

AssetClass::asset_ptr_t AssetClass::CreateUnmanagedAsset(PieceString name, const VarMap& vmap) {
  Renamer fix_name(GetAssetNamer(), name);

  AssetClass::asset_ptr_t asset;

  if (false == Class::hasFactory()) {
    auto vasset = std::make_shared<VirtualAsset>();
    vasset->SetType(Name());

    asset = vasset;
  } else {
    auto raw_asset_ptr = (Asset*)CreateObject();
    asset              = AssetClass::asset_ptr_t(raw_asset_ptr);
  }

  asset->SetName(ork::AddPooledString(name));
  asset->_varmap = vmap;

  return asset;
}

///////////////////////////////////////////////////////////////////////////////

AssetClass::asset_ptr_t AssetClass::FindAsset(PieceString name, const VarMap& vmap) {
  Renamer fix_name(GetAssetNamer(), name);

  PoolString name_string = FindPooledString(name);

  return _assetset->FindAsset(name_string);
}

///////////////////////////////////////////////////////////////////////////////

AssetClass::asset_ptr_t AssetClass::DeclareAsset(PieceString name, const VarMap& vmap) {
  // Editor support that allows nulling out a previously set asset name.
  if (name.empty()) {
    return nullptr;
  }

  if (auto asset = FindAsset(name, vmap)) {
    _assetset->Register(asset->GetName(), asset);
    return asset;
  }

  auto new_asset = CreateUnmanagedAsset(name, vmap);
  auto loader    = FindLoader(name);
  _assetset->Register(new_asset->GetName(), new_asset, loader);

  return new_asset;
}

///////////////////////////////////////////////////////////////////////////////

AssetClass::asset_ptr_t AssetClass::LoadUnManagedAsset(PieceString name, const VarMap& vmap) {
  // Editor support that allows nulling out a previously set asset name.
  if (name.empty()) {
    return nullptr;
  }

  auto new_asset = CreateUnmanagedAsset(name, vmap);

  auto loader = FindLoader(name);

  loader->LoadAsset(new_asset);
  return new_asset;
}

///////////////////////////////////////////////////////////////////////////////

AssetClass::assetset_ptr_t AssetClass::assetSet() {
  return _assetset;
}

///////////////////////////////////////////////////////////////////////////////

bool AssetClass::AutoLoad(int depth) {
  return _assetset->Load(depth);
}

void AssetClass::SetAssetNamer(const std::string& namer) {
  mAssetNamer = new FileAssetNamer(namer.c_str());
}

///////////////////////////////////////////////////////////////////////////////

void AssetClass::AddTypeAlias(ConstString alias) {
  PoolString thealias = ork::AddPooledString(alias);
  GetClassStatic()->AddTypeAlias(thealias, this);
}

///////////////////////////////////////////////////////////////////////////////

Renamer::Renamer(FileAssetNamer* renamer, PieceString& name) {
  if (renamer) {
    if (renamer->Canonicalize(mRenamed, name))
      name = mRenamed;
  }
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::asset
///////////////////////////////////////////////////////////////////////////////
