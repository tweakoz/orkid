///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/path.h>
#include <set>

namespace ork {
class PieceString;
};

namespace ork { namespace asset {

class Asset;

class AssetLoader 
{
public:
	virtual bool CheckAsset(const PieceString &) = 0;
	virtual bool LoadAsset(Asset *asset) = 0;
	virtual void DestroyAsset(Asset *asset) = 0;

	virtual std::set<file::Path> EnumerateExisting() = 0;
};

} }

