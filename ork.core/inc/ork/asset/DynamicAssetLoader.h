///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <ork/asset/AssetLoader.h>

namespace ork::asset {

using set_t = std::set<file::Path>;

struct DynamicAssetLoader : public AssetLoader {

  using check_fn_t = std::function<bool(const AssetPath& name)>;
  using load_fn_t  = std::function<asset_ptr_t(const AssetPath& name, vars_constptr_t vars)>;
  using enum_fn_t  = std::function<set_t()>;

  DynamicAssetLoader();

  bool doesExist(const AssetPath&) override;
  bool resolvePath(
      const AssetPath& pathin, //
      AssetPath& resolved_path) override;
  asset_ptr_t load(const AssetPath&, vars_constptr_t vars) override;
  void destroy(asset_ptr_t asset) override;
  set_t EnumerateExisting() override;

  check_fn_t mCheckFn;
  load_fn_t mLoadFn;
  enum_fn_t mEnumFn;
};

} // namespace ork::asset
