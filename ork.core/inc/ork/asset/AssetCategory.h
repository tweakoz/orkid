///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/object/ObjectCategory.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/orkstl.h>

#include <ork/config/config.h>

namespace ork { namespace asset {

class Asset;
class AssetClass;

class  AssetCategory : public object::ObjectCategory
{
public:
	AssetCategory(const rtti::RTTIData &data);

	void AddTypeAlias(PoolString, AssetClass *);

	Asset *FindAsset(PieceString type, PieceString name) const;
	Asset *LoadUnManagedAsset(PieceString type, PieceString name) const;
	Asset *DeclareAsset(PieceString type, PieceString name) const;
	AssetClass *FindAssetClass(PieceString name) const;
private:
	/*virtual*/ bool SerializeReference(reflect::ISerializer &, const ICastable *) const;
	/*virtual*/ bool DeserializeReference(reflect::IDeserializer &, ICastable *&) const;

	// COMPAT
	orkmap<PoolString, AssetClass *> mTypeAliasMap;
};

} }

