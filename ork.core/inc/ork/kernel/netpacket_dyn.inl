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

struct DynamicMessagePacketIterator {
  DynamicMessagePacketIterator(const DynamicMessagePacket& msg)
      : mMessage(msg)
      , mireadIndex(0) {
  }

  template <typename T> T readItem();

  template <typename T>
  inline void _swapBytesInPlace(T& item) // inplace endian swap
  {
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

  template <typename T> T readItemSwapped();

  ///////////////////////////////////////////////////
  void clear() {
    mireadIndex = 0;
  }
  inline bool valid() const;
  int index() const {
    return mireadIndex;
  }
  void skip(int count) {
    mireadIndex += count;
  }
  ////////////////////////////////////////////////////
  // Variable: mMessage
  // Network Message this iterator is iterating inside
  const DynamicMessagePacket& mMessage;
  ////////////////////////////////////////////////////
  // Variable: mireadIndex
  // read Byte Index into the message
  int mireadIndex;
  ////////////////////////////////////////////////////
};

struct DynamicMessagePacket final : public MessagePacketBase {

  using iter_t = DynamicMessagePacketIterator;

  ///////////////////////////////////////////////////////

  DynamicMessagePacket() {
    clear();
  }

  ///////////////////////////////////////////////////////

  iter_t makeIterator() const {
    return iter_t(*this);
  }

  ///////////////////////////////////////////////////////

  template <typename T> void write(const T& inp) {
    static_assert(std::is_trivially_copyable<T>::value, "can only write is_trivially_copyable's into a NetworkMessage!");
    size_t ilen = sizeof(T);
    writeDataInternal((const void*)&inp, ilen);
  }
  void writeString(const char* pstr) {
    size_t ilen = strlen(pstr) + 1;
    writeDataInternal(&ilen, sizeof(ilen));
    writeDataInternal(pstr, ilen);
    // for( size_t i=0; i<ilen; i ++ )
    //  write( pstr[i] ); // slow method, oh well doesnt matter now...
  }
  void writeString(const std::string& str) {
    size_t ilen = str.length() + 1;
    writeDataInternal(&ilen, sizeof(ilen));
    writeDataInternal(str.c_str(), ilen);
    // for( size_t i=0; i<ilen; i ++ )
    //  write( pstr[i] ); // slow method, oh well doesnt matter now...
  }
  void writeData(const void* pdata, size_t ilen) {
    write(ilen);
    writeDataInternal(pdata, ilen);
  }
  void writeDataInternal(const void* pdata, size_t ilen) {
    const char* start = (const char*)pdata;
    const char* end = start + ilen;
    size_t prelen = length();
    _buffer.insert(_buffer.end(),start,end);
    size_t postlen = length();
    OrkAssert(postlen==(prelen+ilen));
  }

  ///////////////////////////////////////////////////////

  template <typename T> void read(T& outp, iter_t& it) const {
    static_assert(std::is_trivially_copyable<T>::value, "can only read is_trivially_copyable's from a NetworkMessage!");
    size_t ilen = sizeof(T);
    readDataInternal((void*)&outp, ilen, it);
  }
  std::string readString(iter_t& it) const {
    size_t str_length = 0;
    readDataInternal((void*)&str_length, sizeof(str_length), it);

    std::vector<char> strbuffer;
    strbuffer.resize(str_length);
    readDataInternal((void*)strbuffer.data(), str_length, it);
    return std::string(strbuffer.data());
  }
  void readData(void* pdest, size_t ilen, iter_t& it) const {
    size_t rrlen = 0;
    read(rrlen, it);
    OrkAssert(rrlen == ilen);
    readDataInternal(pdest, ilen, it);
  }
  void readDataInternal(void* pdest, size_t ilen, iter_t& it) const {
    OrkAssert((it.mireadIndex + ilen) <= length());
    const char* psrc = _buffer.data()+it.mireadIndex;
    memcpy((char*)pdest, psrc, ilen);
    it.mireadIndex += ilen;
  }

  ///////////////////////////////////////////////////////

  DynamicMessagePacket& operator=(const DynamicMessagePacket& rhs) {

    miSerial   = rhs.miSerial;
    miTimeSent = rhs.miTimeSent;
    _buffer = rhs._buffer;

    return *this;
  }

  ////////////////////////////////////////////////////

  void sendZmq(zmq_socket_ptr_t skt) {
    zmq::message_t zmqmsg_send(this->data(), this->length());
    skt->send(zmqmsg_send, zmq::send_flags::dontwait);
  }
  void recvZmq(zmq_socket_ptr_t skt) {
    zmq::message_t zmqmsg_recv;
    auto recv_status = skt->recv(zmqmsg_recv);
    this->clear();
    this->writeDataInternal(zmqmsg_recv.data(), zmqmsg_recv.size());
  }

  ////////////////////////////////////////////////////

  void dump(const char* label) {
    size_t icount = length();
    size_t j      = 0;
    while (icount > 0) {
      uint8_t* paddr = (uint8_t*)(_buffer.data() + j);
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
    return (const void*) _buffer.data();
  }
  void* data() final {
    return (void*) _buffer.data();
  }
  size_t length() const final {
    return _buffer.size();
  }
  void clear() final { 
    _buffer.clear();
  }
  size_t max() const final {
    return size_t(1)<<32;
  }

  ////////////////////////////////////////////////////

  std::vector<char> _buffer;

};

///////////////////////////////////////////////////////////////////////////////////////

template <typename T> T DynamicMessagePacketIterator::readItem() {
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
  }

} // namespace ork
