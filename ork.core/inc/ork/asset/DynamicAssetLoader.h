///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <ork/asset/AssetLoader.h>

namespace ork { namespace asset {

struct DynamicAssetLoader : public AssetLoader {

  using check_fn_t = std::function<bool(const AssetPath& name)>;
  using load_fn_t  = std::function<bool(asset_ptr_t passet)>;
  using enum_fn_t  = std::function<std::set<file::Path>()>;

  DynamicAssetLoader();

  bool CheckAsset(const AssetPath&) override;
  bool LoadAsset(asset_ptr_t asset) override;
  void DestroyAsset(asset_ptr_t asset) override;
  std::set<file::Path> EnumerateExisting() override;

  check_fn_t mCheckFn;
  load_fn_t mLoadFn;
  enum_fn_t mEnumFn;
};

}} // namespace ork::asset
