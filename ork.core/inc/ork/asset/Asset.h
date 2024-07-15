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
struct AssetLoader;

using vars_t          = varmap::VarMap;
using vars_ptr_t      = std::shared_ptr<vars_t>;
using vars_gen_t      = std::function<vars_ptr_t(object_ptr_t)>;
using loadrequest_ptr_t = std::shared_ptr<LoadRequest>;
using assetloader_ptr_t = std::shared_ptr<AssetLoader>;

struct LoadSynchronizer {
  std::atomic<int> _load_counter = 0;
  void increment();
  void decrement();
  bool isComplete() const;
};
using loadsynchro_ptr_t = std::shared_ptr<LoadSynchronizer>;

struct LoadRequest{

  using event_lambda_t = std::function<void(uint32_t,varmap::var_t)>;

  LoadRequest();
  LoadRequest(const AssetPath& p);
  LoadRequest(const AssetPath& p, vars_ptr_t _asset_vars);

  void incrementPartialLoadCount();
  void decrementPartialLoadCount();
  void waitForCompletion() const;
  void enqueueAsync(void_lambda_t on_complete) const;

  template <typename T>
  std::shared_ptr<T> assetAs() const {
    return std::dynamic_pointer_cast<T>(_asset);
  }

  asset_ptr_t _asset;
  AssetPath _asset_path;
  vars_ptr_t _asset_vars;
  void_lambda_t _on_load_complete;
  event_lambda_t _on_event;

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
