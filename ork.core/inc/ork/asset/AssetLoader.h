///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>
#include <ork/file/path.h>
#include <set>

namespace ork::asset {

class AssetLoader {
public:
  virtual bool doesExist(const AssetPath&) = 0;
  virtual bool resolvePath(
      const AssetPath& pathin, //
      AssetPath& resolved_path)                                    = 0;
  virtual asset_ptr_t load(const AssetPath&, vars_constptr_t vmap) = 0;
  virtual void destroy(asset_ptr_t asset)                          = 0;

  virtual std::set<file::Path> EnumerateExisting() = 0;
};

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::asset
