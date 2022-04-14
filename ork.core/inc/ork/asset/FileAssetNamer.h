////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

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
