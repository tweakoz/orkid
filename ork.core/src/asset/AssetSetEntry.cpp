////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/asset/Asset.h>
#include <ork/asset/AssetSetEntry.h>
#include <ork/asset/AssetDependent.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace asset {
///////////////////////////////////////////////////////////////////////////////

AssetSetEntry::AssetSetEntry(Asset *asset, AssetLoader *loader, AssetSetLevel *level)
	: mAsset(asset)
	, mLoader(loader)
	, mDeclareLevel(level)
	, mLoadLevel(NULL)
{
}

///////////////////////////////////////////////////////////////////////////////

bool AssetSetEntry::Load(AssetSetLevel *level)
{
	if(NULL == mLoadLevel)
	{
			if(NULL == mLoader || false == mLoader->LoadAsset(mAsset))
		{
			mLoadLevel = NULL;
			
			return false;
		}

		mLoadLevel = level;

		mLoadProvider.Provide();
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

#if defined(ORKCONFIG_ASSET_UNLOAD)
bool AssetSetEntry::UnLoad(AssetSetLevel *level)
{
	if( IsLoaded() )
	{
		if( mLoader )
		{
			mLoader->DestroyAsset(mAsset);			
			mLoadProvider.Revoke();
			mLoadLevel = NULL;
			return true;
		}
	}

	return false;
}
#endif

///////////////////////////////////////////////////////////////////////////////

void AssetSetEntry::OnPush(AssetSetLevel *level)
{
	OrkAssert(mAsset);
}

///////////////////////////////////////////////////////////////////////////////

void AssetSetEntry::OnPop(AssetSetLevel *level)
{
	OrkAssert(mAsset);

	if(mLoadLevel == level)
	{
		mLoadProvider.Revoke();
		mLoader->DestroyAsset(mAsset);

		mLoadLevel = NULL;
	}

	if(mDeclareLevel == level)
	{
		OrkAssert(false == mLoadProvider.Providing());
		OrkAssert(NULL == mLoadLevel);

		this->~AssetSetEntry();
	}
}

///////////////////////////////////////////////////////////////////////////////

AssetSetEntry::~AssetSetEntry()
{
	if(mAsset)
	{
		delete mAsset;

		mAsset = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

Asset *AssetSetEntry::GetAsset() const
{
	return mAsset;
}

///////////////////////////////////////////////////////////////////////////////

AssetLoader *AssetSetEntry::GetLoader() const
{
	return mLoader;
}

///////////////////////////////////////////////////////////////////////////////

bool AssetSetEntry::IsLoaded()
{
	return 0 != mLoadLevel;
}

///////////////////////////////////////////////////////////////////////////////

util::dependency::Provider *AssetSetEntry::GetLoadProvider()
{
	return &mLoadProvider;
}

///////////////////////////////////////////////////////////////////////////////

AssetSetEntry *GetAssetSetEntry(const Asset *asset)
{
	AssetClass *asset_class = asset->GetClass();
	AssetSetEntry *entry = asset_class->GetAssetSet().FindAssetEntry(asset->GetName());

	return entry;
}

///////////////////////////////////////////////////////////////////////////////

} }
