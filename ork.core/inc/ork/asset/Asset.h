////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/string/PoolString.h>
#include <ork/config/config.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/file/path.h>

namespace ork::asset {

struct LoadRequest;

using vars_t          = varmap::VarMap;
using vars_gen_t      = std::function<vars_t(object_ptr_t)>;
using loadrequest_ptr_t = std::shared_ptr<LoadRequest>;

struct LoadRequest{

  LoadRequest();
  LoadRequest(const AssetPath& p);
  LoadRequest(const AssetPath& p,const vars_t& _asset_vars);

  void incrementPartialLoadCount();
  void decrementPartialLoadCount();
  void waitForCompletion() const;

  asset_ptr_t _asset;
  AssetPath _asset_path;
  vars_t _asset_vars;
  void_lambda_t _on_load_complete;
  std::atomic<int> _partial_load_counter = 0;
};

struct Asset : public Object {
  DeclareConcreteX(Asset, ork::Object);

public:
  Asset();
  void setRequest(loadrequest_ptr_t req);
  AssetPath name() const;
  virtual std::string type() const;
  bool Load() const;
  bool LoadUnManaged() const;
  bool IsLoaded() const;
  assetset_ptr_t assetset() const;

  vars_t _varmap;
  AssetPath _name;
  loadrequest_ptr_t _load_request;
};

} // namespace ork::asset

namespace ork {
template <>                                    //
struct use_custom_serdes<asset::asset_ptr_t> { //
  static constexpr bool enable = true;
};

} // namespace ork
