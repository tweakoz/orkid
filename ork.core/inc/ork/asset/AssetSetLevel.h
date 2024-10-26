////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstl.h>

namespace ork { namespace asset {

struct AssetSetEntry;

struct AssetSetLevel
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
