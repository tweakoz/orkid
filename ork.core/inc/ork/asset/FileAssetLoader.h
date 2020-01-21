///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/asset/AssetLoader.h>
#include <ork/orkstl.h>                      // for orkvector
#include <ork/kernel/string/MutableString.h> // for orkvector
#include <ork/kernel/fixedstring.h>

namespace ork {
class PieceString;
class MutableString;
}; // namespace ork

namespace ork { namespace asset {

class Asset;
class AssetClass;

typedef fxstring<8> file_ext_t;
typedef fxstring<32> file_loc_t;
typedef file::Path file_pathbase_t;

struct FileSet {
  file_ext_t mExt;
  file_loc_t mLoc;
  file_pathbase_t mPathBase;
};

struct FileAssetLoader : public AssetLoader {
  FileAssetLoader(const AssetClass* ac)
      : mAssetClass(ac) {
  }
  bool FindAsset(const PieceString&, MutableString result, int first_extension = 0);
  bool CheckAsset(const PieceString&) override;
  bool LoadAsset(Asset* asset) override;
  void AddLocation(file_pathbase_t b, file_ext_t e);

protected:
  virtual bool LoadFileAsset(Asset* asset, ConstString filename) = 0;
  std::set<file::Path> EnumerateExisting() override;

  const AssetClass* mAssetClass;
  std::vector<FileSet> mLocations;
};

}} // namespace ork::asset
