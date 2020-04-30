///////////////////////////////////////////////////////////////////////////////
// Orkid2
// Copyright 1996-2020, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid2/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/file/filedev.h>
#include <ork/file/filestd.h>

//#include <ork/util/crc64.h>
#include <ork/util/crc.h>
#include <ork/kernel/string/string.h>
#include <ork/util/endian.h>

#include <stdio.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
FileDevContext::FileDevContext()
    : _absolute_basepath("")
    , mbPrependFilesystemBase(false)
    , mpFileDevice(NULL) {
}
FileDevContext::FileDevContext(const FileDevContext& oth)
    : _absolute_basepath(oth._absolute_basepath)
    , mbPrependFilesystemBase(oth.mbPrependFilesystemBase)
    , mpFileDevice(oth.mpFileDevice)
    , mPathConverters(oth.mPathConverters) {
}
void FileDevContext::setFilesystemBaseAbs(const file::Path& base) {
  _absolute_basepath = base;
  // printf("_absolute_basepath<%s>\n", base.c_str());
  // printf("_absolute_basepath<%s>\n", _absolute_basepath.c_str());
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::Read(File& rFile, void* pTo, size_t iSize) {
  // There should be no assert here since Reading a non-open file is an error condition.
  if (!rFile.IsOpen()) {
    orkprintf("FileError(%s): EFEC_FILE_NOT_OPEN\n", rFile.msFileName.c_str());
    return ork::EFEC_FILE_NOT_OPEN;
  }

  if (!rFile.Reading()) {
    return ork::EFEC_FILE_INVALID_MODE;
  }

  // OrkAssert(pTo && iSize > 0);

  if (0 == pTo) {
    orkprintf("FileError(%s): EFEC_FILE_INVALID_ADDRESS\n", rFile.msFileName.c_str());
    return EFEC_FILE_INVALID_ADDRESS;
  }

  if (iSize < 1) {
    orkprintf("FileError(%s): EFEC_FILE_INVALID_SIZE\n", rFile.msFileName.c_str());
    return EFEC_FILE_INVALID_SIZE;
  }

  OrkAssert((rFile.GetUserPos() + iSize) <= rFile.miFileLen);
  if ((rFile.GetUserPos() + iSize) > rFile.miFileLen) {
    orkprintf("FileError(%s): EFEC_FILE_INVALID_SIZE (seek past file bounds)\n", rFile.msFileName.c_str());
    return ork::EFEC_FILE_INVALID_SIZE;
  }

  ///////////////////////////////////////////
  size_t numactuallyread = 0;
  auto ecode             = DoRead(rFile, pTo, iSize, numactuallyread);
  OrkAssert(numactuallyread == iSize);

  size_t iuserpos = rFile.GetUserPos();

  rFile.SetUserPos(iuserpos + iSize);

  ///////////////////////////////////////////

  return ecode;
} // namespace ork

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::OpenFile(File& rFile) {
  rFile.SetPhysicalPos(0);
  rFile.SetUserPos(0);
  if (mWatcher)
    mWatcher->BeginFile(&rFile);
  if (CheckFileDevCaps(rFile) == EFEC_FILE_UNSUPPORTED) {
    return EFEC_FILE_UNSUPPORTED;
  }
  return DoOpenFile(rFile);
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::CloseFile(File& rFile) {
  if (mWatcher)
    mWatcher->EndFile(&rFile);

  if (!rFile.IsOpen()) {
    return ork::EFEC_FILE_NOT_OPEN;
  }
  EFileErrCode ecode = DoCloseFile(rFile);
  rFile.mHandle      = 0;
  //	OrkHeapCheck();
  return ecode;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::SeekFromStart(File& rFile, size_t iTo) {
  if (!rFile.IsOpen()) {
    return ork::EFEC_FILE_NOT_OPEN;
  }
  EFileErrCode ecode = ork::EFEC_FILE_OK; // DoSeekFromStart( rFile, iTo );
  DoSeekFromStart(rFile, iTo);
  rFile.SetUserPos(iTo);
  return ecode;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::SeekFromCurrent(File& rFile, size_t iOffset) {
  if (!rFile.IsOpen()) {
    return ork::EFEC_FILE_NOT_OPEN;
  }
  EFileErrCode ecode = ork::EFEC_FILE_OK; // DoSeekFromStart( rFile, iTo );
  // EFileErrCode ecode = DoSeekFromCurrent( rFile, iOffset );
  size_t inewpos = rFile.GetUserPos() + iOffset;
  DoSeekFromCurrent(rFile, iOffset);
  rFile.SetUserPos(inewpos);
  return ecode;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::GetLength(File& rFile, size_t& riLength) {
  EFileErrCode ecode = EFEC_FILE_OK;
  if (rFile.IsOpen()) {
    riLength = rFile.miFileLen;
  } else {
    ecode = DoGetLength(rFile, riLength);
    rFile.SetPhysicalPos(0);
    rFile.SetUserPos(0);
  }
  return ecode;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::CheckFileDevCaps(File& rFile) {
  if (rFile.Appending() || rFile.Writing()) {
    bool canw = CanWrite();
    OrkAssert(canw);
    if (!canw)
      return EFEC_FILE_INVALID_CAPS;
  }
  if (rFile.Reading()) {
    bool canr = CanRead();
    OrkAssert(canr);
    if (!canr)
      return EFEC_FILE_INVALID_CAPS;
  }
  return EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

FileDev::FileDev(file::Path::NameType devicename, file::Path fsbase, U32 devcaps)
    : msDeviceName(devicename)
    , muDeviceCaps(devcaps)
    , mFileDevContextStackDepth(0)
    , mWatcher(0) {
  SetFileSystemBaseAbs(fsbase.c_str());
  SetPrependFilesystemBase(true);
}

} // namespace ork
