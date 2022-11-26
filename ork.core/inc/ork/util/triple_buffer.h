////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/atomic.h>
#include <ork/kernel/mutex.h>
#include <assert.h>
#include <unistd.h>
#include <unordered_set>

template <typename T> struct concurrent_triple_buffer {

  typedef T value_type;

  static constexpr int kquanta = 100;

  struct Item {
    std::shared_ptr<T> _payload;
    int _wrindex = -1;
  };

  /////////////////////////////
  concurrent_triple_buffer()
      : _mutex("CTB") {
    _values[0]._payload = std::make_shared<T>(0);
    _values[1]._payload = std::make_shared<T>(1);
    _values[2]._payload = std::make_shared<T>(2);
  }
  /////////////////////////////
  ~concurrent_triple_buffer() {
    disable();
  }
  /////////////////////////////
  // compute and return new write buffer
  /////////////////////////////
  T* begin_push(void) // get handle to a write buffer
  {
    T* rval = nullptr;
    while (nullptr == rval) {
      _mutex.Lock();
      while ((_write == _read) or (_write == _nextread)) {
        _write = (_write + 1) % 3;
      }
      rval = _values[_write]._payload.get();
      _mutex.UnLock();
      if (nullptr == rval) {
        usleep(kquanta);
      }
    }
    // printf("begin_push w<%d>\n", _write);
    return rval;
  }
  /////////////////////////////
  // publish write buffer
  /////////////////////////////
  void end_push(T* pret) {
    _mutex.Lock();
    _nextread                = _write;
    _values[_write]._wrindex = _wrindex++;
    // printf("end_push w<%d> mr<%d>\n", _write, _nextread);
    _mutex.UnLock();
  }
  /////////////////////////////
  const T* begin_pull(void) const // get a read buffer
  {
    const T* rval   = nullptr;
    size_t attempts = 0;
    while (nullptr == rval) {
      _mutex.Lock();
      if (_nextread >= 0) {
        _read = _nextread;
        rval  = _values[_read]._payload.get();
      }
      _mutex.UnLock();
      attempts++;
      if (attempts > 50) {
        //printf("triplebuf<%p> begin_pull _read<%d> _nextread<%d> attempts<%d> \n", this, _read, _nextread, int(attempts));
        return nullptr;
      }
    }
    //printf("triplebuf<%p> begin_pull _read<%d> _nextread<%d> attempts<%d> \n", this, _read, _nextread, int(attempts));
    return rval;
  }
  /////////////////////////////
  // done reading
  /////////////////////////////
  void end_pull(const T* pret) const {
    _mutex.Lock();
    // printf("end_pull r<%d>\n", _read);
    OrkAssert(pret == _values[_read]._payload.get());
    _mutex.UnLock();
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
    OrkAssert(index < 3);
    return _values[index]._payload.get();
  }
  /////////////////////////////
  void dump() const{
    printf( "triplebuf<%p> _wrindex<%d> _write<%d> _read<%d> _nextread<%d>\n", //
            this, //
            _wrindex, //
            _write, //
            _read, //
            _nextread );
  }
  /////////////////////////////
private: //
  /////////////////////////////
  Item _values[3];
  mutable ork::mutex _mutex;
  mutable int _nextread = -1;
  mutable int _read     = -1;
  mutable int _write    = -1;
  mutable int _wrindex  = -1;
};
