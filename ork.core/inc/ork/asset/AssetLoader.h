////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>
#include <ork/file/path.h>
#include <set>

namespace ork::asset {

class AssetLoader {
public:
  virtual ~AssetLoader() {}
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
