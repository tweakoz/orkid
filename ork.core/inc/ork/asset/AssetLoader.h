///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>
#include <ork/file/path.h>
#include <set>

namespace ork {
class PieceString;
};

namespace ork { namespace asset {

class AssetLoader {
public:
  virtual bool CheckAsset(const AssetPath&)    = 0;
  virtual bool LoadAsset(asset_ptr_t asset)    = 0;
  virtual void DestroyAsset(asset_ptr_t asset) = 0;

  virtual std::set<file::Path> EnumerateExisting() = 0;
};

}} // namespace ork::asset
