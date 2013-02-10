////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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
#include <ork/asset/AssetNamer.h>
#include <ork/asset/VirtualAsset.h>
#include <ork/kernel/string/ArrayString.h>

#include <ork/application/application.h> // For Add/FindPooledString

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::asset::AssetClass, "AssetClass")

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace asset {
///////////////////////////////////////////////////////////////////////////////

void AssetClass::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

AssetClass::AssetClass(const rtti::RTTIData &data)
	: object::ObjectClass(data)
	, mPlatformImplementation(this)
	, mGenericImplementation(this)
	, mAssetNamer(NULL)
{
	//CMemoryManager::Notification::Register();
}

///////////////////////////////////////////////////////////////////////////////

void AssetClass::OnPush()
{
	if(this == mPlatformImplementation)
		GetAssetSet().PushLevel(this);
}

///////////////////////////////////////////////////////////////////////////////

void AssetClass::OnPop()
{
	if(this == mPlatformImplementation)
		GetAssetSet().PopLevel();
}

///////////////////////////////////////////////////////////////////////////////

void AssetClass::OnPrintInfo()
{
	if(this != mGenericImplementation)
		return;

	orkprintf("Asset ------------ %s ------------\n", Name().c_str());

	int thelevel = 0;

	for(AssetSetLevel *level = GetAssetSet().GetTopLevel();
		level != NULL;
		level = level->Parent())
	{
		orkprintf("Level %d\n", thelevel++);
		const asset::AssetSetLevel::SetType &set = level->GetSet();

		for(asset::AssetSetLevel::SetType::const_iterator it = set.begin(); it != set.end(); it++)
		{
			orkprintf("+- %s\n", (*it)->GetAsset()->GetName().c_str());
		}
	}

	orkprintf("\n");
}

///////////////////////////////////////////////////////////////////////////////

void AssetClass::AddLoader(AssetLoader &loader)
{
	loader.LinkBefore(&mLoaders);
}

///////////////////////////////////////////////////////////////////////////////

class Renamer
{
	ArrayString<128> mRenamed;
public:
	Renamer(AssetNamer *renamer, PieceString &name);
};

///////////////////////////////////////////////////////////////////////////////

AssetNamer *AssetClass::GetAssetNamer() const
{
	if(mAssetNamer)
		return mAssetNamer;

	if(mGenericImplementation->mAssetNamer)
		return mGenericImplementation->mAssetNamer;

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////

AssetLoader *AssetClass::FindLoader(PieceString name)
{
	if(mPlatformImplementation != this)
		return mPlatformImplementation->FindLoader(name);

	Renamer fix_name(GetAssetNamer(), name);

	for(util::RingLink<AssetLoader>::iterator it = mLoaders.begin(); it != mLoaders.end(); ++it)
	{
		if(it->CheckAsset(name))
			return &*it;
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////

Asset *AssetClass::CreateUnmanagedAsset(PieceString name)
{
	if(mPlatformImplementation != this)
		return mPlatformImplementation->CreateUnmanagedAsset(name);

	Renamer fix_name(GetAssetNamer(), name);

	Asset *asset = NULL;

	if(false == Class::HasFactory())
	{
		VirtualAsset *vasset = new VirtualAsset();
		vasset->SetType(Name());

		asset = vasset;
	}
	else
	{
		asset = rtti::safe_downcast<Asset *>(CreateObject());
	}

	asset->SetName(ork::AddPooledString(name));

	return asset;
}

///////////////////////////////////////////////////////////////////////////////

Asset *AssetClass::FindAsset(PieceString name)
{
	if(mPlatformImplementation != this)
		return mPlatformImplementation->FindAsset(name);

	Renamer fix_name(GetAssetNamer(), name);

	PoolString name_string = FindPooledString(name);

	return mAssetSet.FindAsset(name_string);
}

///////////////////////////////////////////////////////////////////////////////

Asset *AssetClass::DeclareAsset(PieceString name)
{
	if(mPlatformImplementation != this)
		return mPlatformImplementation->DeclareAsset(name);

	// Editor support that allows nulling out a previously set asset name.
	if(name.empty())
	{
		return NULL;
	}

	if(Asset *asset = FindAsset(name))
	{
		mAssetSet.Register(asset->GetName());
		return asset;
	}

	Asset *new_asset = CreateUnmanagedAsset(name);

	mAssetSet.Register(new_asset->GetName(), new_asset, FindLoader(name));

	return new_asset;
}

///////////////////////////////////////////////////////////////////////////////

void AssetClass::SetPlatformImplementation(AssetClass *platform_implementation)
{
	OrkAssertI(mPlatformImplementation == this,
		"PlatformImplementation already set!");

	OrkAssertI(platform_implementation->mGenericImplementation == platform_implementation,
		"PlatformImplementation's GenericImplmentation already set!");

	mPlatformImplementation = platform_implementation;
	mPlatformImplementation->mGenericImplementation = this;
}

///////////////////////////////////////////////////////////////////////////////

AssetClass *AssetClass::GenericImplementation() const
{
	return mGenericImplementation;
}

///////////////////////////////////////////////////////////////////////////////

AssetSet &AssetClass::GetAssetSet()
{
	if(mPlatformImplementation != this)
		return mPlatformImplementation->GetAssetSet();

	return mAssetSet;
}

///////////////////////////////////////////////////////////////////////////////

bool AssetClass::AutoLoad(int depth)
{
	return GetAssetSet().Load(depth);
}

void AssetClass::SetAssetNamer(AssetNamer *namer)
{
	mAssetNamer = namer;
}

///////////////////////////////////////////////////////////////////////////////

void AssetClass::AddTypeAlias(ConstString alias)
{
	PoolString thealias = ork::AddPooledString(alias);
	GetClassStatic()->AddTypeAlias(thealias, this);
}

///////////////////////////////////////////////////////////////////////////////

Renamer::Renamer(AssetNamer *renamer, PieceString &name)
{
	if(renamer)
	{
		if(renamer->Canonicalize(mRenamed, name))
			name = mRenamed;
	}
}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////

