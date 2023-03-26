////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/atomic.h>
#include <ork/kernel/mutex.h>
#include <assert.h>
#include <unistd.h>
#include <unordered_set>

namespace ork{
template <typename T> struct concurrent_double_buffer {

  typedef T value_type;

  static constexpr int kquanta = 100;

  struct Item {
    std::shared_ptr<T> _payload;
    int _wrindex = -1;
  };

  /////////////////////////////
  concurrent_double_buffer()
      : _rdmutex("CDB-READ") {
    _values[0]._payload = std::make_shared<T>();
    _values[1]._payload = std::make_shared<T>();
    _wrindex.store(1);
    _rdindex.store(0);
  }
  /////////////////////////////
  ~concurrent_double_buffer() {
    disable();
  }
  /////////////////////////////
  // compute and return new write buffer
  /////////////////////////////
  T* begin_push(void) { // get handle to a write buffer
    int index_mod = _wrindex.load()&1;
    auto rval = _values[index_mod]._payload.get();
    return rval;
  }
  /////////////////////////////
  // publish write buffer
  /////////////////////////////
  void end_push(T* pret) {
  	_wrindex.fetch_add(1);

  	////////////////////////////////////////
  	// block producer waiting for reader to catch up
  	////////////////////////////////////////

    _rdmutex.Lock();
  	_rdindex.fetch_add(1);
    _rdmutex.UnLock();
  }
  /////////////////////////////
  const T* begin_pull(void) const { // get a read buffer

  	////////////////////////////////////////
  	// block producer waiting for reader to catch up
  	////////////////////////////////////////

    _rdmutex.Lock();

  	////////////////////////////////////////

    int index_mod = (_rdindex.load()&1);
    return _values[index_mod]._payload.get();
  }
  /////////////////////////////
  // done reading
  /////////////////////////////
  void end_pull(const T* pret) const {
    int index_mod = (_rdindex.load()&1);
    OrkAssert(pret == _values[index_mod]._payload.get());
    ///////////////////
  	// unblock producer
    ///////////////////
    _rdmutex.UnLock();
  }
  /////////////////////////////
  void disable() {
  }
  /////////////////////////////
  void enable() {
  }
  /////////////////////////////
  T* rawAccess(int index) {
    OrkAssert(index >= 0);
    OrkAssert(index <= 1);
    return _values[index]._payload.get();
  }
  /////////////////////////////
  void dump() const{
    printf( "doublebuf<%p> _wrindex<%d> _rdindex<%d>\n", //
            this, //
            _wrindex.load(), //
            _rdindex.load() );
  }
  /////////////////////////////
private: //
  /////////////////////////////
  Item _values[2];
  mutable ork::mutex _rdmutex;
  std::atomic<int> _wrindex;
  std::atomic<int> _rdindex;
};

}