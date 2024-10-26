////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/kernel/string/string.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

datablock_ptr_t File::loadDatablock(const file::Path& sFileName){
  File file(sFileName, EFM_READ);
  std::vector<uint8_t> bytes;
  file.Load(bytes);
  datablock_ptr_t datablock = std::make_shared<DataBlock>(bytes.data(), bytes.size());
  return datablock;
}

File::File(FileDev* pdev)
    : mpDevice(pdev)
    , msFileName("NoFile")
    , meFileMode(EFM_READ)
    , miFileLen(0)
    , mHandle(0)
    , mbEnableBuffering(true)
    , miPhysicalPos(0)
    , miUserPos(0) {
  if (NULL == mpDevice)
    mpDevice = FileEnv::GetRef().GetDefaultDevice();
}

///////////////////////////////////////////////////////////////////////////////

File::File(const char* sFileName, EFileMode eMode, FileDev* pdev)
    : mpDevice(pdev)
    , msFileName(sFileName)
    , meFileMode(eMode)
    , miFileLen(0)
    , mHandle(0)
    , mbEnableBuffering(true)
    , miPhysicalPos(0)
    , miUserPos(0){
  if (NULL == mpDevice)
    mpDevice = FileEnv::GetRef().GetDeviceForUrl(msFileName);

  OpenFile(sFileName, eMode);
}

File::File(const file::Path& sFileName, EFileMode eMode, FileDev* pdev)
    : mpDevice(pdev)
    , msFileName(sFileName)
    , meFileMode(eMode)
    , miFileLen(0)
    , mHandle(0)
    , mbEnableBuffering(true)
    , miPhysicalPos(0)
    , miUserPos(0){
  if (NULL == mpDevice)
    mpDevice = FileEnv::GetRef().GetDeviceForUrl(msFileName);

  OpenFile(sFileName, eMode);
}

File::~File() {
  if (IsOpen())
    Close();
}

bool File::IsOpen() const {
  return mHandle != 0;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode File::Open() {
  float ftime = ork::OldSchool::GetRef().GetLoResRelTime();

  //	std::string filename = ork::CreateFormattedString(
  //		"Time<%f> Opening<%s>\n", ftime, this->msFileName.c_str() );
  //	orkprintf( filename.c_str() );

  return mpDevice->openFile(*this);
}

EFileErrCode File::OpenFile(const file::Path& fname, EFileMode eMode) {
  meFileMode = eMode;
  msFileName = fname;
  return Open();
}

EFileErrCode File::Load(std::vector<uint8_t>& bytes) {
  OrkAssert(meFileMode & EFM_READ);
  OrkAssert(IsOpen());

  EFileErrCode result = Open();

  if (EFEC_FILE_OK == result) {
    size_t length = 0;
    result        = GetLength(length);
    if (EFEC_FILE_OK == result) {
      bytes.resize(length);
      result = Read(bytes.data(), length);
    }
    else{
      bytes.clear();
    }
  }

  Close();

  return result;
}

} // namespace ork
