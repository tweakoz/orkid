////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/netpacket.inl>

namespace ork {

struct DynamicMessagePacket;

///////////////////////////////////////////////////////////////////////////////

struct DynamicMessagePacketIterator final : public MessagePacketIteratorBase {

  DynamicMessagePacketIterator(const DynamicMessagePacket& msg);

  const DynamicMessagePacket& mMessage;
};

///////////////////////////////////////////////////////////////////////////////

struct DynamicMessagePacket final : public MessagePacketBase {

  using iter_t = DynamicMessagePacketIterator;

  ///////////////////////////////////////////////////////

  DynamicMessagePacket();
  iter_t makeIterator() const;
  void writeDataInternal(const void* pdata, size_t ilen) final;
  void readDataInternal(void* pdest, size_t ilen, base_iter_t& it) const final;
  DynamicMessagePacket& operator=(const DynamicMessagePacket& rhs);
  void dump(const char* label);
  const void* data() const final;
  void* data() final;
  size_t length() const final;
  void clear() final;
  size_t max() const final;

  ////////////////////////////////////////////////////

  std::vector<char> _buffer;

};

///////////////////////////////////////////////////////////////////////////////////////

/*template <typename T> T DynamicMessagePacketIterator::readItem() {
  T rval;
  mMessage.read(rval, *this);
  return rval;
}

  template <typename T> T DynamicMessagePacketIterator::readItemSwapped() {
    T rval;
    mMessage.read(rval, *this);
    _swapBytesInPlace<T>(rval);
    return rval;
  }

  inline bool DynamicMessagePacketIterator::valid() const {
    return mireadIndex < mMessage.length();
  }*/

} // namespace ork
