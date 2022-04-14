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

  /////////////////////////////
  concurrent_triple_buffer() : _mutex("CTB"){

    mValues[0] = new T(0);
    mValues[1] = new T(1);
    mValues[2] = new T(2);

    _avail_for_write.insert(mValues[0]);
    _avail_for_write.insert(mValues[1]);
    _avail_for_write.insert(mValues[2]);
  }
  /////////////////////////////
  ~concurrent_triple_buffer() {
    disable();
    delete mValues[2];
    delete mValues[1];
    delete mValues[0];
  }
  /////////////////////////////
  // compute and return new write buffer
  /////////////////////////////
  T* begin_push(void) // get handle to a write buffer
  {
    T* rval = nullptr;
    while (nullptr==rval) {
      bool do_sleep = false;

      _mutex.Lock();
      auto it = _avail_for_write.begin();
      if(it!=_avail_for_write.end()){
        rval = *it;
        _avail_for_write.erase(it);
      }
      _mutex.UnLock();

      if(do_sleep) {
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
      if(_freshest){
        _avail_for_write.insert(_freshest);
      }
      _freshest = pret;
      _mutex.UnLock();
  }
  /////////////////////////////
  const T* begin_pull(void) const // get a read buffer
  {
      const T* rval = nullptr;
      while( nullptr == rval ){
        _mutex.Lock();
        rval = _freshest;
        _freshest = nullptr;
        _mutex.UnLock();
      }

      return rval;
  }
  /////////////////////////////
  // done reading
  /////////////////////////////
  void end_pull(const T* pret) const {
      bool done = false;
      _mutex.Lock();
      if(_freshest){
        _avail_for_write.insert(_freshest);
      }
      _freshest = (T*) pret;
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
    return mValues[index];
  }
  /////////////////////////////
private: //
  /////////////////////////////
  T* mValues[3];
  mutable std::unordered_set<T*> _avail_for_write;
  mutable ork::mutex _mutex;
  mutable T* _freshest = nullptr;
};
