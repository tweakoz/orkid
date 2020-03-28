////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/string/string.h>
#include <ork/util/crc.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>

using namespace std::literals;

namespace ork { namespace chunkfile {

///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddData(const void* ptr, size_t length) {
  Write((unsigned char*)ptr, length);
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem(const bool& data) {
  bool temp = data;
  swapbytes_dynamic(temp);
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem(const unsigned char& data) {
  Write(&data, sizeof(data));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem(const unsigned short& data) {
  unsigned short temp = data;
  swapbytes_dynamic(temp);
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem(const int& data) {
  int temp = data;
  swapbytes_dynamic(temp);
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem(const float& data) {
  float temp = data;
  swapbytes_dynamic(temp);
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem(const fmtx4& data) {
  fmtx4 temp = data;
  for (int i = 0; i < 16; i++) {
    swapbytes_dynamic(temp.GetArray()[i]);
  }
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem(const fvec4& data) {
  fvec4 temp = data;
  for (int i = 0; i < 4; i++) {
    swapbytes_dynamic(temp.GetArray()[i]);
  }
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem(const fvec3& data) {
  fvec3 temp = data;
  for (int i = 0; i < 3; i++) {
    swapbytes_dynamic(temp.GetArray()[i]);
  }
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem(const fvec2& data) {
  fvec2 temp = data;
  for (int i = 0; i < 2; i++) {
    swapbytes_dynamic(temp.GetArray()[i]);
  }
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::addVarMap(const varmap::VarMap& vmap, Writer& writer) {
  AddItem<size_t>(vmap._themap.size());
  for (const auto& item : vmap._themap) {
    const auto& key        = item.first;
    const auto& val        = item.second;
    size_t keystring_index = writer.stringIndex(key.c_str());
    AddItem<size_t>(keystring_index);
    if (auto as = val.TryAs<std::string>()) {
      AddItem<uint64_t>("std::string"_crcu);
    } else if (auto as = val.TryAs<bool>()) {
      AddItem<uint64_t>("bool"_crcu);
      AddItem<bool>(as.value());
    } else if (auto as = val.TryAs<int32_t>()) {
      AddItem<uint64_t>("int32_t"_crcu);
      AddItem<int32_t>(as.value());
    } else if (auto as = val.TryAs<uint32_t>()) {
      AddItem<uint64_t>("uint32_t"_crcu);
      AddItem<uint32_t>(as.value());
    } else if (auto as = val.TryAs<int64_t>()) {
      AddItem<uint64_t>("int64_t"_crcu);
      AddItem<int64_t>(as.value());
    } else if (auto as = val.TryAs<uint64_t>()) {
      AddItem<uint64_t>("uint64_t"_crcu);
      AddItem<uint64_t>(as.value());
    } else if (auto as = val.TryAs<size_t>()) {
      AddItem<uint64_t>("size_t"_crcu);
      AddItem<size_t>(as.value());
    } else if (auto as = val.TryAs<float>()) {
      AddItem<uint64_t>("float"_crcu);
      AddItem<float>(as.value());
    } else if (auto as = val.TryAs<double>()) {
      AddItem<uint64_t>("double"_crcu);
      AddItem<double>(as.value());
    } else if (auto as = val.TryAs<fvec2>()) {
      AddItem<uint64_t>("fvec2"_crcu);
      AddItem<fvec2>(as.value());
    } else if (auto as = val.TryAs<fvec3>()) {
      AddItem<uint64_t>("fvec3"_crcu);
      AddItem<fvec3>(as.value());
    } else if (auto as = val.TryAs<fvec4>()) {
      AddItem<uint64_t>("fvec4"_crcu);
      AddItem<fvec4>(as.value());
    } else if (auto as = val.TryAs<fquat>()) {
      AddItem<uint64_t>("fquat"_crcu);
      AddItem<fquat>(as.value());
    } else if (auto as = val.TryAs<fmtx3>()) {
      AddItem<uint64_t>("fmtx3"_crcu);
      AddItem<fmtx3>(as.value());
    } else if (auto as = val.TryAs<fmtx4>()) {
      AddItem<uint64_t>("fmtx4"_crcu);
      AddItem<fmtx4>(as.value());
    } else {
      OrkAssert(false);
    }
  }
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

const void* InputStream::GetCurrent() {
  const char* pchbase = (const char*)mpbase;
  return (const void*)&pchbase[midx];
}

void* InputStream::GetDataAt(size_t idx) {
  OrkAssert(idx < milength);
  const char* pchbase = (const char*)mpbase;
  return (void*)&pchbase[idx];
}
void InputStream::getVarMap(varmap::VarMap& out_vmap, const Reader& reader) {
  size_t num_items = 0;
  for (size_t index = 0; index < num_items; index++) {
    size_t keystring_index = 0;
    uint64_t typeID        = 0;
    GetItem<size_t>(keystring_index);
    GetItem<uint64_t>(typeID);
    const char* key_string = reader.GetString(keystring_index);
    switch (typeID) {
      case "std::string"_crcu: {
        std::string value;
        GetItem<std::string>(value);
        out_vmap[key_string] = value;
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

  OrkAssert(false);
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
    : mpstrtab(0)
    , mistrtablen(0)
    , mbOk(false)
    , _allocator(allocator) {
  const Char4 good_chunk_magic("chkf");
  OrkHeapCheck();
  if (FileEnv::GetRef().DoesFileExist(inpath)) {
    ork::File inputfile(inpath, ork::EFM_READ);
    OrkHeapCheck();

    ///////////////////////////
    Char4 bad_chunk_magic;
    inputfile.Read(&bad_chunk_magic, sizeof(bad_chunk_magic));
    OrkAssert(bad_chunk_magic == good_chunk_magic);
    OrkHeapCheck();
    ///////////////////////////
    inputfile.Read(&mistrtablen, sizeof(mistrtablen));
    char* pst = new char[mistrtablen];
    inputfile.Read(pst, mistrtablen);
    mpstrtab = pst;
    OrkHeapCheck();
    ///////////////////////////
    int32_t ifiletype = 0;
    inputfile.Read(&ifiletype, sizeof(ifiletype));
    const char* pthistype = mpstrtab + ifiletype;
    OrkAssert(0 == strcmp(pthistype, ptype));
    OrkHeapCheck();
    ///////////////////////////
    int32_t inumchunks = 0;
    inputfile.Read(&inumchunks, sizeof(inumchunks));
    mbOk = true;
    ///////////////////////////
    for (int ic = 0; ic < inumchunks; ic++) {
      int32_t ichunkid, ioffset, ichunklen;
      inputfile.Read(&ichunkid, sizeof(ichunkid));
      inputfile.Read(&ioffset, sizeof(ioffset));
      inputfile.Read(&ichunklen, sizeof(ichunklen));
      PoolString psname   = AddPooledString(GetString(ichunkid));
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
      OrkHeapCheck();
    }
    ///////////////////////////
    for (int ic = 0; ic < inumchunks; ic++) {
      if (mStreamBank[ic].GetLength()) {
        inputfile.Read(mStreamBank[ic].GetDataAt(0), mStreamBank[ic].GetLength());
      }
    }
    ///////////////////////////
  }
}
////////////////////////////////////////////////////////////////////////////////////
InputStream* Reader::GetStream(const char* streamname) {
  PoolString ps                         = ork::AddPooledString(streamname);
  typename StreamLut::const_iterator it = mInputStreams.find(ps);
  return (it == mInputStreams.end()) ? 0 : it->second;
}
////////////////////////////////////////////////////////////////////////////////////
const char* Reader::GetString(int index) const {
  OrkAssert(index < mistrtablen);
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
  return nstream;
}

////////////////////////////////////////////////////////////////////////////////////

int Writer::stringIndex(const char* pstr) {
  return _stringblock.AddString(pstr).Index();
}

////////////////////////////////////////////////////////////////////////////////////

void Writer::WriteToFile(const file::Path& outpath) {
  ork::File outputfile(outpath, ork::EFM_WRITE);

  Char4 chunk_magic("chkf");
  // swapbytes_dynamic( chunk_magic );

  ////////////////////////
  outputfile.Write(&chunk_magic, sizeof(chunk_magic));
  ////////////////////////

  OutputStream StringBlockStream;
  StringBlockStream.Write((const unsigned char*)_stringblock.data(), _stringblock.size());
  int istringblksize = StringBlockStream.GetSize();

  ////////////////////////
  swapbytes_dynamic(istringblksize);
  outputfile.Write(&istringblksize, sizeof(istringblksize));
  outputfile.Write(StringBlockStream.GetData(), StringBlockStream.GetSize());
  ////////////////////////
  swapbytes_dynamic(_filetype);
  outputfile.Write(&_filetype, sizeof(_filetype));
  ////////////////////////
  int inumchunks = (int)mOutputStreams.size();
  swapbytes_dynamic(inumchunks);
  outputfile.Write(&inumchunks, sizeof(inumchunks));
  ////////////////////////

  int ioffset = 0;
  for (orkmap<int, OutputStream*>::const_iterator it = mOutputStreams.begin(); it != mOutputStreams.end(); it++) {
    int ichunkid         = it->first;
    OutputStream* stream = it->second;
    int ichunklen        = stream->GetSize();
    ////////////////////////
    swapbytes_dynamic(ichunkid);
    swapbytes_dynamic(ioffset);
    swapbytes_dynamic(ichunklen);
    outputfile.Write(&ichunkid, sizeof(ichunkid));
    outputfile.Write(&ioffset, sizeof(ioffset));
    outputfile.Write(&ichunklen, sizeof(ichunklen));
    ////////////////////////

    ioffset += ichunklen;
  }

  ////////////////////////
  for (orkmap<int, OutputStream*>::const_iterator it = mOutputStreams.begin(); it != mOutputStreams.end(); it++) {
    int ichunkid         = it->first;
    OutputStream* stream = it->second;
    int ichunklen        = stream->GetSize();
    if (ichunklen && stream->GetData()) {
      outputfile.Write(stream->GetData(), ichunklen);
    }
  }
}
}} // namespace ork::chunkfile
