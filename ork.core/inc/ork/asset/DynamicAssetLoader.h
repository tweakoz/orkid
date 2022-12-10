////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <ork/asset/AssetLoader.h>

namespace ork::asset {

using set_t = std::set<file::Path>;

struct DynamicAssetLoader : public AssetLoader {

  using check_fn_t = std::function<bool(const AssetPath& name)>;
  using load_fn_t  = std::function<asset_ptr_t(loadrequest_ptr_t loadreq)>;
  using enum_fn_t  = std::function<set_t()>;

  DynamicAssetLoader();

  bool doesExist(const AssetPath&) override;
  bool resolvePath(
      const AssetPath& pathin, //
      AssetPath& resolved_path) override;
  asset_ptr_t load(loadrequest_ptr_t loadreq) override;
  void destroy(asset_ptr_t asset) override;
  set_t EnumerateExisting() override;

  check_fn_t _checkFn;
  load_fn_t _loadFn;
  enum_fn_t _enumFn;
};

} // namespace ork::asset
