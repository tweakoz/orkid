////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/file.h>
#include <ork/util/endian.h>
#include <ork/stream/IOutputStream.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector4.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector2.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/fixedlut.hpp>
#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/datablock.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

namespace chunkfile {

struct Reader;
struct Writer;

class OutputStream : public ork::stream::IOutputStream {
public:
  bool Write(const unsigned char* buffer, size_type bufmax); // virtual

  OutputStream();

  void reserve(size_t len) {
    _data.reserve(len);
  }

  /////////////////////////////////////////////
  template <typename T> void AddItem(const T& data);
  void AddItem(const bool& data);
  void AddItem(const uint8_t& data);
  void AddItem(const uint16_t& data);

  void AddItem(const int32_t& data);
  void AddItem(const uint32_t& data);
  void AddItem(const int64_t& data);
  void AddItem(const uint64_t& data);

  void AddItem(const float& data);
  void AddItem(const double& data);

  void AddItem(const fmtx4& data);
  void AddItem(const fvec4& data);
  void AddItem(const fvec3& data);
  void AddItem(const fvec2& data);
  void AddItem(const fquat& data);
  void addVarMap(const varmap::VarMap& vmap, Writer& writer);
  void AddData(const void* ptr, size_t length);
  void AddDataBlock(datablock_ptr_t dblock);
  /////////////////////////////////////////////

  size_t GetSize() const {
    return _data.size();
  }
  const void* GetData() const {
    return GetSize() ? &_data[0] : 0;
  }

  /////////////////////////////////////////////

private:
  std::vector<uint8_t> _data;
};

///////////////////////////////////////////////////////////////////////////////

struct Writer {

  ////////////////////////////////////////////////////////////////////////////////////

  Writer(const char* file_type);
  OutputStream* AddStream(std::string stream_name);
  int stringIndex(const char* pstr);
  void WriteToFile(const file::Path& outpath);
  void writeToDataBlock(datablock_ptr_t& out_datablock);

  ////////////////////////////////////////////////////////////////////////////////////

  StringBlock _stringblock;
  int _filetype;

  orkmap<int, OutputStream*> mOutputStreams;
};

struct InputStream {
  InputStream(const void* pb = 0, size_t ilength = 0);
  template <typename T> void GetItem(T& item);
  template <typename T> void RefItem(T*& item);
  void getVarMap(varmap::VarMap& out_vmap, const Reader& reader);

  const void* GetCurrent();
  void* GetDataAt(size_t idx);
  size_t GetLength() const {
    return milength;
  }
  void advance(size_t l) {
    midx += l;
  }
  const void* mpbase;
  size_t midx;
  size_t milength;
};

///////////////////////////////////////////////////////////////////////////////

struct ILoadAllocator { //////////////////////////////
  virtual ~ILoadAllocator() {
  }
  virtual void* alloc(const char* pchkname, int ilen)  = 0;
  virtual void done(const char* pchkname, void* pdata) = 0;
};

struct DefaultLoadAllocator : public ILoadAllocator { //////////////////////////////
  inline void* alloc(const char* pchkname, int ilen) final {
    return malloc(ilen);
  }
  inline void done(const char* pchkname, void* pdata) final {
    free(pdata);
  }
};

///////////////////////////////////////////////////////////////////////////////

struct Reader {

  Reader(const file::Path& inpath, const char* ptype, ILoadAllocator& allocator);
  Reader(datablock_ptr_t datablock, ILoadAllocator& allocator);
  ~Reader();

  bool readFromDataBlock(datablock_ptr_t datablock);

  InputStream* GetStream(const char* streamname);
  const char* GetString(int index) const;

  bool IsOk() const {
    return mbOk;
  }

  static const int kmaxstreams = 16;
  InputStream mStreamBank[kmaxstreams];
  typedef ork::fixedlut<ork::PoolString, InputStream*, kmaxstreams> StreamLut;

  int mistrtablen;
  const char* mpstrtab;
  bool mbOk;
  std::string _chunkfiletype;

  StreamLut mInputStreams;
  ILoadAllocator& _allocator;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace chunkfile
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
