///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/object/ObjectClass.h>
#include <ork/rtti/RTTI.h>
#include <ork/asset/AssetCategory.h>
#include <ork/asset/AssetSet.h>
#include <ork/util/RingLink.h>

namespace ork { namespace asset {

class Asset;
class AssetLoader;
class AssetNamer;

class AssetClass 
	: public object::ObjectClass
{
	RttiDeclareExplicit(AssetClass, object::ObjectClass, rtti::NamePolicy, AssetCategory);

public:
	AssetClass(const rtti::RTTIData &);

	AssetLoader *FindLoader(PieceString);
	Asset *DeclareAsset(PieceString);
	Asset *FindAsset(PieceString);

	void SetPlatformImplementation(AssetClass *platform_implementation);
	AssetClass *GenericImplementation() const;
	void AddLoader(AssetLoader &loader);

	Asset *CreateUnmanagedAsset(PieceString);

	AssetSet &GetAssetSet();

	void SetAssetNamer(AssetNamer *namer);
	void AddTypeAlias(ConstString alias);
	
	bool AutoLoad(int depth = -1);

private:
	AssetNamer *GetAssetNamer() const;
	AssetNamer *mAssetNamer;
	AssetClass *mPlatformImplementation;
	AssetClass *mGenericImplementation;
	util::RingLink<AssetLoader> mLoaders;
	AssetSet mAssetSet;

    // CMemoryManager::Notification implementation
	/*virtual*/ void OnPush();
	/*virtual*/ void OnPop();
	/*virtual*/ void OnPrintInfo();
};

} }
