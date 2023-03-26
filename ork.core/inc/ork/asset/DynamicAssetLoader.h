////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
