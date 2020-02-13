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
  static const VarMap _gnovars;
  AssetCategory(const rtti::RTTIData& data);

  void AddTypeAlias(PoolString, AssetClass*);

  Asset* FindAsset(PieceString type, PieceString name) const;
  Asset* LoadUnManagedAsset(PieceString type, PieceString name) const;
  Asset* DeclareAsset(PieceString type, PieceString name, const VarMap& vmap = _gnovars) const;
  AssetClass* FindAssetClass(PieceString name) const;

private:
  /*virtual*/ bool SerializeReference(reflect::ISerializer&, const ICastable*) const;
  /*virtual*/ bool DeserializeReference(reflect::IDeserializer&, ICastable*&) const;

  // COMPAT
  orkmap<PoolString, AssetClass*> mTypeAliasMap;
};

}} // namespace ork::asset
