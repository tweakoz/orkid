////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/file/efileenum.h>
#include <ork/file/path.h>
#include <ork/file/filedevcontext.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class FileProgressWatcher {
  bool mbEnable;

public:
  FileProgressWatcher()
      : mbEnable(false) {
  }

  void SetEnable(bool bena) {
    mbEnable = bena;
  }
  bool GetEnable() const {
    return mbEnable;
  }

  virtual void BeginBlock()                             = 0;
  virtual void EndBlock()                               = 0;
  virtual void BeginFile(const File* file)              = 0;
  virtual void EndFile(const File* file)                = 0;
  virtual void Reading(const File* file, size_t ibytes) = 0;
};

///////////////////////////////////////////////////////////////////////////////

class FileDev {

public:

  static const int kBUFFERMASK    = 0xffffffe0;
  static const int kBUFFERALIGN   = 32;
  static const int kBUFFERALIGNM1 = (kBUFFERALIGN - 1);
  static const int kSEEKALIGN     = 4;

  EFileErrCode checkFileDevCaps(File& rFile);

  inline bool canRead(void) {
    return (bool)(muDeviceCaps & EFDF_CAN_READ);
  }
  inline bool canWrite(void) {
    return (bool)(muDeviceCaps & EFDF_CAN_WRITE);
  }
  inline void setPrependFilesystemBase(bool setting) {
    mbPrependFSBase = setting;
  }
  inline bool getPrependFilesystemBase(void) const {
    return mbPrependFSBase;
  }
  inline void setFileSystemBaseAbs(const file::Path::NameType& Base) {
    mFsBaseAbs = Base;
  }
  inline void setFileSystemBaseRel(const file::Path::NameType& Base) {
    mFsBaseRel = Base;
  }
  inline const file::Path::NameType& getFilesystemBaseAbs(void) const {
    return mFsBaseAbs;
  }
  inline const file::Path::NameType& getFilesystemBaseRel(void) const {
    return mFsBaseRel;
  }

  //////////////////////////////////////////

  EFileErrCode read(File& rFile, void* pTo, size_t iSize);
  EFileErrCode openFile(File& rFile);
  EFileErrCode closeFile(File& rFile);
  EFileErrCode seekFromStart(File& rFile, size_t iTo);
  EFileErrCode seekFromCurrent(File& rFile, size_t iOffset);
  EFileErrCode getLength(File& rFile, size_t& riLength);

  //////////////////////////////////////////
  // Abstract Interface

  virtual EFileErrCode write(File& rFile, const void* pFrom, size_t iSize)        = 0;
  virtual EFileErrCode getCurrentDirectory(file::Path::NameType& directory)       = 0;
  virtual EFileErrCode setCurrentDirectory(const file::Path::NameType& directory) = 0;

  virtual bool doesFileExist(const file::Path& filespec)      = 0;
  virtual bool doesDirectoryExist(const file::Path& filespec) = 0;
  virtual bool isFileWritable(const file::Path& filespec)     = 0;


  void setWatcher(fileprogresswatcher_ptr_t watcher) {
    _watcher = watcher;
  }
  fileprogresswatcher_ptr_t getWatcher() const {
    return _watcher;
  }

protected:

  FileDev(file::Path::NameType devicename, file::Path fsbase, U32 devcaps);

  virtual ~FileDev() {
  }

  virtual EFileErrCode _doRead(File& rFile, void* pTo, size_t iSize, size_t& iactualread) = 0;
  virtual EFileErrCode _doOpenFile(File& rFile)                                           = 0;
  virtual EFileErrCode _doCloseFile(File& rFile)                                          = 0;
  virtual EFileErrCode _doSeekFromStart(File& rFile, size_t iTo)                          = 0;
  virtual EFileErrCode _doSeekFromCurrent(File& rFile, size_t iOffset)                    = 0;
  virtual EFileErrCode _doGetLength(File& rFile, size_t& riLength)                        = 0;

  static const int kFileDevContextStackMax = 4;

  file::Path::NameType msDeviceName;
  U32 muDeviceCaps;
  int mFileDevContextStackDepth;
  fileprogresswatcher_ptr_t _watcher;

  FileDevContext mFileDevContext[kFileDevContextStackMax];

  file::Path::NameType mFsBaseAbs;
  file::Path::NameType mFsBaseRel;
  bool mbPrependFSBase;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
