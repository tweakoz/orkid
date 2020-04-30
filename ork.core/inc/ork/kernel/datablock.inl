#pragma once

#include <vector>
#include <cstdint>
#include <ork/util/crc64.h>
namespace ork {

struct DataBlock {

  DataBlock(const void* buffer = nullptr, size_t length = 0);
  void reserve(size_t len);
  void addData(const void* data, size_t len);
  template <typename T> void addItem(const T& data);
  bool _append(const unsigned char* buffer, size_t bufmax);
  const uint8_t* data(size_t index = 0) const {
    return (const uint8_t*)_storage.data() + index;
  }
  void* allocateBlock(size_t length);
  template <typename T> T* allocateItems(size_t itemcount);
  size_t length() const {
    return _storage.size();
  }
  uint64_t hash() const;
  std::vector<uint8_t> _storage;
  std::string _name = "noname";
};

typedef std::shared_ptr<DataBlock> datablockptr_t;

///////////////////////////////////////////////////////////////////////////////
inline DataBlock::DataBlock(const void* buffer, size_t len) {
  if (buffer and len)
    addData(buffer, len);
}
///////////////////////////////////////////////////////////////////////////////
inline void DataBlock::reserve(size_t len) {
  _storage.reserve(len);
}
///////////////////////////////////////////////////////////////////////////////
inline void* DataBlock::allocateBlock(size_t length) {
  size_t prev_length = _storage.size();
  _storage.resize(prev_length + length);
  auto cursor = _storage.data() + prev_length;
  return (void*)cursor;
}
///////////////////////////////////////////////////////////////////////////////
template <typename T> inline T* DataBlock::allocateItems(size_t itemcount) {
  size_t length = itemcount * sizeof(T);
  void* block   = allocateBlock(length);
  return (T*)block;
}
///////////////////////////////////////////////////////////////////////////////
inline void DataBlock::addData(const void* ptr, size_t length) {
  _append((unsigned char*)ptr, length);
}
///////////////////////////////////////////////////////////////////////////////
template <typename T> inline void DataBlock::addItem(const T& data) {
  _append((unsigned char*)&data, sizeof(data));
}
///////////////////////////////////////////////////////////////////////////////
inline bool DataBlock::_append(const unsigned char* buffer, size_t bufmax) {
  if (bufmax != 0)
    _storage.insert(_storage.end(), buffer, buffer + bufmax);
  return true;
}
///////////////////////////////////////////////////////////////////////////////
inline uint64_t DataBlock::hash() const {
  boost::Crc64 hasher;
  hasher.accumulateString(_name);                      // identifier
  hasher.accumulate(_storage.data(), _storage.size()); // data content
  hasher.finish();
  return hasher.result();
}
///////////////////////////////////////////////////////////////////////////////

struct DataBlockInputStream {
  DataBlockInputStream(datablockptr_t block = nullptr)
      : _datablock(block) {
  }
  template <typename T> T getItem();

  inline const void* current() {
    return (const void*)(_datablock->data(_cursor));
  }
  inline const void* data(size_t idx) const {
    return (const void*)(_datablock->data(idx));
  }
  size_t length() const {
    return _datablock->_storage.size();
  }
  void advance(size_t l) {
    _cursor += l;
  }
  void resetCursor() {
    _cursor = 0;
  }
  datablockptr_t _datablock;
  size_t _cursor = 0;
};

///////////////////////////////////////////////////////////////////////////////
template <typename T> inline T DataBlockInputStream::getItem() {
  size_t isize = sizeof(T);
  OrkAssert((_cursor + isize) <= length());
  auto pt = (T*)_datablock->data(_cursor);
  _cursor += isize;
  return *pt;
}
///////////////////////////////////////////////////////////////////////////////
/*template< typename T > inline void DataBlockInputStream::RefItem( T* &item )
{
    int isize = sizeof( T );
    int ileft = milength - midx;
    OrkAssert((midx+isize)<=milength);
    const char *pchbase = (const char*) mpbase;
    item = (T*) & pchbase[ midx ];
    midx += isize;
}*/
///////////////////////////////////////////////////////////////////////////////

} // namespace ork
