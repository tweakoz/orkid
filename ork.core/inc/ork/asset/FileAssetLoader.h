///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/asset/AssetLoader.h>
#include <ork/orkstl.h> // for orkvector
#include <ork/kernel/string/MutableString.h> // for orkvector

namespace ork {
class PieceString;
class MutableString;
};

namespace ork { namespace asset {

class Asset;

class FileAssetLoader : public AssetLoader
{
	orkvector<ConstString> mFileExtensions;
public:
	bool FindAsset(const PieceString &, MutableString result, int first_extension = 0);
	/*virtual*/ bool CheckAsset(const PieceString &);
	/*virtual*/ bool LoadAsset(Asset *asset);

	void AddFileExtension(ConstString extension);

protected:
	virtual bool LoadFileAsset(Asset *asset, ConstString filename) = 0;
};

} }
