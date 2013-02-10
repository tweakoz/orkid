///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/kernel/string/MutableString.h>

namespace ork { namespace asset {

class AssetNamer 
{
public:
	virtual bool Canonicalize(MutableString result, PieceString input) = 0;
};

} }

