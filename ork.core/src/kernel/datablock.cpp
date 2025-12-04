////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/datablock.h>
#include <ork/kernel/opq.h>
#include <ork/util/crc.h>
#include <ork/util/xxhash.inl>
#include <random>

#define LZ4_DISABLE_DEPRECATE_WARNINGS
#include <lz4.h>
#include <lz4hc.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
std::string dblock_to_str(datablock_ptr_t db){
    if(db==nullptr)
        return "";
  std::string rval = FormatString("dblock[len<%d>]",db->_storage.size());
  return rval;
}
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
  return std::make_shared<XXH64HASH>();
}
///////////////////////////////////////////////////////////////////////////////
DataBlock::DataBlock(const void* buffer, size_t len) {

  _vars = std::make_shared<varmap::VarMap>();

  if (buffer and len)
    addData(buffer, len);
  else if(len){
    _storage.resize(len);
  }
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
void DataBlock::resize(size_t len) {
  _storage.resize(len);
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
  XXH64HASH xxh;
  xxh.init();
  xxh.accumulateString(_name);                      // identifier
  xxh.accumulate(_storage.data(), _storage.size()); // data content
  xxh.finish();
  return xxh.result();
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
/////////

datablock_ptr_t DataBlock::createFromPath(std::string path){
  FILE* fin = fopen(path.c_str(), "rb");
  if(fin){
    auto rval = std::make_shared<DataBlock>();
    rval->_name = path;
    fseek(fin, 0, SEEK_END);
    size_t len = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    rval->_storage.resize(len);
    fread(rval->_storage.data(), 1, len, fin);
    fclose(fin);
    return rval;
  }
  return nullptr;
}

datablock_ptr_t DataBlock::clone() const {
  auto rval = std::make_shared<DataBlock>();
  rval->_name = _name;
  rval->_vars = _vars;
  rval->_storage = _storage;
  return rval;
}

//////////////////////////////////////////////////////////////////////

static constexpr uint32_t MAGIC_LZ44 = 0x4C5A3434; // "LZ44" - LZ4 compressed
static constexpr uint32_t MAGIC_NONE = 0x4E4F4E45; // "NONE" - uncompressed (too large for LZ4)

datablock_ptr_t DataBlock::compressed(int level) const {
  if (_storage.empty()) {
    // Even for empty data, create proper LZ4 format with header
    auto output = std::make_shared<DataBlock>();
    output->_name = _name + ".lz4";
    output->addItem<uint32_t>(MAGIC_LZ44);
    output->addItem<uint64_t>(0); // uncompressed size = 0
    // No compressed data to add
    return output;
  }

  // Check if already compressed (idempotent) - need at least 4 bytes for magic
  if (_storage.size() >= 4) {
    uint32_t maybe_magic = *reinterpret_cast<const uint32_t*>(_storage.data());
    if (maybe_magic == MAGIC_LZ44 || maybe_magic == MAGIC_NONE) {
      // Already has compression header, return copy as-is
      auto output = std::make_shared<DataBlock>();
      output->_name = _name;
      output->_storage = _storage;
      return output;
    }
  }

  // Check if data exceeds LZ4 max input size - store uncompressed with NONE header
  if (_storage.size() > LZ4_MAX_INPUT_SIZE) {
    auto output = std::make_shared<DataBlock>();
    output->_name = _name + ".lz4"; // still use .lz4 extension for consistency

    // Reserve space for: magic(4) + uncompressed_size(8) + raw_data
    output->reserve(4 + 8 + _storage.size());

    // Write NONE magic number
    output->addItem<uint32_t>(MAGIC_NONE);

    // Write uncompressed size
    output->addItem<uint64_t>(_storage.size());

    // Copy raw data
    uint8_t* raw_buffer = static_cast<uint8_t*>(output->allocateBlock(_storage.size()));
    std::memcpy(raw_buffer, _storage.data(), _storage.size());

    return output;
  }

  // Determine max compressed size
  int max_compressed_size = LZ4_compressBound(_storage.size());

  // Create output datablock with header
  auto output = std::make_shared<DataBlock>();
  output->_name = _name + ".lz4";

  // Reserve space for: magic(4) + uncompressed_size(8) + compressed_data
  output->reserve(4 + 8 + max_compressed_size);

  // Write magic number
  output->addItem<uint32_t>(MAGIC_LZ44);

  // Write uncompressed size
  output->addItem<uint64_t>(_storage.size());

  // Allocate space for compressed data
  uint8_t* compressed_buffer = static_cast<uint8_t*>(output->allocateBlock(max_compressed_size));

  // Compress
  int compressed_size;
  if (level > 0) {
    // Use HC compression for higher levels
    compressed_size = LZ4_compress_HC(
      reinterpret_cast<const char*>(_storage.data()),
      reinterpret_cast<char*>(compressed_buffer),
      _storage.size(),
      max_compressed_size,
      level);
  } else {
    // Use fast compression
    compressed_size = LZ4_compress_default(
      reinterpret_cast<const char*>(_storage.data()),
      reinterpret_cast<char*>(compressed_buffer),
      _storage.size(),
      max_compressed_size);
  }

  if (compressed_size <= 0) {
    throw std::runtime_error("LZ4 compression failed");
  }

  // Trim to actual compressed size
  output->_storage.resize(4 + 8 + compressed_size);

  return output;
}

//////////////////////////////////////////////////////////////////////

datablock_ptr_t DataBlock::decompressed() const {
  // Check if data has no header (idempotent - already uncompressed)
  if (_storage.size() < 12) { // magic(4) + size(8)
    // Too small for header, assume already uncompressed - return copy
    auto output = std::make_shared<DataBlock>();
    output->_name = _name;
    output->_storage = _storage;
    return output;
  }

  DataBlockInputStream stream(std::make_shared<const DataBlock>(*this));

  // Check magic number
  uint32_t magic = stream.getItem<uint32_t>();

  // If no recognized magic, assume already uncompressed (idempotent)
  if (magic != MAGIC_LZ44 && magic != MAGIC_NONE) {
    auto output = std::make_shared<DataBlock>();
    output->_name = _name;
    output->_storage = _storage;
    return output;
  }

  // Read uncompressed size
  uint64_t uncompressed_size = stream.getItem<uint64_t>();

  // Create output datablock
  auto output = std::make_shared<DataBlock>();
  output->_name = _name;
  if (output->_name.ends_with(".lz4")) {
    output->_name = output->_name.substr(0, output->_name.length() - 4);
  }

  // Handle empty data case
  if (uncompressed_size == 0) {
    return output; // Return empty datablock
  }

  // Handle NONE magic - data is uncompressed, just strip header
  if (magic == MAGIC_NONE) {
    printf("[DEBUG decompressed] NONE magic detected: storage_size=%zu, uncompressed_size=%llu\n",
           _storage.size(), (unsigned long long)uncompressed_size);
    output->reserve(uncompressed_size);
    uint8_t* raw_buffer = static_cast<uint8_t*>(output->allocateBlock(uncompressed_size));
    const uint8_t* src_data = _storage.data() + 12; // skip header
    std::memcpy(raw_buffer, src_data, uncompressed_size);
    return output;
  }

  // Handle LZ44 magic - LZ4 compressed data
  output->reserve(uncompressed_size);

  // Allocate decompression buffer
  uint8_t* decompressed_buffer = static_cast<uint8_t*>(output->allocateBlock(uncompressed_size));

  // Decompress
  const uint8_t* compressed_data = _storage.data() + 12; // skip header
  size_t compressed_size = _storage.size() - 12;

  int decompressed_size = LZ4_decompress_safe(
    reinterpret_cast<const char*>(compressed_data),
    reinterpret_cast<char*>(decompressed_buffer),
    compressed_size,
    uncompressed_size);

  if (decompressed_size < 0 || static_cast<size_t>(decompressed_size) != uncompressed_size) {
    throw std::runtime_error("LZ4 decompression failed or size mismatch");
  }

  return output;
}

datablock_ptr_t DataBlock::createFromRandom(size_t length) {
  auto block = std::make_shared<DataBlock>();
  block->_name = "random_data";
  block->_storage.resize(length);
  
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 255);
  for (size_t i = 0; i < length; ++i) {
    block->_storage[i] = static_cast<uint8_t>(dis(gen));
  }
  
  return block;
}

///////////////////////////////////////////////////////////////////////////////
// DatablockFuture implementation
///////////////////////////////////////////////////////////////////////////////

DatablockFuture::DatablockFuture() {
}

DatablockFuture::~DatablockFuture() {
}

datablock_ptr_t DatablockFuture::wait() {
  while (!_completed && !_failed) {
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
  return _result;
}

bool DatablockFuture::isReady() const {
  return _completed || _failed;
}

float DatablockFuture::progress() const {
  if (_failed) return 0.0f;
  if (_completed) return 1.0f;
  return 0.5f; // Default progress for in-progress operations
}

//////////////////////////////////////////////////////////////////////
} // namespace ork
