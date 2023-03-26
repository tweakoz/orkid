////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/atomic.h>
#include <zmq.hpp>
#include <zmq_addon.hpp>

namespace ork {

using zmq_socket_ptr_t = std::shared_ptr<::zmq::socket_t>;

struct MessagePacketBase;
using msgpacketbase_ptr_t = std::shared_ptr<MessagePacketBase>;
using msgpacketbase_ref_t = MessagePacketBase&;

template <size_t ksize> struct StaticMessagePacket;
typedef StaticMessagePacket<4096> StandardNetworkPacket;

///////////////////////////////////////////////////////////////////////////////

struct MessagePacketIteratorBase {

    MessagePacketIteratorBase(const MessagePacketBase& basepkt);

  template <typename T>
  inline void _swapBytesInPlace(T& item);
  template <typename T> T readItemSwapped();
  template <typename T> T readItem();

  void clear();
  void skip(int count);

  bool valid() const;
  int index() const;

  const MessagePacketBase& _basepacket;
  ////////////////////////////////////////////////////
  // Variable: mireadIndex
  // read Byte Index into the message
  int mireadIndex;
  ////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

//! data read marker/iterator for a NetworkMessage
template <size_t ksize>
struct MessagePacketIterator final : public MessagePacketIteratorBase { //
  MessagePacketIterator(const StaticMessagePacket<ksize>& msg)
      : MessagePacketIteratorBase(msg)
      , mMessage(msg){}

  const StaticMessagePacket<ksize>& mMessage;
};

typedef MessagePacketIterator<4096> StandardNetworkPacketIterator;

///////////////////////////////////////////////////////////////////////////////

//! message packet (an atomic network message)
//! statically sized, storage for the message is embedded.
//! This allow explicit allocation policies, such as embedding in shared memory

struct MessagePacketBase {

  using base_iter_t = MessagePacketIteratorBase;

  virtual ~MessagePacketBase();

  ////////////////////////////////////////////////////
  template <typename T> void read(T& outp, base_iter_t& it) const {
    static_assert(std::is_trivially_copyable<T>::value, "can only read is_trivially_copyable's from a NetworkMessage!");
    size_t ilen = sizeof(T);
    readDataInternal((void*)&outp, ilen, it);
  }
  template <typename T> void write(const T& inp) {
    static_assert(std::is_trivially_copyable<T>::value, "can only write is_trivially_copyable's into a NetworkMessage!");
    size_t ilen = sizeof(T);
    writeDataInternal((const void*)&inp, ilen);
  }
  ////////////////////////////////////////////////////
  void readData(void* pdest, size_t ilen, base_iter_t& it) const;
  std::string readString(base_iter_t& it) const;
  ////////////////////////////////////////////////////
  void writeData(const void* pdata, size_t ilen);
  void writeString(const char* pstr);
  void writeString(const std::string& str);
  ////////////////////////////////////////////////////
  void sendZmq(zmq_socket_ptr_t skt);
  void recvZmq(zmq_socket_ptr_t skt);
  ///////////////////////////////////////////////////////
  virtual void clear()             = 0;
  virtual const void* data() const = 0;
  virtual void* data()             = 0;
  virtual size_t max() const       = 0;
  virtual size_t length() const    = 0;
  virtual void writeDataInternal(const void* pdata, size_t ilen) = 0;
  virtual void readDataInternal(void* pdest, size_t ilen, base_iter_t& it) const = 0;
  ///////////////////////////////////////////////////////
  mutable uint64_t miSerial;
  mutable uint64_t miTimeSent;
};

///////////////////////////////////////////////////////////////////////////////

template <size_t ksize> struct StaticMessagePacket : public MessagePacketBase {

  static constexpr size_t kmaxsize = ksize;

  typedef MessagePacketIterator<ksize> iter_t;

  ///////////////////////////////////////////////////////
  StaticMessagePacket() {
    clear();
  }
  ///////////////////////////////////////////////////////
  iter_t makeIterator() const {
    return iter_t(*this);
  }
  ///////////////////////////////////////////////////////

  void writeDataInternal(const void* pdata, size_t ilen) final {
    assert((miSize + ilen) <= kmaxsize);
    const char* pch = (const char*)pdata;
    char* pdest     = &mBuffer[miSize];
    memcpy(pdest, (const char*)pdata, ilen);
    miSize += ilen;
  }
  void readDataInternal(void* pdest, size_t ilen, base_iter_t& it) const final {
    assert((it.mireadIndex + ilen) <= kmaxsize);
    const char* psrc = &mBuffer[it.mireadIndex];
    memcpy((char*)pdest, psrc, ilen);
    it.mireadIndex += ilen;
  }
  ///////////////////////////////////////////////////////
  StaticMessagePacket& operator=(const StaticMessagePacket& rhs) {
    miSize     = rhs.miSize;
    miSerial   = rhs.miSerial;
    miTimeSent = rhs.miTimeSent;

    if (miSize)
      memcpy(mBuffer, rhs.mBuffer, miSize);

    return *this;
  }
  ////////////////////////////////////////////////////
  void dump(const char* label) {
    size_t icount = miSize;
    size_t j      = 0;
    while (icount > 0) {
      uint8_t* paddr = (uint8_t*)(mBuffer + j);
      printf("msg<%p:%s> [%02lx : ", this, label, j);
      size_t thisc = (icount >= 16) ? 16 : icount;
      for (size_t i = 0; i < thisc; i++) {
        size_t idx = j + i;
        printf("%02x ", paddr[i]);
      }
      j += thisc;
      icount -= thisc;

      printf("\n");
    }
  }
  const void* data() const final {
    return (const void*)mBuffer;
  }
  void* data() final {
    return (void*)mBuffer;
  }
  size_t max() const final {
    return kmaxsize;
  }
  size_t length() const final {
    return miSize;
  }
  void clear() final {
    miSize = 0;
  }

  ////////////////////////////////////////////////////

  char mBuffer[kmaxsize];
  size_t miSize;

};

//! Message Stream Abstract interface
//! allows bidirectional serialization with half the code (serdes)

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct MessageStreamTraits;

///////////////////////////////////////////////////////////////////////////////

struct MessageStreamBase {
  template <typename T> MessageStreamBase& operator||(T& inp);

  virtual void serdesImpl(void* pdata, size_t len)         = 0;
  virtual void serdesStringImpl(std::string& str)          = 0;
  virtual void serdesFixedStringImpl(FixedStringBase& str) = 0;
  virtual bool isOutputStream() const                      = 0;

  StandardNetworkPacket mMessage;
};
///////////////////////////////////////////////////////////////////////////////
template <typename T> struct MessageStreamTraits {
  static void serdes(MessageStreamBase& stream_bas, T& data_ref) {
    static_assert(std::is_trivially_copyable<T>::value, "can only write is_trivially_copyable's into a NetworkMessage!");
    size_t ilen = sizeof(T);
    stream_bas.serdesImpl(&data_ref, ilen);
  }
};
///////////////////////////////////////////////////////////////////////////////
// TODO - figure out how to templatize on size of the fxstring!
template <> struct MessageStreamTraits<fxstring<64>> {
  static void serdes(MessageStreamBase& stream_bas, fxstring<64>& data_ref) {
    stream_bas.serdesFixedStringImpl(data_ref);
  }
};
///////////////////////////////////////////////////////////////////////////////
template <> struct MessageStreamTraits<std::string> {
  static void serdes(MessageStreamBase& stream_bas, std::string& data_ref) {
    stream_bas.serdesStringImpl(data_ref);
  }
};
///////////////////////////////////////////////////////////////////////////////

template <typename T> inline MessageStreamBase& MessageStreamBase::operator||(T& inp) {
  MessageStreamTraits<T>::serdes(*this, inp);
  // this->serdes(inp);
  return *this;
}

///////////////////////////////////////////////////////////////////////////////

//! Message Stream outgoing stream (write into NetworkMessage )
struct MessageOutStream : public MessageStreamBase // into netmessage
{
  void serdesFixedStringImpl(ork::FixedStringBase& str) override {
    mMessage.writeString(str.c_str());
  }
  void serdesStringImpl(std::string& str) override {
    mMessage.writeString(str.c_str());
  }
  void serdesImpl(void* pdata, size_t ilen) override {
    mMessage.writeDataInternal(pdata, ilen);
  }
  template <typename T> void writeItem(T& item) {
    item.serdes(*this);
  }
  bool isOutputStream() const override {
    return true;
  }
};

//! Message Stream outgoing stream (read from NetworkMessage )
struct MessageInpStream : public MessageStreamBase // outof netmessage
{
  MessageInpStream()
      : mIterator(mMessage) {
  }

  void serdesFixedStringImpl(ork::FixedStringBase& str) override {
    // todo make me more efficient!!!
    std::string inp = mMessage.readString(mIterator);
    str.set(inp.c_str());
  }
  void serdesStringImpl(std::string& str) override {
    str = mMessage.readString(mIterator);
  }
  void serdesImpl(void* pdata, size_t ilen) override {
    mMessage.readDataInternal(pdata, ilen, mIterator);
  }

  template <typename T> void readItem(T& item) {
    item.serdes(*this);
  }
  bool isOutputStream() const override {
    return false;
  }

  StandardNetworkPacketIterator mIterator;
};



  template <typename T>
  inline void MessagePacketIteratorBase::_swapBytesInPlace(T& item) { // inplace endian swap
    int isize = sizeof(T);

    T temp = item;

    auto src = reinterpret_cast<uint8_t*>(&item);
    auto tmp = reinterpret_cast<uint8_t*>(&temp);

    for (int i = 0, j = isize - 1; i < isize; i++, j--) {
      tmp[j] = src[i];
    }

    for (int i = 0; i < isize; i++) {
      src[i] = tmp[i];
    }
  }

  template <typename T> T MessagePacketIteratorBase::readItemSwapped() {
    T rval;
    _basepacket.read(rval, *this);
    _swapBytesInPlace<T>(rval);
    return rval;
  }

  template <typename T> T MessagePacketIteratorBase::readItem() {
    T rval;
    _basepacket.read(rval, *this);
    return rval;
  }


} // namespace ork
