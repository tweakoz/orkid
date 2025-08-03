////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/svariant.h>
#include <cstring>
#include <type_traits>

namespace ork {

template<size_t KSIZE>
struct IpcMessagePacket;

///////////////////////////////////////////////////////////////////////////////
// Iterator for IpcMessagePacket
///////////////////////////////////////////////////////////////////////////////

template<size_t KSIZE>
struct IpcMessagePacketIterator {
  using packet_t = IpcMessagePacket<KSIZE>;
  
  const packet_t* _packet;
  size_t _index;
  
  IpcMessagePacketIterator(const packet_t& packet)
      : _packet(&packet)
      , _index(0) {}
      
  size_t index() const { return _index; }
  void setIndex(size_t idx) { _index = idx; }
};

///////////////////////////////////////////////////////////////////////////////
// POD-safe message packet for IPC
// No virtual functions, no dynamic allocations
///////////////////////////////////////////////////////////////////////////////

template<size_t KSIZE>
struct IpcMessagePacket {
  static constexpr size_t kMaxSize = KSIZE;
  
  // POD members only
  uint8_t _data[KSIZE];
  size_t _writeIndex;
  size_t _readIndex;
  
  // Default constructor - POD compatible
  IpcMessagePacket() {
    clear();
  }
  
  // Clear the packet
  void clear() {
    _writeIndex = 0;
    _readIndex = 0;
    std::memset(_data, 0, KSIZE);
  }
  
  // Get current write position
  size_t length() const {
    return _writeIndex;
  }
  
  // Get remaining space
  size_t remaining() const {
    return KSIZE - _writeIndex;
  }
  
  // Write string (length-prefixed)
  void writeString(const std::string& str) {
    size_t len = str.length();
    write<size_t>(len);
    if(len > 0) {
      writeData(str.c_str(), len);
    }
  }
  
  // Read string (length-prefixed)
  std::string readString() {
    size_t len = read<size_t>();
    if(len > 0 && (_readIndex + len) <= _writeIndex) {
      std::string result(reinterpret_cast<const char*>(_data + _readIndex), len);
      _readIndex += len;
      return result;
    }
    return "";
  }
  
  // Write POD type
  template<typename T>
  typename std::enable_if<std::is_trivially_copyable<T>::value>::type
  write(const T& value) {
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    if(_writeIndex + sizeof(T) <= KSIZE) {
      std::memcpy(_data + _writeIndex, &value, sizeof(T));
      _writeIndex += sizeof(T);
    }
  }
  
  // Read POD type
  template<typename T>
  typename std::enable_if<std::is_trivially_copyable<T>::value, T>::type
  read() {
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    T value{};
    if(_readIndex + sizeof(T) <= _writeIndex) {
      std::memcpy(&value, _data + _readIndex, sizeof(T));
      _readIndex += sizeof(T);
    }
    return value;
  }
  
  // Read POD type with output parameter (for compatibility)
  template<typename T>
  typename std::enable_if<std::is_trivially_copyable<T>::value>::type
  read(T& value) {
    value = read<T>();
  }
  
  // Write raw data
  void writeData(const void* data, size_t size) {
    if(_writeIndex + size <= KSIZE) {
      std::memcpy(_data + _writeIndex, data, size);
      _writeIndex += size;
    }
  }
  
  // Read raw data
  void readData(void* data, size_t size) {
    if(_readIndex + size <= _writeIndex) {
      std::memcpy(data, _data + _readIndex, size);
      _readIndex += size;
    }
  }
  
  // Get raw data pointer
  const uint8_t* data() const {
    return _data;
  }
  
  // Reset read position
  void resetRead() {
    _readIndex = 0;
  }
  
  // Compatibility with existing API
  void writeDataInternal(const void* data, size_t size) {
    writeData(data, size);
  }
  
  // Create an iterator for reading
  IpcMessagePacketIterator<KSIZE> makeIterator() const {
    return IpcMessagePacketIterator<KSIZE>(*this);
  }
  
  // Read string with iterator (for API compatibility)
  std::string readString(IpcMessagePacketIterator<KSIZE>& iter) {
    _readIndex = iter.index();
    std::string result = readString();
    iter.setIndex(_readIndex);
    return result;
  }
  
  // Read with iterator (for API compatibility)
  template<typename T>
  void read(T& value, IpcMessagePacketIterator<KSIZE>& iter) {
    _readIndex = iter.index();
    value = read<T>();
    iter.setIndex(_readIndex);
  }
  
  // Read raw data with iterator
  void readDataInternal(void* dest, size_t size, IpcMessagePacketIterator<KSIZE>& iter) {
    _readIndex = iter.index();
    readData(dest, size);
    iter.setIndex(_readIndex);
  }
};

// Verify POD-ness at compile time
template<size_t KSIZE>
struct is_pod_check {
  static_assert(std::is_trivially_copyable<IpcMessagePacket<KSIZE>>::value, 
                "IpcMessagePacket must be trivially copyable for shared memory safety");
  static_assert(std::is_standard_layout<IpcMessagePacket<KSIZE>>::value, 
                "IpcMessagePacket must have standard layout for shared memory safety");
};

} // namespace ork