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

  struct Item{
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
      switch (_write) {
        case -1:
          _write = 0;
          break;
        case 0:
          rval = _values[0]._payload.get();
          break;
        case 1:
          rval = _values[1]._payload.get();
          break;
        case 2:
          rval = _values[2]._payload.get();
          break;
        default:
          OrkAssert(false);
          break;
      }
      _mutex.UnLock();
      if (nullptr == rval) {
        usleep(kquanta);
      }
    }
    return rval;
  }
  /////////////////////////////
  // publish write buffer
  /////////////////////////////
  void end_push(T* pret) {
    _mutex.Lock();

    _values[_write]._wrindex = _wrindex++;

    switch (_write) {
      case -1:
        OrkAssert(false);
        break;
      case 0:
        _write = (_values[1]._wrindex<_values[2]._wrindex) ? 1 : 2;
        _nextread  = 0;
        break;
      case 1:
        _write = (_values[0]._wrindex<_values[2]._wrindex) ? 0 : 2;
        _nextread  = 1;
        break;
      case 2:
        _write = (_values[0]._wrindex<_values[1]._wrindex) ? 0 : 1;
        _nextread  = 2;
        break;
      default:
        OrkAssert(false);
        break;
    }
    _mutex.UnLock();
  }
  /////////////////////////////
  const T* begin_pull(void) const // get a read buffer
  {
    const T* rval = nullptr;
    size_t attempts = 0;
    while (nullptr == rval) {
      _mutex.Lock();
      switch (_nextread) {
        case -1:
          usleep(kquanta);
          break;
        case 0:
          rval   = _values[0]._payload.get();
          _write = (_values[1]._wrindex<_values[2]._wrindex) ? 1 : 2;
          _read = 0;
          break;
        case 1:
          rval   = _values[1]._payload.get();
          _write = (_values[0]._wrindex<_values[2]._wrindex) ? 0 : 2;
          _read = 1;
          break;
        case 2:
          rval   = _values[2]._payload.get();
          _write = (_values[0]._wrindex<_values[1]._wrindex) ? 0 : 1;
          _read = 2;
          break;
        default:
          OrkAssert(false);
          break;
      }
      _mutex.UnLock();
      attempts++;
      if(attempts>50){
        return nullptr;
      }
    }
    return rval;
  }
  /////////////////////////////
  // done reading
  /////////////////////////////
  void end_pull(const T* pret) const {
    OrkAssert(pret==_values[_read]._payload.get());
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
private: //
  /////////////////////////////
  Item _values[3];
  mutable ork::mutex _mutex;
  mutable int _nextread          = -1;
  mutable int _read          = -1;
  mutable int _write = -1;
  mutable int _wrindex = -1;
};
