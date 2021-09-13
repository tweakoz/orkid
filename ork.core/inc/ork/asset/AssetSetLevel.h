///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/orkstl.h>

namespace ork { namespace asset {

class AssetSetEntry;

class AssetSetLevel
{
public:
	AssetSetLevel(AssetSetLevel *parent)
		: _parent(parent)
	{}

	typedef orkvector<AssetSetEntry *> SetType;

	SetType &Getset() { return mSet; }

	AssetSetLevel *Parent() const { return _parent; }
private:
	AssetSetLevel *_parent;
	SetType mSet;
};

} }
