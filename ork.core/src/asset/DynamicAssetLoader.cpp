////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/asset/DynamicAssetLoader.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/Array.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/string/string.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace asset {
///////////////////////////////////////////////////////////////////////////////

DynamicAssetLoader::DynamicAssetLoader() : mCheckFn(nullptr), mLoadFn(nullptr), mEnumFn(nullptr) {}

std::set<file::Path> DynamicAssetLoader::EnumerateExisting() {
  std::set<file::Path> rval;
  if (mEnumFn)
    rval = mEnumFn();
  return rval;
}

/*void DynamicAssetLoader::AddLocation( file_pathbase_t b, file_ext_t e)
{
        file::Path p(b.c_str());

        FileSet fset;
        fset.mExt = e;
        fset.mLoc = p.HasUrlBase() ? p.GetUrlBase() : "";
        fset.mPathBase = b;
        mLocations.push_back(fset);

        printf( "FileAssetLoader added set ext<%s> loc<%s> base<%s>\n",
                        fset.mExt.c_str(),
                        fset.mLoc.c_str(),
                        fset.mPathBase.c_str() );
}*/

///////////////////////////////////////////////////////////////////////////////
/*
bool DynamicAssetLoader::FindAsset(const PieceString &name, MutableString
result, int first_extension)
{
        return false;
}*/

///////////////////////////////////////////////////////////////////////////////

bool DynamicAssetLoader::CheckAsset(const PieceString& name) { return (mCheckFn != nullptr) ? mCheckFn(name) : false; }

///////////////////////////////////////////////////////////////////////////////

bool DynamicAssetLoader::LoadAsset(Asset* asset) { return (mLoadFn != nullptr) ? mLoadFn(asset) : false; }

void DynamicAssetLoader::DestroyAsset(Asset* asset) {}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::asset
///////////////////////////////////////////////////////////////////////////////
