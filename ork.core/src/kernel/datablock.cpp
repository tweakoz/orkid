////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/datablock.h>
namespace ork {
///////////////////////////////////////////////////////////////////////////////
DataBlock::hasher_t DataBlock::createHasher() {
  return std::make_shared<::boost::Crc64>();
}
///////////////////////////////////////////////////////////////////////////////
DataBlock::DataBlock(const void* buffer, size_t len) {

  _vars = std::make_shared<varmap::VarMap>();

  if (buffer and len)
    addData(buffer, len);
}
///////////////////////////////////////////////////////////////////////////////
const uint8_t* DataBlock::data(size_t index) const {
  return (const uint8_t*)_storage.data() + index;
}
///////////////////////////////////////////////////////////////////////////////
void DataBlock::reserve(size_t len) {
  _storage.reserve(len);
}
///////////////////////////////////////////////////////////////////////////////
size_t DataBlock::length() const {
  return _storage.size();
}
///////////////////////////////////////////////////////////////////////////////
void* DataBlock::allocateBlock(size_t length) {
  size_t prev_length = _storage.size();
  _storage.resize(prev_length + length);
  auto cursor = _storage.data() + prev_length;
  return (void*)cursor;
}
///////////////////////////////////////////////////////////////////////////////
void DataBlock::addData(const void* ptr, size_t length) {
  _append((unsigned char*)ptr, length);
}
///////////////////////////////////////////////////////////////////////////////
bool DataBlock::_append(const unsigned char* buffer, size_t bufmax) {
  if (bufmax != 0)
    _storage.insert(_storage.end(), buffer, buffer + bufmax);
  return true;
}
///////////////////////////////////////////////////////////////////////////////
uint64_t DataBlock::hash() const {
  boost::Crc64 hasher;
  hasher.accumulateString(_name);                      // identifier
  hasher.accumulate(_storage.data(), _storage.size()); // data content
  hasher.finish();
  return hasher.result();
}
///////////////////////////////////////////////////////////////////////////////
void DataBlock::accumlateHash(hasher_t hasher) const {
  hasher->accumulateString(_name);                      // identifier
  hasher->accumulate(_storage.data(), _storage.size()); // data content
}
///////////////////////////////////////////////////////////////////////////////

DataBlockInputStream::DataBlockInputStream(datablock_constptr_t block)
    : _datablock(block) {
}

const void* DataBlockInputStream::current() {
  return (const void*)(_datablock->data(_cursor));
}
const void* DataBlockInputStream::data(size_t idx) const {
  return (const void*)(_datablock->data(idx));
}
size_t DataBlockInputStream::length() const {
  return _datablock->_storage.size();
}
void DataBlockInputStream::advance(size_t l) {
  _cursor += l;
}
void DataBlockInputStream::resetCursor() {
  _cursor = 0;
}

///////////////////////////////////////////////////////////////////////////////
/*template< typename T > void DataBlockInputStream::RefItem( T* &item )
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
