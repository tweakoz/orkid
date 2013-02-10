///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/orkstl.h>

namespace ork { namespace asset {

class AssetSetEntry;

class AssetSetLevel
{
public:
	AssetSetLevel(AssetSetLevel *parent)
		: mParent(parent)
	{}

	typedef orkvector<AssetSetEntry *> SetType;

	SetType &GetSet() { return mSet; }

	AssetSetLevel *Parent() const { return mParent; }
private:
	AssetSetLevel *mParent;
	SetType mSet;
};

} }
