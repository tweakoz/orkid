///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <ork/asset/AssetLoader.h>

namespace ork { namespace asset {

struct DynamicAssetLoader : public AssetLoader {
  DynamicAssetLoader();

  bool CheckAsset(const PieceString&) override;
  bool LoadAsset(Asset* asset) override;
  void DestroyAsset(Asset* asset) override;
  std::set<file::Path> EnumerateExisting() override;

  typedef std::function<bool(const PieceString& name)> check_fn_t;
  typedef std::function<bool(Asset* passet)> load_fn_t;
  typedef std::function<std::set<file::Path>()> enum_fn_t;

  check_fn_t mCheckFn;
  load_fn_t mLoadFn;
  enum_fn_t mEnumFn;
};

}} // namespace ork::asset
