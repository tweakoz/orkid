////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/string/string.h>
#include <ork/util/crc.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/kernel/memcpy.inl>
#include <ork/util/hexdump.inl>

using namespace std::literals;

namespace ork { namespace chunkfile {

///////////////////////////////////////////////////////////////////////////////
void OutputStream::addIndexedString(const std::string& str, Writer& writer){
  uint64_t index = writer.stringIndex(str);
  addItem<uint64_t>(index);
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addData(const void* ptr, size_t length) {
  Write((unsigned char*)ptr, length);
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addDataBlock(datablock_ptr_t dblock) {
  addData(dblock->data(), dblock->length());
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const bool& data) {
  bool temp = data;
  swapbytes_dynamic(temp);
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const uint8_t& data) {
  Write(&data, sizeof(data));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const uint16_t& data) {
  uint16_t temp = data;
  swapbytes_dynamic(temp);
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const uint32_t& data) {
  uint32_t temp = data;
  swapbytes_dynamic(temp);
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const uint64_t& data) {
  uint64_t temp = data;
  swapbytes_dynamic(temp);
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const int& data) {
  int temp = data;
  swapbytes_dynamic(temp);
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const float& data) {
  float temp = data;
  swapbytes_dynamic(temp);
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const fmtx4& data) {
  fmtx4 temp = data;
  for (int i = 0; i < 16; i++) {
    swapbytes_dynamic(temp.asArray()[i]);
  }
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const fquat& data) {
  fquat temp = data;
  for (int i = 0; i < 4; i++) {
    swapbytes_dynamic(temp.asArray()[i]);
  }
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const fvec4& data) {
  fvec4 temp = data;
  for (int i = 0; i < 4; i++) {
    swapbytes_dynamic(temp.asArray()[i]);
  }
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const fvec3& data) {
  fvec3 temp = data;
  for (int i = 0; i < 3; i++) {
    swapbytes_dynamic(temp.asArray()[i]);
  }
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addItem(const fvec2& data) {
  fvec2 temp = data;
  for (int i = 0; i < 2; i++) {
    swapbytes_dynamic(temp.asArray()[i]);
  }
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addVarMap(const varmap::VarMap& vmap, Writer& writer) {
  addItem<size_t>("BeginVarMap"_crcu);
  addItem<size_t>(vmap._themap.size());
  for (const auto& item : vmap._themap) {
    const auto& key        = item.first;
    const auto& val        = item.second;
    size_t keystring_index = writer.stringIndex(key.c_str());
    addItem<size_t>(keystring_index);
    if (auto as = val.tryAs<std::string>()) {
      addItem<uint64_t>("std::string"_crcu);
      size_t str_index = writer.stringIndex(as.value().c_str());
      addItem<size_t>(str_index);
    } else if (auto as = val.tryAs<bool>()) {
      addItem<uint64_t>("bool"_crcu);
      addItem<bool>(as.value());
    } else if (auto as = val.tryAs<int32_t>()) {
      addItem<uint64_t>("int32_t"_crcu);
      addItem<int32_t>(as.value());
    } else if (auto as = val.tryAs<uint32_t>()) {
      addItem<uint64_t>("uint32_t"_crcu);
      addItem<uint32_t>(as.value());
    } else if (auto as = val.tryAs<int64_t>()) {
      addItem<uint64_t>("int64_t"_crcu);
      addItem<int64_t>(as.value());
    } else if (auto as = val.tryAs<uint64_t>()) {
      addItem<uint64_t>("uint64_t"_crcu);
      addItem<uint64_t>(as.value());
    } else if (auto as = val.tryAs<size_t>()) {
      addItem<uint64_t>("size_t"_crcu);
      addItem<size_t>(as.value());
    } else if (auto as = val.tryAs<float>()) {
      addItem<uint64_t>("float"_crcu);
      addItem<float>(as.value());
    } else if (auto as = val.tryAs<double>()) {
      addItem<uint64_t>("double"_crcu);
      addItem<double>(as.value());
    } else if (auto as = val.tryAs<fvec2>()) {
      addItem<uint64_t>("fvec2"_crcu);
      addItem<fvec2>(as.value());
    } else if (auto as = val.tryAs<fvec3>()) {
      addItem<uint64_t>("fvec3"_crcu);
      addItem<fvec3>(as.value());
    } else if (auto as = val.tryAs<fvec4>()) {
      addItem<uint64_t>("fvec4"_crcu);
      addItem<fvec4>(as.value());
    } else if (auto as = val.tryAs<fquat>()) {
      addItem<uint64_t>("fquat"_crcu);
      addItem<fquat>(as.value());
    } else if (auto as = val.tryAs<fmtx3>()) {
      addItem<uint64_t>("fmtx3"_crcu);
      addItem<fmtx3>(as.value());
    } else if (auto as = val.tryAs<fmtx4>()) {
      addItem<uint64_t>("fmtx4"_crcu);
      addItem<fmtx4>(as.value());
    } else {
      OrkAssert(false);
    }
  }
  addItem<size_t>("EndVarMap"_crcu);
}
///////////////////////////////////////////////////////////////////////////////

bool OutputStream::Write(const unsigned char* buffer, size_type bufmax) // virtual
{
  if (bufmax != 0)
    _data.insert(_data.end(), buffer, buffer + bufmax);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

OutputStream::OutputStream() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

InputStream::InputStream(const void* pb, size_t ilength)
    : mpbase(pb)
    , midx(0)
    , milength(ilength) {
}

////////////////////////////////////////////////////////////////////////////////////

const void* InputStream::GetCurrent() {
  const char* pchbase = (const char*)mpbase;
  return (const void*)&pchbase[midx];
}

void InputStream::dump() const{
  hexdumpbytes((const uint8_t*) mpbase, milength);
}

////////////////////////////////////////////////////////////////////////////////////

std::string InputStream::readIndexedString(const Reader& reader){
  uint64_t index;
  GetItem<uint64_t>(index);
  return reader.GetString(index);
}

////////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> InputStream::readData(size_t length){
  std::vector<uint8_t> data;
  data.resize(length);
  memcpy_fast(data.data(), GetCurrent(), length);
  midx += length;
  return data;
}

////////////////////////////////////////////////////////////////////////////////////

template <> std::string InputStream::readItem<std::string>(){
  uint64_t size;
  GetItem<uint64_t>(size);
  printf( "size<0x%zx>\n", size);
  std::vector<uint8_t> data = readData(size);
  data.push_back(0);
  return std::string((const char*)data.data());
}

////////////////////////////////////////////////////////////////////////////////////

void* InputStream::GetDataAt(size_t idx) {
  OrkAssert(idx < milength);
  const char* pchbase = (const char*)mpbase;
  return (void*)&pchbase[idx];
}

////////////////////////////////////////////////////////////////////////////////////

void InputStream::getVarMap(varmap::VarMap& out_vmap, const Reader& reader) {
  size_t mkr_beginvarmap = 0;
  size_t mkr_endvarmap   = 0;
  GetItem<size_t>(mkr_beginvarmap);
  OrkAssert(mkr_beginvarmap == "BeginVarMap"_crcu);
  size_t num_items = 0;
  GetItem<size_t>(num_items);
  for (size_t index = 0; index < num_items; index++) {
    size_t keystring_index = 0;
    uint64_t typeID        = 0;
    GetItem<size_t>(keystring_index);
    GetItem<uint64_t>(typeID);
    const char* key_string = reader.GetString(keystring_index);
    switch (typeID) {
      case "std::string"_crcu: {
        std::string value;
        size_t sidx = 0;
        GetItem<size_t>(sidx);
        out_vmap[key_string] = reader.GetString(sidx);
        break;
      }
      case "bool"_crcu: {
        bool value;
        GetItem<bool>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "int32_t"_crcu: {
        int32_t value;
        GetItem<int32_t>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "uint32_t"_crcu: {
        uint32_t value;
        GetItem<uint32_t>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "int64_t"_crcu: {
        int64_t value;
        GetItem<int64_t>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "uint64_t"_crcu: {
        uint64_t value;
        GetItem<uint64_t>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "float"_crcu: {
        float value;
        GetItem<float>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "double"_crcu: {
        double value;
        GetItem<double>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "fvec2"_crcu: {
        fvec2 value;
        GetItem<fvec2>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "fvec3"_crcu: {
        fvec3 value;
        GetItem<fvec3>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "fvec4"_crcu: {
        fvec4 value;
        GetItem<fvec4>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "fquat"_crcu: {
        fquat value;
        GetItem<fquat>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "fmtx3"_crcu: {
        fmtx3 value;
        GetItem<fmtx3>(value);
        out_vmap[key_string] = value;
        break;
      }
      case "fmtx4"_crcu: {
        fmtx4 value;
        GetItem<fmtx4>(value);
        out_vmap[key_string] = value;
        break;
      }
      default:
        OrkAssert(false);
    }
  }
  GetItem<size_t>(mkr_endvarmap);
  OrkAssert(mkr_endvarmap == "EndVarMap"_crcu);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Reader::~Reader() {
  for (typename StreamLut::const_iterator it = mInputStreams.begin(); it != mInputStreams.end(); it++) {
    const PoolString& pname = it->first;
    InputStream* stream     = it->second;
    if (stream->GetLength()) {
      _allocator.done(pname.c_str(), stream->GetDataAt(0));
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
Reader::Reader(const file::Path& inpath, const char* ptype, ILoadAllocator& allocator)
    : _strtablen(0)
    , mpstrtab(0)
    , mbOk(false)
    , _allocator(allocator) {

  if (FileEnv::GetRef().DoesFileExist(inpath)) {
    ork::File inputfile(inpath, ork::EFM_READ);
    size_t length = 0;
    inputfile.GetLength(length);
    auto dblock = std::make_shared<DataBlock>();
    void* dest  = dblock->allocateBlock(length);
    inputfile.Read(dest, length);
    inputfile.Close();
    mbOk = readFromDataBlock(dblock);
    OrkAssert(_chunkfiletype == std::string(ptype));
  }
}
///////////////////////////////////////////////////////////////////////////////
Reader::Reader(datablock_ptr_t datablock, ILoadAllocator& allocator)
    : _strtablen(0)
    , mpstrtab(0)
    , mbOk(false)
    , _allocator(allocator) {
  mbOk = readFromDataBlock(datablock);
}
///////////////////////////////////////////////////////////////////////////////
bool Reader::readFromDataBlock(datablock_ptr_t datablock) {
  DataBlockInputStream dblockstream(datablock);
  const Char4 good_chunk_magic("chkf");
  OrkHeapCheck();
  ///////////////////////////
  Char4 check_chunk_magic(dblockstream.getItem<uint32_t>());
  if (check_chunk_magic != good_chunk_magic)
    return false;
  check_chunk_magic = dblockstream.getItem<uint32_t>();
  if (check_chunk_magic != good_chunk_magic)
    return false;
  ///////////////////////////
  int32_t ifiletype     = dblockstream.getItem<size_t>();
  ///////////////////////////
  int32_t inumchunks = dblockstream.getItem<size_t>();
  ///////////////////////////
  _strtablen = dblockstream.getItem<size_t>();
  char* pst   = new char[_strtablen];
  memcpy_fast(pst, dblockstream.current(), _strtablen);
  dblockstream.advance(_strtablen);
  mpstrtab = pst;
  OrkHeapCheck();
  ///////////////////////////
  #error fixme 
  //size_t marker = dblockstream.getItem<size_t>();
  //OrkAssert(marker == 0x0123456789abcdef);
  ///////////////////////////
  _chunkfiletype        = mpstrtab + ifiletype;
  OrkHeapCheck();
  ///////////////////////////
  printf( "///////////////////chunkreader strtable////////////////////\n");
  printf( "strtab_len<0x%zx>\n", _strtablen );
  hexdumpbytes((const uint8_t*) mpstrtab, _strtablen);
  printf( "///////////////////////////////////////////////////////////\n");
  ///////////////////////////
  for (int ic = 0; ic < inumchunks; ic++) {
    size_t ichunkid    = dblockstream.getItem<size_t>();
    size_t ioffset     = dblockstream.getItem<size_t>();
    size_t ichunklen   = dblockstream.getItem<size_t>();
    PoolString psname  = AddPooledString(GetString(ichunkid));

    printf( "parsing chunk<%s> offset<0x%zx> len<0x%zx>\n", psname.c_str(), ioffset, ichunklen );

    InputStream* stream = &mStreamBank[ic];
    OrkHeapCheck();
    if (ichunklen) {
      void* pdata = _allocator.alloc(psname.c_str(), ichunklen);
      OrkAssert(pdata != 0);
      new (stream) InputStream(pdata, ichunklen);
      mInputStreams.AddSorted(psname, stream);
    } else {
      new (stream) InputStream(0, 0);
      mInputStreams.AddSorted(psname, stream);
    }
    stream->_streamname = psname.c_str();
    OrkHeapCheck();
  }
  ///////////////////////////
  for (int ic = 0; ic < inumchunks; ic++) {
    auto inputstream = &mStreamBank[ic];
    if (inputstream->GetLength()) {
      auto destaddr = inputstream->GetDataAt(0);
      size_t length = inputstream->GetLength();
      printf( "COPY TO INPUTSTREAM<%s> addr<%p> size<0x%zx>\n", //
              inputstream->_streamname.c_str(), //
              destaddr, //
              length );
      memcpy_fast(destaddr, dblockstream.current(), length);
      dblockstream.advance(length);
    }
  }
  ///////////////////////////
  return true;
}
////////////////////////////////////////////////////////////////////////////////////
InputStream* Reader::GetStream(const char* streamname) {
  PoolString ps                         = ork::AddPooledString(streamname);
  typename StreamLut::const_iterator it = mInputStreams.find(ps);
  return (it == mInputStreams.end()) ? 0 : it->second;
}
////////////////////////////////////////////////////////////////////////////////////
const char* Reader::GetString(size_t index) const {
  OrkAssert(index < _strtablen);
  OrkAssert(mpstrtab);
  return mpstrtab + index;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Writer::Writer(const char* file_type) {
  _filetype = stringIndex(file_type);
}

////////////////////////////////////////////////////////////////////////////////////

OutputStream* Writer::AddStream(std::string stream_name) {
  OutputStream* nstream = new OutputStream;
  int ichunkid          = _stringblock.AddString(stream_name.c_str()).Index();
  mOutputStreams.insert(std::make_pair(ichunkid, nstream));
  nstream->_name = stream_name;
  return nstream;
}

////////////////////////////////////////////////////////////////////////////////////

size_t Writer::stringIndex(const char* pstr) {
  return _stringblock.AddString(pstr).Index();
}
size_t Writer::stringIndex(const std::string& pstr) {
  return _stringblock.AddString(pstr.c_str()).Index();
}

////////////////////////////////////////////////////////////////////////////////////

void Writer::writeToDataBlock(datablock_ptr_t& out_datablock) {
  ////////////////////////
  Char4 chunk_magic("chkf");
  out_datablock->addItem<Char4>(chunk_magic);
  out_datablock->addItem<Char4>(chunk_magic);
  out_datablock->addItem<size_t>(_filetype);
  ////////////////////////
  int inumchunks = (int)mOutputStreams.size();
  out_datablock->addItem<size_t>(inumchunks);
  ////////////////////////
  size_t offset = 0;
  for (orkmap<int, OutputStream*>::const_iterator it = mOutputStreams.begin(); it != mOutputStreams.end(); it++) {
    size_t ichunkid         = it->first;
    OutputStream* stream = it->second;
    size_t chunklen        = stream->GetSize();
    ////////////////////////
    out_datablock->addItem<int>(ichunkid);
    out_datablock->addItem<size_t>(offset);
    out_datablock->addItem<size_t>(chunklen);
    ////////////////////////
    offset += chunklen;
  }
  ////////////////////////
  for (orkmap<int, OutputStream*>::const_iterator it = mOutputStreams.begin(); it != mOutputStreams.end(); it++) {
    size_t ichunkid         = it->first;
    OutputStream* stream = it->second;
    size_t ichunklen        = stream->GetSize();
    if (ichunklen && stream->GetData()) {
      out_datablock->addData(stream->GetData(), ichunklen);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////

void Writer::WriteToFile(const file::Path& outpath) {

  auto datablock = std::make_shared<DataBlock>();
  writeToDataBlock(datablock);
  ork::File outputfile(outpath, ork::EFM_WRITE);
  outputfile.Write(datablock->data(), datablock->length());
}



}} // namespace ork::chunkfile
