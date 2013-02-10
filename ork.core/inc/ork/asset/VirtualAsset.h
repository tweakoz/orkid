///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/asset/Asset.h>

namespace ork { namespace asset {

class VirtualAsset : public Asset
{
    RttiDeclareConcrete( VirtualAsset, Asset );
    
public:
	VirtualAsset();

	void SetType(PoolString category);

	/*virtual*/ PoolString GetType() const;
private:
	PoolString mCategory;
};


} }
