////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/asset/AssetDependent.h>

#include <ork/asset/Asset.h>
#include <ork/asset/AssetSetEntry.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace asset {
///////////////////////////////////////////////////////////////////////////////

util::dependency::Provider *assetLoadProvider(const Asset *asset)
{
	if(asset)
	{
		AssetSetEntry *entry = assetSetEntry(asset);
		return entry->GetLoadProvider();
	}
	else
	{
		return NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
