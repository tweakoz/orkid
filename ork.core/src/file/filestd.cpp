////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/string/string.h>

#include <ork/file/filestd.h>

using ork::EFileErrCode;
using ork::File;
using ork::FileDev;
using ork::FileH;

#include <errno.h>
#include <algorithm>

#include <unistd.h>
#include <glob.h>
#include <fts.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

///////////////////////////////////////////////////////////////////////////////

FileDevStd::FileDevStd(void)
    : FileDev("StandardIO", "./", (ork::EFDF_CAN_READ | ork::EFDF_CAN_WRITE)) {
#if defined(_OSX)
  char buffer[256];

  CFBundleRef Bundle  = CFBundleGetMainBundle();
  CFURLRef URL        = CFBundleCopyBundleURL(Bundle);
  CFStringRef PathStr = CFURLCopyFileSystemPath(URL, kCFURLPOSIXPathStyle);
  Boolean bval        = CFStringGetCString(PathStr, buffer, 256, kCFStringEncodingASCII);

  orkprintf("cwd %s\n", buffer);

  chdir(buffer);
#endif
}

///////////////////////////////////////////////////////////////////////////////

static int GetLengthFromToc(const ork::file::Path& fname) {
  return -1;
}

///////////////////////////////////////////////////////////////////////////////

int ifilecount = 0;

EFileErrCode FileDevStd::DoOpenFile(File& rFile) {
  ifilecount++;

  const ork::file::Path& fname = rFile.GetFileName();

  bool breading = (false == rFile.Writing());
  bool bwriting = rFile.Writing();

  //    printf( "OpenFile<%s> Read<%d> Write<%d>\n", fname.c_str(), int(breading), int(bwriting) );

  /////////////////////////////////////////////////////////////////////////////
  // compute the filename

  ork::file::Path fullfname = fname.ToAbsolute();

  if (breading) {
    bool bexists = this->DoesFileExist(fname);

    if (false == bexists) {
      return ork::EFEC_FILE_DOES_NOT_EXIST;
    }
    // reading and it exists
  }

  /////////////////////////////////////////////////////////////////////////////
  const char* strMode = 0;
  if (rFile.Appending())
    strMode = "at+";
  else if (rFile.Writing())
    strMode = "wb";
  else if (rFile.Reading())
    strMode = "rb";
  // if ( !rFile.Ascii() ) {	strMode += "b"; }

  rFile.mHandle = reinterpret_cast<FileH>(fopen(fullfname.c_str(), strMode));

  if (rFile.mHandle) {
    FILE* pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
    fseek(pFILE, 0, SEEK_END);
    rFile.miFileLen = ftell(pFILE);
    fseek(pFILE, 0, SEEK_SET);

    return ork::EFEC_FILE_OK;
  } else
    return ork::EFEC_FILE_DOES_NOT_EXIST;
}

///////////////////////////////////////////////////////////////////////////////

ork::EFileErrCode FileDevStd::DoCloseFile(File& rFile) {
  ifilecount--;
  FILE* pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
  if (pFILE)
    fclose(pFILE);
  return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

ork::EFileErrCode FileDevStd::DoRead(File& rFile, void* pTo, size_t icount, size_t& iactualread) {
  FILE* pFILE  = reinterpret_cast<FILE*>(rFile.mHandle);
  int iphyspos = ftell(pFILE);
  if (rFile.GetPhysicalPos() != iphyspos) {
    printf("rFile.GetPhysicalPos()<%d> iphyspos<%d>\n", int(rFile.GetPhysicalPos()), iphyspos);
  }
  OrkAssert(rFile.GetPhysicalPos() == iphyspos);
  /////////////////////////////////////////////////////////
  iactualread = 0;
  /////////////////////////////////////////////////////////
  size_t ifilelen = 0;
  rFile.GetLength(ifilelen);
  size_t iphysleft = (ifilelen - iphyspos);
  /////////////////////////////////////////////////////////
  if (icount <= iphysleft) {
    iactualread = fread(pTo, 1, icount, pFILE);
  }
  /////////////////////////////////////////////////////////
  else // read past end of file, so terminate read buffer with 0's
  {
    iactualread    = fread(pTo, 1, iphysleft, pFILE);
    intptr_t itail = icount - iphysleft;
    memset(((char*)pTo) + iphysleft, 0, itail);
  }

  if (mWatcher)
    mWatcher->Reading(&rFile, iactualread);

  /////////////////////////////////////////////////////////
  // orkprintf( "PostDoRead<%d> tell<%d> phys<%d>\n", iSize, ipos, rFile.GetPhysicalPos() );
  /////////////////////////////////////////////////////////
  if (iactualread > 0) {
    size_t ioldphys = rFile.GetPhysicalPos();
    rFile.SetPhysicalPos(ioldphys + iactualread);
  } else {
    perror("FileDevStd::DoReadFailed: ");
    return ork::EFEC_FILE_UNKNOWN;
  }
  /////////////////////////////////////////////////////////
  return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

ork::EFileErrCode FileDevStd::Write(File& rFile, const void* pFrom, size_t iSize) {
  if (!rFile.IsOpen()) {
    return ork::EFEC_FILE_NOT_OPEN;
  }

  if (!rFile.Writing() && !rFile.Appending()) {
    return ork::EFEC_FILE_INVALID_MODE;
  }

  ///////////////////////////////
  ///////////////////////////////

  FILE* pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
  OrkAssert(pFILE);
  fwrite(pFrom, 1, iSize, pFILE);
  fflush(pFILE);

  size_t inewpos = rFile.GetPhysicalPos() + iSize;
  rFile.SetPhysicalPos(inewpos);
  rFile.SetUserPos(inewpos);

  return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDevStd::DoSeekFromStart(File& rFile, size_t iTo) {
  FILE* pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
  OrkAssert(pFILE);
  fseek(pFILE, iTo, SEEK_SET);
  rFile.SetPhysicalPos(iTo);
  return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

ork::EFileErrCode FileDevStd::DoSeekFromCurrent(File& rFile, size_t iOffset) {
  FILE* pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
  OrkAssert(pFILE);
  fseek(pFILE, iOffset, SEEK_CUR);
  rFile.SetPhysicalPos(rFile.GetPhysicalPos() + iOffset);
  return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDevStd::DoGetLength(File& rFile, size_t& riLen) {
  riLen = 0;

  const ork::file::Path& fname  = rFile.GetFileName();
  file::Path::SmallNameType url = fname.GetUrlBase();
  auto ctx                      = ork::FileEnv::contextForUriProto(url.c_str());
  ///////////////////////////////

  FILE* pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
  if (pFILE) {
    fseek(pFILE, 0, SEEK_END);
    riLen = ftell(pFILE);
    fseek(pFILE, 0, SEEK_SET);
  } else {
    return EFEC_FILE_NOT_OPEN;
  }
  ///////////////////////////////
  return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////
// GetCurrentDirectory/SetCurrentDirectory
// these are currently broken , they "should" be the current directory of the device,
// but what they are is the current directory of the process
///////////////////////////////////////////////

EFileErrCode FileDevStd::GetCurrentDirectory(file::Path::NameType& directory) {
  file::Path::NameType outspec;
  // Not implemented for this platform!
  OrkAssert(false);
  return EFEC_FILE_UNSUPPORTED;
  directory = outspec;
  return ork::EFEC_FILE_OK;
}

EFileErrCode FileDevStd::SetCurrentDirectory(const file::Path::NameType& directory) {
  OrkAssert(false);
  return ork::EFEC_FILE_UNKNOWN;
}

///////////////////////////////////////////////////////////////////////////////

bool FileDevStd::DoesFileExist(const file::Path& filespec) {
  auto url = filespec.GetUrlBase();
  auto ctx = ork::FileEnv::contextForUriProto(url.c_str());

  file::Path pathspec(filespec.c_str());
  file::Path abspath = pathspec.ToAbsolute();
  const char* pFn    = abspath.c_str();

  // printf("FileDevStd<%p> DoesFileExist<%s> url<%s> Abs<%s>\n", this, filespec.c_str(), url.c_str(), abspath.c_str());

  FILE* fin = fopen(abspath.c_str(), "rb");
  bool bv   = (fin == 0) ? false : true;
  if (fin != 0)
    fclose(fin);

  if (false == bv) {
    ork::FileEnvDir* pdir = ork::FileEnv::GetRef().OpenDir(filespec.c_str());
    bv                    = (pdir != 0);
    if (pdir)
      ork::FileEnv::GetRef().CloseDir(pdir);
  }
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool FileDevStd::IsFileWritable(const file::Path& filespec) {
  file::Path absol = filespec.ToAbsolute();

  FILE* fin = fopen(absol.c_str(), "a+");
  bool bv   = (fin == 0) ? false : true;
  if (fin != 0)
    fclose(fin);

  return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool FileDevStd::DoesDirectoryExist(const file::Path& filespec) {
  file::Path absol = filespec.ToAbsolute();

  const char* pFn = absol.c_str();

  bool bv = false;
  if (bv) {
  }
  return bv;
}

///////////////////////////////////////////////////////////////////////////////

ork::FileStampH ork::FileEnv::EncodeFileStamp(int year, int month, int day, int hour, int minute, int second) {
  OrkAssert(year >= 2000);
  OrkAssert(year <= 2063);
  OrkAssert(month >= 0);
  OrkAssert(month <= 12);
  OrkAssert(day >= 0);
  OrkAssert(day <= 31);
  OrkAssert(hour >= 0);
  OrkAssert(hour <= 24);
  OrkAssert(minute >= 0);
  OrkAssert(minute <= 60);
  OrkAssert(second >= 0);
  OrkAssert(second <= 60);
  year -= 2000;
  U32 Result = (year << 26) + (month << 22) + (day << 17) + (hour << 12) + (minute << 6) + second;
  return static_cast<FileStampH>(Result);
}

///////////////////////////////////////////////////////////////////////////////

void ork::FileEnv::DecodeFileStamp(ork::FileStampH stamp, int& year, int& month, int& day, int& hour, int& minute, int& second) {
  U32 UStamp = static_cast<U32>(stamp);
  year       = ((stamp & 0xfc000000) >> 26) + 2000;
  month      = ((stamp & 0x03c00000) >> 22);
  day        = ((stamp & 0x003e0000) >> 17);
  hour       = ((stamp & 0x0001f000) >> 12);
  minute     = ((stamp & 0x00000fc0) >> 6);
  second     = (stamp & 0x0000003f);
}

ork::FileStampH ork::FileEnv::GetFileStamp(const file::Path::NameType& filespec) {
  ork::FileStampH Result = 0x00000000;
  return Result;
}

///////////////////////////////////////////////////////////////////////////////

ork::FileEnvDir* ork::FileEnv::OpenDir(const char* name) {
  ork::FileEnvDir* dir = 0;
  return dir;
}

int ork::FileEnv::CloseDir(ork::FileEnvDir* dir) {
  int result = -1;
  return result;
}

file::Path::NameType ork::FileEnv::ReadDir(ork::FileEnvDir* dir) {
  file::Path::NameType result;
  return result;
}

void ork::FileEnv::RewindDir(ork::FileEnvDir* dir) {
}

///////////////////////////////////////////////////////////////////////////////

int unix2winpathsep(int c) {
  int rval = c;
  if (rval == '/')
    rval = '\\';
  return rval;
}
int win2unixpathsep(int c) {
  int rval = c;
  if (rval == '\\')
    rval = '/';
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

int wildcmp(const char* wild, const char* string) {
  const char *cp = 0, *mp = 0;
  while ((*string) && (*wild != '*')) {
    if ((*wild != *string) && (*wild != '?')) {
      return 0;
    }
    wild++;
    string++;
  }
  while (*string) {
    if (*wild == '*') {
      if (!*++wild) {
        return 1;
      }
      mp = wild;
      cp = string + 1;
    } else if ((*wild == *string) || (*wild == '?')) {
      wild++;
      string++;
    } else {
      wild   = mp;
      string = cp++;
    }
  }
  while (*wild == '*') {
    wild++;
  }
  return !*wild;
}

///////////////////////////////////////////////////////////////////////////////

orkset<file::Path::NameType>
ork::FileEnv::filespec_search_sorted(const file::Path::NameType& wildcards, const ork::file::Path& initdir) {
  orkvector<file::Path::NameType> files = filespec_search(wildcards, initdir);
  orkset<file::Path::NameType> rval;

  size_t inumf = files.size();

  for (size_t i = 0; i < inumf; i++) {
    rval.insert(files[i]);
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

orkvector<file::Path::NameType>
ork::FileEnv::filespec_search(const file::Path::NameType& wildcards, const ork::file::Path& initdir) {
  orkvector<file::Path::NameType> rval;

  file::Path::NameType _wildcards = wildcards;
  if (_wildcards == (file::Path::NameType) "")
    _wildcards = (file::Path::NameType) "*";

  const char* path    = initdir.ToAbsolute(ork::file::Path::EPATHTYPE_POSIX).c_str();
  char* const paths[] = {(char* const)path, 0};

  // printf( "path<%s>\n", path );

  FTS* tree = fts_open(&paths[0], FTS_NOCHDIR, 0);
  if (!tree) {
    perror("fts_open");
    OrkAssert(false);
    return rval;
  }

  FTSENT* node;
  while ((node = fts_read(tree))) {
    if (node->fts_level > 0 && node->fts_name[0] == '.')
      fts_set(tree, node, FTS_SKIP);
    else if (node->fts_info & FTS_F) {

      int match = wildcmp(_wildcards.c_str(), node->fts_name);
      if (match) {
        file::Path::NameType fullname = node->fts_accpath;
        rval.push_back(fullname);
        // orkprintf( "file found <%s>\n", fullname.c_str() );
      }

#if 0
			printf("got file named %s at depth %d, "
			"accessible via %s from the current directory "
			"or via %s from the original starting directory\n",
			node->fts_name, node->fts_level,
			node->fts_accpath, node->fts_path);
#endif

      /* if fts_open is not given FTS_NOCHDIR,
       * fts may change the program's current working directory */
    }
  }
  if (errno) {
    perror("fts_read");
    OrkAssert(false);
    return rval;
  }

  if (fts_close(tree)) {
    perror("fts_close");
    OrkAssert(false);
    return rval;
  }

  return rval;
}

std::string
getfilename(const std::string& filterdesc, const std::string& filter, const std::string& title, const std::string& initdir) {
  return std::string("");
}

} // namespace ork
