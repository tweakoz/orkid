
#pragma once

#include <ork/kernel/atomic.h>
#include <ork/kernel/concurrent_queue.h>
#include <assert.h>
#include <unistd.h>

template <typename T, const int N> class concurrent_multi_buffer {
public:
  static const int knumitems = N;
  static const int kquanta   = 1;

  typedef T value_type;
  /////////////////////////////
public:
  /////////////////////////////
  concurrent_multi_buffer() {
    mWritesOut = 0;
    mReadsOut  = 0;

    for (int i = 0; i < knumitems; i++) {
      mWritesOut++;
      mWriteItems.push(new T(0));
    }
  }
  /////////////////////////////
  ~concurrent_multi_buffer() {
    disable();

    // delete mValues[2];
    // delete mValues[1];
    // delete mValues[0];
  }
  /////////////////////////////
  // compute and return new write buffer
  /////////////////////////////
  T* BeginWrite(void) // get handle to a write buffer
  {
    T* rval = nullptr;
    int iw  = mWritesOut.fetch_add(-1);
    // OrkAssert(iw>0);
    // printf( "TRIPLEBUF BEGIN_PUSH iw<%d>\n", iw );
    mWriteItems.pop(rval, kquanta);
    return rval;
  }
  /////////////////////////////
  // publish write buffer
  /////////////////////////////
  void EndWrite(T* pret) {
    int ir = mReadsOut.fetch_add(1);
    // OrkAssert(ir<knumitems);
    // printf( "TRIPLEBUF END_PUSH ir<%d>\n", ir );
    mReadItems.push(pret, kquanta);
  }
  /////////////////////////////
  const T* BeginRead(void) const // get a read buffer
  {
    T* rval = nullptr;
    int ir  = mReadsOut.fetch_add(-1);
    mReadItems.try_pop(rval);
    return (const T*)rval;
  }
  /////////////////////////////
  // done reading
  /////////////////////////////
  void EndRead(const T* pret) const {
    int iw = mWritesOut.fetch_add(1);
    // OrkAssert(iw<knumitems);
    // printf( "TRIPLEBUF END_PULL iw<%d>\n", iw );
    mWriteItems.push((T*)pret, kquanta);
  }
  /////////////////////////////
  void disable() {
    // printf( "MULTIBUF DISABLE\n" );
    int icount = 0;
    while (icount < knumitems) {
      T* item = nullptr;

      if (mReadItems.try_pop(item)) {
        int ir = mReadsOut.fetch_add(-1);
        mDisabledItems.push(item);
        icount++;
      } else if (mWriteItems.try_pop(item)) {
        int iw = mWritesOut.fetch_add(-1);
        mDisabledItems.push(item);
        icount++;
      }
    }
  }
  /////////////////////////////
  void enable() {
    // printf( "MULTIBUF ENABLE\n" );
    T* item    = nullptr;
    int icount = 0;
    while (icount < knumitems) {
      T* item = nullptr;

      if (mDisabledItems.try_pop(item)) {
        int iw = mWritesOut.fetch_add(1);
        // printf( "MULTIBUF ENABLE(a) iw<%d>\n", iw );
        mWriteItems.push(item);
        icount++;
      } else if (mWriteItems.try_pop(item)) {
        int iw = mWritesOut.fetch_add(1);
        // printf( "MULTIBUF ENABLE(b) iw<%d>\n", iw );
        mWriteItems.push(item);
        icount++;
      }
    }
  }
  /////////////////////////////
private: //
  /////////////////////////////
  mutable ork::MpMcBoundedQueue<T*, knumitems> mReadItems;
  mutable ork::MpMcBoundedQueue<T*, knumitems> mWriteItems;
  mutable ork::MpMcBoundedQueue<T*, knumitems> mDisabledItems;
  mutable ork::atomic<int> mWritesOut;
  mutable ork::atomic<int> mReadsOut;
};
