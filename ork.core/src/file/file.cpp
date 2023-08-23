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

strfilereadresult_ptr_t File::readAsString(const file::Path& input_path){

  strfilereadresult_ptr_t rval = nullptr;

  auto input_file = std::make_shared<File>();
  auto ecode = input_file->OpenFile(input_path, EFM_READ);
  if( ecode == EFEC_FILE_OK ){
    size_t len = 0;
    input_file->GetLength(len);
    rval = std::make_shared<StringFileReadResult>();
    if(len>0){
      rval->_data.resize(len+1);
      input_file->Read( (void*)rval->_data.data(), len );
      rval->_data[len] = 0;
    }
  }
  return rval;
}

binfilereadresult_ptr_t File::readAsBinary(const file::Path& input_path){

  binfilereadresult_ptr_t rval = nullptr;

  auto input_file = std::make_shared<File>();
  auto ecode = input_file->OpenFile(input_path, EFM_READ);
  if( ecode == EFEC_FILE_OK ){
    size_t len = 0;
    input_file->GetLength(len);
    rval = std::make_shared<BinaryFileReadResult>();
    if(len>0){
      rval->_data.resize(len);
      input_file->Read( (void*)rval->_data.data(), len );
    }
  }
  return rval;
}

bool File::writeString(const file::Path& output_path, std::string data){
  auto output_file = std::make_shared<File>();
  auto ecode = output_file->OpenFile(output_path, EFM_WRITE);
  OrkAssert( ecode == EFEC_FILE_OK );

  EFileErrCode OK = output_file->Write( (void*)data.data(), data.length() );
  OrkAssert( OK == EFEC_FILE_OK );
  output_file->Close();
  return true;
}
bool File::writeBinary(const file::Path& input_path, std::vector<uint8_t> data){
  auto output_file = std::make_shared<File>();
  auto ecode = output_file->OpenFile(input_path, EFM_WRITE);
  OrkAssert( ecode == EFEC_FILE_OK );

  EFileErrCode OK = output_file->Write( (void*)data.data(), data.size() );
  OrkAssert( OK == EFEC_FILE_OK );
  return true;
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

EFileErrCode File::Load(void** filebuffer, size_t& size) {
  OrkAssert(meFileMode & EFM_READ);
  OrkAssert(IsOpen());

  EFileErrCode result = Open();

  if (EFEC_FILE_OK == result) {
    size_t length = 0;
    result        = GetLength(length);
    if (EFEC_FILE_OK == result) {
      if (*filebuffer) {
        if (size > length)
          size = length;
        result = Read(*filebuffer, size);
        if (EFEC_FILE_OK != result)
          size = 0;
      } else {
        size             = length;
        char* charBuffer = new char[size];
        *filebuffer      = charBuffer;
        result           = Read(*filebuffer, size);
        if (EFEC_FILE_OK != result) {
          size = 0;
          OrkAssertI(false, "Stacked allocations cannot be deleted.");
          delete[] charBuffer;
          *filebuffer = NULL;
        }
      }
    }
  }

  Close();

  return result;
}

} // namespace ork
