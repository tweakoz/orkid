///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/object/ObjectCategory.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/orkstl.h>

#include <ork/config/config.h>
#include <ork/kernel/varmap.inl>

namespace ork { namespace asset {

using namespace ork::varmap;

class Asset;
class AssetClass;

class AssetCategory : public object::ObjectCategory {
public:
  using asset_ptr_t      = std::shared_ptr<Asset>;
  using asset_constptr_t = std::shared_ptr<const Asset>;
  using vars_t           = VarMap;
  using vars_gen_t       = std::function<const vars_t&(Object*)>;
  static const VarMap novars();
  AssetCategory(const rtti::RTTIData& data);

  void AddTypeAlias(PoolString, AssetClass*);

  asset_ptr_t FindAsset(PieceString type, PieceString name) const;
  asset_ptr_t LoadUnManagedAsset(PieceString type, PieceString name) const;
  asset_ptr_t DeclareAsset(PieceString type, PieceString name, const VarMap& vmap = novars()) const;
  AssetClass* FindAssetClass(PieceString name) const;

private:
  /*virtual*/ bool serializeObject(reflect::ISerializer&, const ICastable*) const;
  /*virtual*/ bool deserializeObject(reflect::IDeserializer&, ICastable*&) const;

  // COMPAT
  orkmap<PoolString, AssetClass*> mTypeAliasMap;
};

}} // namespace ork::asset
