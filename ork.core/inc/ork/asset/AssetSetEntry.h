///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/asset/AssetSet.h>
#include <ork/asset/AssetLoader.h>
#include <ork/util/dependency/Provider.h>

namespace ork { namespace asset {

class Asset;
class AssetLoader;
class AssetDependent;
class AssetSetLevel;

class AssetSetEntry
{
public:
	AssetSetEntry(Asset *asset, AssetLoader *loader, AssetSetLevel *level);
	~AssetSetEntry();

#if defined(ORKCONFIG_ASSET_UNLOAD)
	bool UnLoad(AssetSetLevel *level);
#endif
	bool Load(AssetSetLevel *level);
	void OnPush(AssetSetLevel *level);
	void OnPop(AssetSetLevel *level);
	Asset *GetAsset() const;
	AssetLoader *GetLoader() const;
	bool IsLoaded();
	util::dependency::Provider *GetLoadProvider();

private:
	Asset *mAsset;
	AssetLoader *mLoader;
	util::dependency::Provider mLoadProvider;
	AssetSetLevel *mDeclareLevel;
	AssetSetLevel *mLoadLevel;
};

AssetSetEntry *GetAssetSetEntry(const Asset *asset);

} }

