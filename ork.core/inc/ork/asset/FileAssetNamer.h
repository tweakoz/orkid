///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/string/PieceString.h>
#include <ork/kernel/string/ArrayString.h>

namespace ork { namespace asset {

class FileAssetNamer 
{
public:
	FileAssetNamer(ConstString prefix = "");
	bool Canonicalize(MutableString result, PieceString input);
private:
	ConstString mPrefix;
};

} }
