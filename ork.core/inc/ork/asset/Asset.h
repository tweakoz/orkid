///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/string/PoolString.h>
#include <ork/config/config.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/file/path.h>

namespace ork::asset {

class AssetSet;
using assetset_ptr_t = std::shared_ptr<AssetSet>;

class Asset : public Object {
  DeclareConcreteX(Asset, ork::Object);

public:
  Asset();
  void setName(AssetPath name);
  AssetPath name() const;
  virtual std::string type() const;
  bool Load() const;
  bool LoadUnManaged() const;
  bool IsLoaded() const;
  assetset_ptr_t assetSet() const;

  varmap::VarMap _varmap;
  AssetPath _name;
};

using asset_ptr_t      = std::shared_ptr<Asset>;
using asset_constptr_t = std::shared_ptr<const Asset>;

} // namespace ork::asset

namespace ork {
template <>                                    //
struct use_custom_serdes<asset::asset_ptr_t> { //
  static constexpr bool enable = true;
};

} // namespace ork
