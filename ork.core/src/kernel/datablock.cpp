////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/datablock.h>
#include <ork/util/crc.h>

namespace ork {
///////////////////////////////////////////////////////////////////////////////
encryptioncodec_ptr_t encryptionCodecFactory(uint32_t codecID){
  switch(codecID){
    case "default_encryption"_crcu:
      return std::make_shared<DefaultEncryptionCodec>();
      break;
    default:
      OrkAssert(false);
      break;
  }
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
DefaultEncryptionCodec::DefaultEncryptionCodec(){
}
///////////////////////////////////////////////////////////////////////////////
datablock_ptr_t DefaultEncryptionCodec::encrypt(const DataBlock* inp) {
  auto rval = std::make_shared<DataBlock>();
  rval->_name = inp->_name;
  rval->_vars = inp->_vars;
  size_t size = inp->_storage.size();
  rval->_storage.reserve(size+4);
  auto encmagic = Char4("oems");
  auto data = (uint8_t*) encmagic.mCharMems;
  rval->addItem<uint32_t>(encmagic.muVal32);
  rval->addItem<uint32_t>("default_encryption"_crcu);
  uint32_t counter = (5<<0)|(1<<8)|(11<<16);
  for (uint8_t byte : inp->_storage) {
    byte += (counter&0xff);
    rval->_storage.push_back(byte);
    counter++;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
datablock_ptr_t DefaultEncryptionCodec::decrypt(const DataBlock* inp) {
  auto rval = std::make_shared<DataBlock>();
  rval->_name = inp->_name;
  rval->_vars = inp->_vars;
  size_t size = inp->_storage.size();
  rval->_storage.reserve(size);
  uint32_t counter = (5<<0)|(1<<8)|(11<<16);
  for (uint8_t byte : inp->_storage) {
    byte -= (counter&0xff);
    rval->_storage.push_back(byte);
    counter++;
  }
  return rval;  
}
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
datablock_ptr_t DataBlock::encrypt(encryptioncodec_ptr_t codec) const {
  return codec->encrypt(this);
}
///////////////////////////////////////////////////////////////////////////////
datablock_ptr_t DataBlock::decrypt(encryptioncodec_ptr_t codec) const {
  return codec->decrypt(this);
}
///////////////////////////////////////////////////////////////////////////////
const uint8_t* DataBlock::data(size_t index) const {
  return (const uint8_t*)_storage.data() + index;
}
///////////////////////////////////////////////////////////////////////////////
bool DataBlock::is_ascii() const {
  for( uint8_t byte : _storage ){
    if( byte>=128 )
      return false;
  }
  return true;
}
///////////////////////////////////////////////////////////////////////////////
bool DataBlock::is_likely_json() const {

    std::stack<char> brackets;
    bool inQuote = false;
    char prevChar = 0;
    char ch;

    for( auto ch : _storage ) {
        // Skip characters within quotes
        if (ch == '"' && prevChar != '\\') {
            inQuote = !inQuote;
            continue;
        }
        if (inQuote) {
            prevChar = ch;
            continue;
        }

        switch (ch) {
            case '{':
            case '[':
                brackets.push(ch);
                break;
            case '}':
                if (brackets.empty() || brackets.top() != '{') return false;
                brackets.pop();
                break;
            case ']':
                if (brackets.empty() || brackets.top() != '[') return false;
                brackets.pop();
                break;
        }
        prevChar = ch;
    }

    return brackets.empty();
}
void DataBlock::zeroExtend(){
  _storage.push_back(0);
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
