///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once
#if !defined(WII)
//#define ORKCONFIG_ASSET_UNLOAD 
#endif
#include <ork/asset/AssetClass.h>
#include <ork/object/Object.h>
// COMPAT
//e#include <ork/kernel/core/object/object.h>
#include <ork/kernel/string/PoolString.h>

#include <ork/config/config.h>

namespace ork { namespace asset {

//typedef rtti::RTTI<Asset, Object, rtti::AbstractPolicy, AssetClass> AssetBase;
    
// COMPAT: inheritance from CObject
class Asset : public Object
{
    RttiDeclareAbstractWithCategory( Asset, Object, AssetClass );
    
public:
	void SetName(PoolString name);
	PoolString GetName() const;
	virtual PoolString GetType() const;
	bool Load() const;
	bool IsLoaded() const;
private:
	PoolString mName;
};

} }
