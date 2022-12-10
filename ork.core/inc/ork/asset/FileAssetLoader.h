////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/AssetLoader.h>
#include <ork/orkstl.h>                      // for orkvector
#include <ork/kernel/string/MutableString.h> // for orkvector
#include <ork/kernel/fixedstring.h>
#include <ork/file/filedevcontext.h>
#include <ork/object/Object.h>

namespace ork {
class PieceString;
class MutableString;
}; // namespace ork

namespace ork::asset {

typedef fxstring<8> file_ext_t;

struct FileSet {
  file_ext_t mExt;
  filedevctx_constptr_t mPathBase;
};

struct FileAssetLoader : public AssetLoader {
  FileAssetLoader(const object::ObjectClass* ac)
      : _assetClass(ac) {
  }
  bool _find(
      const AssetPath&, //
      AssetPath& result_out,
      int first_extension = 0);

  bool resolvePath(
      const AssetPath& pathin, //
      AssetPath& resolved_path) override;

  bool doesExist(const AssetPath&) override;
  asset_ptr_t load(loadrequest_ptr_t loadreq) override;
  void addLocation(
      filedevctx_constptr_t b, //
      file_ext_t e);

  virtual void initLoadersForUriProto(const std::string& uriproto) {}

protected:
  virtual asset_ptr_t _doLoadAsset(loadrequest_ptr_t loadreq) = 0;
  std::set<file::Path> EnumerateExisting() override;

  const object::ObjectClass* _assetClass;
  std::vector<FileSet> mLocations;
};

} // namespace ork::asset
