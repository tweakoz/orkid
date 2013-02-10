///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/asset/AssetNamer.h>
#include <ork/kernel/string/PoolString.h>

namespace ork { namespace asset {

class FileAssetNamer : public AssetNamer
{
public:
	FileAssetNamer(ConstString prefix = "");
	/*virtual*/ bool Canonicalize(MutableString result, PieceString input);
private:
	ConstString mPrefix;
};

} }
