///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/RingLink.h>

namespace ork {
class PieceString;
};

namespace ork { namespace asset {

class Asset;

class AssetLoader : public util::RingLink<AssetLoader>
{
public:
	virtual bool CheckAsset(const PieceString &) = 0;
	virtual bool LoadAsset(Asset *asset) = 0;
	virtual void DestroyAsset(Asset *asset) = 0;
};

} }

