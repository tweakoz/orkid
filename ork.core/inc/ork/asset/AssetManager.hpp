///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/pch.h>

#include <ork/asset/AssetLoader.h>
#include <ork/util/RingLink.hpp>

namespace ork { namespace asset {

template<typename AssetType> ork::recursive_mutex AssetManager<AssetType>::gLock( "AssetManagerMutex");

template<typename AssetType> bool AssetManager<AssetType>::gbAUTOLOAD = true;


template<typename AssetType>
inline
AssetType *AssetManager<AssetType>::Create(const PieceString& asset_name)
{
	gLock.Lock();
	AssetType* prval = rtti::safe_downcast<AssetType *>(AssetType::GetClassStatic()->DeclareAsset(asset_name));
	gLock.UnLock();
	return prval;
}

template<typename AssetType>
inline
AssetType *AssetManager<AssetType>::Find(const PieceString& asset_name)
{
	gLock.Lock();
	AssetType* prval =  rtti::safe_downcast<AssetType *>(AssetType::GetClassStatic()->FindAsset(asset_name));
	gLock.UnLock();
	return prval;
}

template<typename AssetType>
inline
file::Path AssetManager<AssetType>::FindOnDisk(const PieceString& asset_name)
{
	file::Path rval;
	gLock.Lock();
	AssetClass* pclazz = AssetType::GetClassStatic();
	AssetLoader* loader = pclazz->FindLoader(asset_name);
	if( loader )
	{
	}
	gLock.UnLock();
	return rval;
}

template<typename AssetType>
inline
AssetType *AssetManager<AssetType>::Load(const PieceString& asset_name)
{
	gLock.Lock();

	AssetType *asset = Create(asset_name);
	
	if(asset)
	{
		if( false == asset->IsLoaded() )
		{
			asset->Load();
		}
		gLock.UnLock();
		return asset;
	}
	gLock.UnLock();
	return NULL;
}

template<typename AssetType>
inline
bool AssetManager<AssetType>::AutoLoad(int depth)
{
	if( gbAUTOLOAD )
	{
		gLock.Lock();
		bool brval = AssetType::GetClassStatic()->GetAssetSet().Load(depth);
		gLock.UnLock();
		return brval;
	}
	else
	{
		return false;
	}
}

#if defined(ORKCONFIG_ASSET_UNLOAD)
template<typename AssetType>
inline
bool AssetManager<AssetType>::AutoUnLoad(int depth)
{
	return AssetType::GetClassStatic()->GetAssetSet().UnLoad(depth);
}
#endif


} }

template class ork::util::RingLink<ork::asset::AssetLoader>;