////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

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
