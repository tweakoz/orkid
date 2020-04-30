///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/object/ObjectClass.h>
#include <ork/rtti/RTTI.h>
#include <ork/asset/AssetCategory.h>
#include <ork/file/path.h>
#include <ork/kernel/varmap.inl>

namespace ork { namespace asset {

class Asset;
class AssetSet;
class AssetLoader;
class FileAssetNamer;

class AssetClass : public object::ObjectClass {
  RttiDeclareExplicit(AssetClass, object::ObjectClass, rtti::NamePolicy, AssetCategory);
  static const VarMap novars();

  using assetset_ptr_t   = std::shared_ptr<AssetSet>;
  using asset_ptr_t      = std::shared_ptr<Asset>;
  using asset_constptr_t = std::shared_ptr<const Asset>;

public:
  AssetClass(const rtti::RTTIData&);

  AssetLoader* FindLoader(PieceString);
  asset_ptr_t LoadUnManagedAsset(PieceString, const VarMap& vmap = novars());
  asset_ptr_t DeclareAsset(PieceString, const VarMap& vmap = novars());
  asset_ptr_t FindAsset(PieceString, const VarMap& vmap = novars());

  void AddLoader(AssetLoader* loader);

  asset_ptr_t CreateUnmanagedAsset(PieceString, const VarMap& vmap = novars());

  assetset_ptr_t assetSet();

  void SetAssetNamer(const std::string& namer);
  void AddTypeAlias(ConstString alias);

  bool AutoLoad(int depth = -1);

  std::set<file::Path> EnumerateExisting() const;

private:
  FileAssetNamer* GetAssetNamer() const;
  FileAssetNamer* mAssetNamer;
  std::set<AssetLoader*> mLoaders;
  assetset_ptr_t _assetset;
};

}} // namespace ork::asset
