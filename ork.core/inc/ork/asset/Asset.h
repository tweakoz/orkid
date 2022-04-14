////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/string/PoolString.h>
#include <ork/config/config.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/file/path.h>

namespace ork::asset {

using vars_t          = varmap::VarMap;
using vars_ptr_t      = std::shared_ptr<vars_t>;
using vars_constptr_t = std::shared_ptr<const vars_t>;
using vars_gen_t      = std::function<vars_ptr_t(object_ptr_t)>;

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
  assetset_ptr_t assetset() const;

  vars_constptr_t _varmap;
  AssetPath _name;
};

} // namespace ork::asset

namespace ork {
template <>                                    //
struct use_custom_serdes<asset::asset_ptr_t> { //
  static constexpr bool enable = true;
};

} // namespace ork
