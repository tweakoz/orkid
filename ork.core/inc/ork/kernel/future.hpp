////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/svariant.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/atomic.h>
#include <functional>

namespace ork {

struct Future {
  typedef svar160_t var_t;
  typedef int future_id_t;

  Future();
  bool isSignaled() const {
    return _state.load() > 0;
  }
  template <typename T> void signal(const T& result);
  void clear();
  void waitForSignal() const;
  void setId(future_id_t id) {
    _ID = id;
  }
  future_id_t getId() const {
    return _ID;
  }
  const var_t& getResult() const;
  ////////////////////

  typedef std::function<void(const Future& fut)> fut_blk_cb_t;

  ////////////////////

  future_id_t _ID;
  ork::atomic<int> _state;
  var_t _result;
  var_t _callback;
  std::string _name;
  // mutable std::condition_variable mWaitCV;
};

template <typename T> void Future::signal(const T& result) {
  _result.set<T>(result);

  if (_callback.isA<fut_blk_cb_t>()) {
    const fut_blk_cb_t& blk = _callback.get<fut_blk_cb_t>();
    blk(*this);
  }

  _state.fetch_add(1);
}

} // namespace ork
