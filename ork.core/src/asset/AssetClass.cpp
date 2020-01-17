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

void AssetClass::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

AssetClass::AssetClass(const rtti::RTTIData &data)
	: object::ObjectClass(data)
	, mAssetNamer(NULL)
{
}

std::set<file::Path> AssetClass::EnumerateExisting() const {
  std::set<file::Path> rval;

  for (auto& l : mLoaders) {
    auto s = l->EnumerateExisting();
    for (auto i : s) {
       //printf( "enumexist loader<%p> asset<%s>\n", l, i.c_str() );
      rval.insert(i);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void AssetClass::AddLoader(AssetLoader* loader)
{
	mLoaders.insert(loader);
}

///////////////////////////////////////////////////////////////////////////////

class Renamer
{
	ArrayString<128> mRenamed;
public:
	Renamer(FileAssetNamer* renamer, PieceString &name);
};

///////////////////////////////////////////////////////////////////////////////

FileAssetNamer *AssetClass::GetAssetNamer() const
{
	return mAssetNamer;
}

///////////////////////////////////////////////////////////////////////////////

AssetLoader *AssetClass::FindLoader(PieceString name)
{
	Renamer fix_name(GetAssetNamer(), name);

	for( auto it : mLoaders )
	{
		if(it->CheckAsset(name))
			return it;
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////

Asset *AssetClass::CreateUnmanagedAsset(PieceString name)
{
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
	Renamer fix_name(GetAssetNamer(), name);

	PoolString name_string = FindPooledString(name);

	return mAssetSet.FindAsset(name_string);
}

///////////////////////////////////////////////////////////////////////////////

Asset *AssetClass::DeclareAsset(PieceString name)
{
	// Editor support that allows nulling out a previously set asset name.
	if(name.empty())
	{
		return nullptr;
	}

	if(Asset *asset = FindAsset(name))
	{
		mAssetSet.Register(asset->GetName());
		return asset;
	}

	Asset *new_asset = CreateUnmanagedAsset(name);
	auto loader = FindLoader(name);
	mAssetSet.Register(new_asset->GetName(), new_asset, loader );

	return new_asset;
}

///////////////////////////////////////////////////////////////////////////////

Asset *AssetClass::LoadUnManagedAsset(PieceString name)
{
	// Editor support that allows nulling out a previously set asset name.
	if(name.empty())
	{
		return nullptr;
	}

	Asset *new_asset = CreateUnmanagedAsset(name);

	auto loader = FindLoader(name);

	loader->LoadAsset( new_asset );
	return new_asset;
}

///////////////////////////////////////////////////////////////////////////////

AssetSet &AssetClass::GetAssetSet()
{
	return mAssetSet;
}

///////////////////////////////////////////////////////////////////////////////

bool AssetClass::AutoLoad(int depth)
{
	return GetAssetSet().Load(depth);
}

void AssetClass::SetAssetNamer(const std::string& namer)
{
	mAssetNamer = new FileAssetNamer(namer.c_str());
}

///////////////////////////////////////////////////////////////////////////////

void AssetClass::AddTypeAlias(ConstString alias)
{
	PoolString thealias = ork::AddPooledString(alias);
	GetClassStatic()->AddTypeAlias(thealias, this);
}

///////////////////////////////////////////////////////////////////////////////

Renamer::Renamer(FileAssetNamer* renamer, PieceString &name)
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
