////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
