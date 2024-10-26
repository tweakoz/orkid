////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/file/efileenum.h>

// These are not needed for this header, but remains for compatibility for other include file.h and
// expecting to get these class declarations.
#include <ork/file/fileenv.h>
#include <ork/file/filedev.h>
#include <ork/kernel/datablock.h>
#include <ctype.h>


///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class File;
using file_ptr_t = std::shared_ptr<File>;

class File {

public:
  FileDev* mpDevice;
  file::Path msFileName;
  EFileMode meFileMode;
  size_t miFileLen;
  FileH mHandle;
  bool mbEnableBuffering;
  size_t miPhysicalPos;
  size_t miUserPos; // current position user wants

  static datablock_ptr_t loadDatablock(const file::Path& sFileName);

  File(FileDev* pdev = NULL);
  File(const char* sFileName, EFileMode eMode, FileDev* pdev = NULL);
  File(const file::Path& sFileName, EFileMode eMode, FileDev* pdev = NULL);
  ~File();

  EFileErrCode OpenFile(const file::Path& sFileName, EFileMode eMode);
  EFileErrCode Open();
  EFileErrCode Close();
  EFileErrCode Load(std::vector<uint8_t>& bytes);

  EFileErrCode Read(void* pTo, size_t iSize);
  EFileErrCode Write(const void* pFrom, size_t iSize);
  EFileErrCode SeekFromStart(size_t iOffset);
  EFileErrCode SeekFromCurrent(size_t iOffset);
  EFileErrCode GetLength(size_t& riOffset);

  // Serializer functions
  template <class T> inline File& operator<<(const T& d);
  template <class T> inline File& operator>>(T& d);

  const file::Path& GetFileName(void) {
    return msFileName;
  }

  bool IsOpen(void) const;

  bool Reading(void) {
    return bool(meFileMode & EFM_READ);
  }
  bool Writing(void) {
    return bool(meFileMode & EFM_WRITE);
  }
  bool Ascii(void) {
    return bool(meFileMode & EFM_ASCII);
  }
  bool Appending(void) {
    return bool(meFileMode & EFM_APPEND);
  }
  bool Binary(void) {
    return !Ascii();
  }
  bool Fast(void) {
    return bool(meFileMode & EFM_FAST);
  }

  size_t GetUserPos() const {
    return miUserPos;
  }
  void SetUserPos(size_t ip) {
    miUserPos = ip;
  }

  size_t GetPhysicalPos() const {
    return miPhysicalPos;
  }
  void SetPhysicalPos(size_t ip) {
    miPhysicalPos = ip;
  }

  size_t NumPhysicalRemaining() const {
    return miFileLen - miPhysicalPos;
  }

  bool IsBufferingEnabled() const {
    return mbEnableBuffering;
  }
};

///////////////////////////////////////////////////////////////////////////////

inline EFileErrCode File::Read(void* pTo, size_t iSize) {
  return mpDevice->read(*this, pTo, iSize);
}

///////////////////////////////////////////////////////////////////////////////

inline EFileErrCode File::Write(const void* pFrom, size_t iSize) {
  return mpDevice->write(*this, pFrom, iSize);
}

///////////////////////////////////////////////////////////////////////////////

inline EFileErrCode File::SeekFromStart(size_t iOffset) {
  return mpDevice->seekFromStart(*this, iOffset);
}

///////////////////////////////////////////////////////////////////////////////

inline EFileErrCode File::SeekFromCurrent(size_t iOffset) {
  return mpDevice->seekFromCurrent(*this, iOffset);
}

///////////////////////////////////////////////////////////////////////////////

inline EFileErrCode File::GetLength(size_t& riOffset) {
  return mpDevice->getLength(*this, riOffset);
}

///////////////////////////////////////////////////////////////////////////////

inline EFileErrCode File::Close(void) {
  return mpDevice->closeFile(*this);
}

///////////////////////////////////////////////////////////////////////////////

template <class T> File& File::operator<<(const T& d) {
  OrkAssert(!Reading());

  if (Ascii()) {
    // CStringStream strStream;
    // strStream << d;
    // FileEnv::Write( *this, (void *)strStream.str().c_str(), strStream.str().length() * sizeof( char ) );
  } else
    mpDevice->write(*this, (void*)&d, sizeof(T));

  return *this;
}

///////////////////////////////////////////////////////////////////////////////

template <class T> File& File::operator>>(T& d) {
  OrkAssert(Reading());
  mpDevice->read(*this, (void*)&d, sizeof(T));
  return *this;
}

///////////////////////////////////////////////////////////////////////////////

#define FileReadObj(fil, item) fil.Read((void*)&item, sizeof(item))
#define FileWriteObj(fil, item) fil.Write((void*)&item, sizeof(item))
#define FileWriteStr(fil, item) fil.Write((void*)item, (strlen(item) + 1))

///////////////////////////////////////////////////////////////////////////////
// functors for transform

struct dos2unixpathsep {
  char operator()(char c) {
    if (c == '\\')
      c = '/';
    return c;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct unix2dospathsep {
  char operator()(char c) {
    if (c == '/')
      c = '\\';
    return c;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct pathtolower {
  char operator()(char c) {
#if defined(WII)
    c = char(std::tolower(c));
#else
    c = char(tolower(c));
#endif
    return c;
  }
};

///////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
#define NativePathSep '\\'
#elif defined(_LINUX) || defined(_OSX)
#define NativePathSep '/'
#endif

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
