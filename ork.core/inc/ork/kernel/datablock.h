////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <cstdint>
#include <ork/util/crc64.h>
#include <ork/util/xxhash.inl>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/fixedstring.h>

namespace ork {

struct DataBlock;
using datablock_ptr_t = std::shared_ptr<DataBlock>;
using datablock_constptr_t = std::shared_ptr<const DataBlock>;
using datablock_list_t = std::vector<datablock_ptr_t>;
using datablock_crcmap_t = std::unordered_map<uint32_t, datablock_ptr_t>;

///////////////////////////////////////////////////////////////////////////////
/// DataBlock : SerDes container for arbitrary binary data
///   with convenience methods for getting data in and out
///   The data is serdes'ed to and from a vector of bytes
///////////////////////////////////////////////////////////////////////////////

struct EncryptionCodec{
  virtual ~EncryptionCodec() {}
  virtual datablock_ptr_t encrypt(const DataBlock* inp) = 0;
  virtual datablock_ptr_t decrypt(const DataBlock* inp) = 0;
  svar64_t _impl;
};
struct DefaultEncryptionCodec : public EncryptionCodec {
  DefaultEncryptionCodec();
  datablock_ptr_t encrypt(const DataBlock* inp) final;
  datablock_ptr_t decrypt(const DataBlock* inp) final;
};

using encryptioncodec_ptr_t = std::shared_ptr<EncryptionCodec>;
encryptioncodec_ptr_t encryptionCodecFactory(uint32_t codecID);

struct DataBlock {
  /////////////////////////////////////////////
  using hasher_t = xxh64hasher_ptr_t;
  /////////////////////////////////////////////
  static hasher_t createHasher();
  /////////////////////////////////////////////
  DataBlock(const void* buffer = nullptr, size_t length = 0);
  /////////////////////////////////////////////
  /// append bytes of an individual item into datablock
  template <typename T> void addItem(const T& data);
  /////////////////////////////////////////////
  /// append bytes of an N individual items into datablock
  ///  expecting caller to fill in bytes (using returned pointer)
  template <typename T> T* allocateItems(size_t itemcount);
  /////////////////////////////////////////////
  /// reserve len bytes in container (for optimizing allocations)
  void reserve(size_t len);
  /////////////////////////////////////////////
  /// append data from memory into datablock
  void addData(const void* data, size_t len);
  /////////////////////////////////////////////
  /// reference data at index
  const uint8_t* data(size_t index = 0) const;
  /////////////////////////////////////////////
  /// append a chunk of length
  ///  expecting caller to fill in bytes (using returned pointer)
  void* allocateBlock(size_t length);
  /////////////////////////////////////////////
  /// current length of datablock
  size_t length() const;
  /////////////////////////////////////////////
  bool is_ascii() const;
  bool is_likely_json() const;
  void zeroExtend();
  /////////////////////////////////////////////
  /// hash of entire datablock (will incur a computational cost)
  uint64_t hash() const;
  /////////////////////////////////////////////
  /// accumulate hash of datablock into pre-existing hasher
  void accumlateHash(hasher_t hasher) const;
  /////////////////////////////////////////////
  bool _append(const unsigned char* buffer, size_t bufmax);
  /////////////////////////////////////////////
  datablock_ptr_t encrypt(encryptioncodec_ptr_t codec) const;
  /////////////////////////////////////////////
  datablock_ptr_t decrypt(encryptioncodec_ptr_t codec) const;
  /////////////////////////////////////////////
  std::vector<uint8_t> _storage;
  std::shared_ptr<varmap::VarMap> _vars;
  std::string _name = "noname";
};

///////////////////////////////////////////////////////////////////////////////
template <typename T> T* DataBlock::allocateItems(size_t itemcount) {
  size_t length = itemcount * sizeof(T);
  void* block   = allocateBlock(length);
  return (T*)block;
}
///////////////////////////////////////////////////////////////////////////////
template <typename T> void DataBlock::addItem(const T& data) {
  _append((unsigned char*)&data, sizeof(data));
}
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// DataBlockInputStream : non-mutating read context for a datablock
///////////////////////////////////////////////////////////////////////////////

struct DataBlockInputStream {

  DataBlockInputStream(datablock_constptr_t block = nullptr);
  /////////////////////////////////////////////
  /// memory at current cursor
  const void* current();
  /////////////////////////////////////////////
  /// memory at random index
  const void* data(size_t idx) const;
  /////////////////////////////////////////////
  /// length of datablock
  size_t length() const;
  /////////////////////////////////////////////
  /// advance cursor n bytes
  void advance(size_t l);
  /////////////////////////////////////////////
  /// reset cursor back to 0
  void resetCursor();
  /////////////////////////////////////////////
  /// get item, advancing cursor by sizeof(T)
  template <typename T> T getItem();

  datablock_constptr_t _datablock;
  size_t _cursor = 0;
};

///////////////////////////////////////////////////////////////////////////////
template <typename T> T DataBlockInputStream::getItem() {
  size_t isize = sizeof(T);
  OrkAssert((_cursor + isize) <= length());
  auto pt = (T*)_datablock->data(_cursor);
  _cursor += isize;
  return *pt;
}

} // namespace ork
