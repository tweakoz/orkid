////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/file/fileenv.h>
#include <ork/kernel/slashnode.h>
#include <ork/orkstd.h> // For OrkAssert

#include <ork/file/filestd.h>
#include <unistd.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
namespace file {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

file::Path::NameType GetStartupDirectory() {
  return GetCurDir() + file::Path::NameType("/");
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode SetCurDir(const file::Path::NameType& inspec) {
  return EFEC_FILE_UNSUPPORTED; // FileEnv::GetRef().mpDefaultDevice->SetCurrentDirectory(inspec);
}

///////////////////////////////////////////////////////////////////////////////

file::Path::NameType GetCurDir() // the curdir of the process, not the FileDevice
{
  file::Path::NameType outspec;
  char cwdbuf[4096];
  const char* cwdr = getcwd(cwdbuf, sizeof(cwdbuf));
  OrkAssert(cwdr != 0);
  // printf( "cwdbuf<%s>\n", cwdbuf );
  outspec = cwdr;
  // printf( "aa\n");
  file::Path mypath(outspec.c_str());
  // printf( "ab\n");
  // std::transform( outspec.begin(), outspec.end(), outspec.begin(), dos2unixpathsep() );
  mypath = mypath.c_str();
  return outspec;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
} // namespace file
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FileEnv::FileEnv()
    : NoRttiSingleton<FileEnv>()
    , mpDefaultDevice(NULL) {
  FileDev* fileEnvironment = new FileDevStd;
  SetDefaultDevice(fileEnvironment);
}

///////////////////////////////////////////////////////////////////////////////

FileDev* FileEnv::GetDeviceForUrl(const file::Path& fileName) const {
  auto& env    = FileEnv::GetRef();
  auto urlbase = env.UrlNameToBase(fileName.GetUrlBase().c_str()).c_str();

  auto it = env.RefUrlRegistry().find(urlbase);
  if (it != env.RefUrlRegistry().end())
    if (it->second->GetFileDevice())
      return it->second->GetFileDevice();

  return GetRef().mpDefaultDevice;
}

///////////////////////////////////////////////////////////////////////////////

bool FileEnv::CanRead(void) {
  return GetRef().mpDefaultDevice->CanRead();
}
bool FileEnv::CanWrite(void) {
  return GetRef().mpDefaultDevice->CanWrite();
}
bool FileEnv::CanReadAsync(void) {
  return GetRef().mpDefaultDevice->CanReadAsync();
}

///////////////////////////////////////////////////////////////////////////////

void FileEnv::SetFilesystemBase(file::Path::NameType FSBase) {
  GetRef().mpDefaultDevice->SetFileSystemBaseAbs(FSBase);
}

///////////////////////////////////////////////////////////////////////////////

const file::Path::NameType& FileEnv::GetFilesystemBase(void) {
  return GetRef().mpDefaultDevice->GetFilesystemBaseAbs();
}

///////////////////////////////////////////////////////////////////////////////

const_filedevctxptr_t FileEnv::UrlBaseToContext(const file::Path::SmallNameType& UrlName) {
  static filedevctxptr_t default_ctx = std::make_shared<FileDevContext>();

  file::Path::SmallNameType strip_name;
  strip_name.replace(UrlName.c_str(), "://", "");

  auto& the_map = GetRef()._fileDevContextMap;
  auto it       = the_map.find(strip_name);
  if (it != the_map.end()) {
    return it->second;
  }
  return default_ctx;
}

///////////////////////////////////////////////////////////////////////////////

file::Path::SmallNameType FileEnv::UrlNameToBase(const file::Path::NameType& UrlName) {
  file::Path::SmallNameType urlbase              = "";
  file::Path::NameType::size_type find_url_colon = UrlName.cue_to_char(':', 0);
  if (UrlName.npos != find_url_colon) {
    if (int(UrlName.size()) > (find_url_colon + 2)) {
      if ((UrlName[find_url_colon + 1] == '/') || (UrlName[find_url_colon + 2] == '/')) {
        urlbase = UrlName.substr(0, find_url_colon).c_str();
      }
    }
  }

  return urlbase;
}

///////////////////////////////////////////////////////////////////////////////

file::Path::NameType FileEnv::UrlNameToPath(const file::Path::NameType& UrlName) {
  file::Path::NameType path                      = "";
  file::Path::NameType::size_type find_url_colon = UrlName.cue_to_char(':', 0);
  if (UrlName.npos != find_url_colon) {
    if (int(UrlName.size()) > (find_url_colon + 3)) {
      if ((UrlName[find_url_colon + 1] == '/') || (UrlName[find_url_colon + 2] == '/')) {
        file::Path::NameType urlbase = UrlName.substr(0, find_url_colon);

        file::Path::SmallNameType urlp = urlbase.c_str();

        if (OldStlSchoolIsInMap(GetRef()._fileDevContextMap, urlp)) {
          file::Path::NameType::size_type ipathbase = find_url_colon + 3;
          path                                      = UrlName.substr(ipathbase, UrlName.size() - ipathbase).c_str();
        }
      }
    }
  }

  if (path.length() == 0) {
    path = UrlName;
  }

  return path;
}

///////////////////////////////////////////////////////////////////////////////

ork::file::Path FileEnv::GetPathFromUrlExt(
    const file::Path::NameType& UrlName,
    const file::Path::NameType& subfolder,
    const file::Path::SmallNameType& ext) {
  file::Path::NameType Base = UrlNameToBase(UrlName).c_str();
  file::Path::NameType Tail = UrlNameToPath(UrlName);

  if (Tail == (Base + "://"))
    Tail = "";

  auto ctx  = UrlBaseToContext(Base.c_str());
  auto base = ctx->GetFilesystemBaseAbs();

  // printf( "  FileEnv::GetPathFromUrlExt UrlName<%s> subfolder<%s> base<%s>\n", UrlName.c_str(), subfolder.c_str(), base.c_str()
  // );

  file::Path::NameType path;

  if (ctx->GetPrependFilesystemBase()) {
    path = file::Path::NameType(base.c_str()) + subfolder.c_str() + Tail + ext.c_str();
    // printf( "  FileEnv::GetPathFromUrlExt path<%s>\n", path.c_str() );
  } else {

    path = (subfolder + Tail + ext.c_str());
  }

  return ork::file::Path(path.c_str());
}

///////////////////////////////////////////////////////////////////////////////

void FileEnv::registerUrlBase(const file::Path::SmallNameType& UrlName, filedevctxptr_t FileContext) {
  file::Path::SmallNameType urlbase = UrlNameToBase(UrlName.c_str());
  filedevctxmap_t& Map              = GetRef()._fileDevContextMap;
  Map[urlbase]                      = FileContext;
  // FileDevContext& nc = const_cast<FileDevContext&>(FileContext);
  // nc.CreateToc(UrlName);
}

///////////////////////////////////////////////////////////////////////////////

bool FileEnv::PathIsUrlForm(const file::Path& PathName) {
  return PathName.HasUrlBase();
}

///////////////////////////////////////////////////////////////////////////////

bool FileEnv::IsUrlBaseRegistered(const file::Path::SmallNameType& urlBase) {
  return OldStlSchoolIsInMap(GetRef()._fileDevContextMap, urlBase);
}

file::Path::NameType FileEnv::StripUrlFromPath(const file::Path::NameType& urlName) {
  file::Path::NameType urlStr                    = urlName.c_str();
  file::Path::NameType path                      = "";
  file::Path::NameType::size_type find_url_colon = urlStr.cue_to_char(':', 0);
  if (urlStr.npos != find_url_colon) {
    if (int(urlStr.size()) > (find_url_colon + 3)) {
      if ((urlStr[find_url_colon + 1] == '/') || (urlStr[find_url_colon + 2] == '/')) {
        file::Path::NameType::size_type ipathbase = find_url_colon + 3;
        return urlStr.substr(ipathbase, urlStr.size() - ipathbase);
      }
    }
  }

  return urlStr;
}

///////////////////////////////////////////////////////////////////////////////

bool FileEnv::DoesFileExist(const file::Path& filespec) {
  return FileEnv::GetRef().GetDeviceForUrl(filespec)->DoesFileExist(filespec);
}

///////////////////////////////////////////////////////////////////////////////

bool FileEnv::DoesDirectoryExist(const file::Path& filespec) {
  return FileEnv::GetRef().GetDeviceForUrl(filespec)->DoesDirectoryExist(filespec);
}

///////////////////////////////////////////////////////////////////////////////

bool FileEnv::IsFileWritable(const file::Path& filespec) {
  return FileEnv::GetRef().GetDeviceForUrl(filespec)->IsFileWritable(filespec);
}

///////////////////////////////////////////////////////////////////////////////

void FileEnv::SetPrependFilesystemBase(bool setting) {
  FileEnv::GetRef().mpDefaultDevice->SetPrependFilesystemBase(setting);
}

///////////////////////////////////////////////////////////////////////////////

bool FileEnv::GetPrependFilesystemBase(void) {
  return (FileEnv::GetRef().mpDefaultDevice->GetPrependFilesystemBase());
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

bool FileEnv::filespec_isdos(const file::Path::NameType& inspec) {
  bool rval = true;

  file::Path::NameType::size_type len = inspec.size();

  if (inspec[1] == ':') {
    return true;
  }

  // Windows-style network paths
  if ((inspec[0] == '\\') && (inspec[1] == '\\')) {
    return true;
  }

  for (file::Path::NameType::size_type i = 0; i < len; ++i) {
    const char tch = inspec[i];

    if (!IsCharDos(tch)) {
      rval = false;
      break;
    }
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

bool FileEnv::filespec_isunix(const file::Path::NameType& inspec) {
  bool rval = true;

  file::Path::NameType::size_type len = inspec.size();

  for (file::Path::NameType::size_type i = 0; i < len; ++i) {
    const char tch = inspec[i];

    if (!IsCharUnix(tch)) {
      rval = false;
      break;
    }
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

file::Path::NameType FileEnv::filespec_to_extension(const file::Path::NameType& inspec) {
  file::Path::NameType::size_type i1stDot = inspec.find_first_of(".");
  file::Path::NameType::size_type iLstDot = inspec.find_last_of(".");
  // OrkAssert( i1stDot==iLstDot );
  file::Path::NameType::size_type iOldLength = inspec.length();
  file::Path::NameType::size_type iNewLength = (iOldLength - iLstDot) - 1;
  file::Path::NameType outstr                = "";

  if (i1stDot != file::Path::NameType::npos)
    outstr = inspec.substr(iLstDot + 1, iNewLength);

  return outstr;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

file::Path::NameType FileEnv::filespec_no_extension(const file::Path::NameType& inspec) {
  file::Path::NameType::size_type i1stDot = inspec.find_first_of(".");
  file::Path::NameType::size_type iLstDot = inspec.find_last_of(".");
  // OrkAssert( i1stDot==iLstDot );
  file::Path::NameType::size_type iOldLength = inspec.length();
  file::Path::NameType outstr                = inspec.substr(0, iLstDot);
  return outstr;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

file::Path::NameType FileEnv::filespec_strip_base(const file::Path::NameType& inspec, const file::Path::NameType& base) {
  file::Path::NameType outstr = inspec;
  std::transform(outstr.begin(), outstr.end(), outstr.begin(), dos2unixpathsep());
  if (outstr.find(base.c_str()) == 0)
    return outstr.substr(base.length());
  else
    return outstr;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

orkvector<file::Path::NameType> FileEnv::filespec_separate_terms(const file::Path::NameType& inspec) {
  file::Path::NameType _instr = inspec;
  std::transform(_instr.begin(), _instr.end(), _instr.begin(), dos2unixpathsep());
  orkvector<file::Path::NameType> outvec;
  const file::Path::NameType delims("/");
  file::Path::NameType::size_type idx, len, ilen;
  ilen     = _instr.size();
  idx      = _instr.cue_to_char('/', 0);
  int word = 0;
  outvec.clear();
  bool bDone = false;
  file::Path::NameType::size_type Nidx;
  while ((idx < ilen) && (!bDone)) {
    Nidx                         = _instr.cue_to_char('/', int(idx) + 1);
    bool bAnotherSlash           = (Nidx != -1);
    len                          = Nidx - idx;
    file::Path::NameType newword = _instr.substr(idx + 1, len - 1);
    outvec.push_back(newword);
    idx = Nidx;
    word++;
  }
  return outvec;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

file::Path::NameType FileEnv::FilespecToContainingDirectory(const file::Path::NameType& path) {
  file::Path::NameType rval("");
  bool isurl                        = FileEnv::PathIsUrlForm(ork::file::Path(path.c_str()));
  file::Path::SmallNameType urlbase = FileEnv::UrlNameToBase(path.c_str());
  // This nonsense is so we can work with URLs too...
  file::Path::NameType UrlStrippedPath = path;
  if (isurl) {
    UrlStrippedPath = StripUrlFromPath(path.c_str());
  }

  bool isunix = filespec_isunix(UrlStrippedPath);
  bool isdos  = filespec_isdos(UrlStrippedPath);

  // Precondition
  OrkAssert(isunix || isdos);

  std::transform(UrlStrippedPath.begin(), UrlStrippedPath.end(), UrlStrippedPath.begin(), dos2unixpathsep());

  //	printf( "FilespecToContainingDirectory<%s>\n", path.c_str() );
  //	printf( "ix:UrlStrippedPath<%s>\n", UrlStrippedPath.c_str() );
  auto idx = UrlStrippedPath.find_last_of("/");
  //	printf( "idx<%d>\n", idx );
  if (idx != file::Path::NameType::npos) {
    file::Path::NameType tmp = UrlStrippedPath.substr(0, idx);
    rval                     = tmp;
  }

  // rval=TruncateAtFirstCharFromSet(UrlStrippedPath, "/");
  //	printf( "UrlStrippedPath<%s>\n", UrlStrippedPath.c_str() );
  //	printf( "rval<%s>\n", rval.c_str() );
  if (isurl) {
    rval = file::Path::NameType(urlbase.c_str()) + file::Path::NameType("://") + rval;
  }
  // std::transform( rval.begin(), rval.end(), rval.begin(), dos2unixpathsep() );
  //	printf( "rval<%s>\n", rval.c_str() );
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

file::Path::NameType
FileEnv::TruncateAtFirstCharFromSet(const file::Path::NameType& stringToTruncate, const file::Path::NameType& setOfChars) {
  size_t isetsize = setOfChars.length();

  file::Path::NameType::size_type ilchar = file::Path::NameType::npos;

  for (size_t i = 0; i < isetsize; i++) {
    char buffer[2] = {setOfChars.c_str()[0], 0};

    file::Path::NameType::size_type indexOfLastSeparator = stringToTruncate.find_last_of(&buffer[0]);

    if (indexOfLastSeparator > ilchar) {
      ilchar = indexOfLastSeparator;
    }
  }

  if (ilchar == file::Path::NameType::npos) {
    return "";
  } else {
    return stringToTruncate.substr(0, ilchar);
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FileEnv::BeginLinFile(const file::Path& lfn, ELINFILEMODE emode) {
  if (emode == ELFM_WRITE) {
    std::string lfile1     = std::string(lfn.c_str());
    GetRef().mpLinFile     = new File(lfile1.c_str(), ork::EFM_WRITE);
    GetRef().meLinFileMode = emode;
  } else if (emode == ELFM_READ) {
    std::string lfile3 = std::string("data/") + std::string(lfn.c_str());
    if (ork::FileEnv::DoesFileExist(lfile3.c_str())) {
      // OutputDebugString( "OpeningLinFile!!!\n" );
      GetRef().mpLinFile     = new File(lfile3.c_str(), ork::EFM_READ);
      GetRef().meLinFileMode = emode;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void FileEnv::EndLinFile() {
  if (GetRef().mpLinFile)
    delete GetRef().mpLinFile;

  GetRef().mpLinFile = 0;

  GetRef().meLinFileMode = ork::ELFM_NONE;
}

//////////////////////////////////////////////////////////

} // namespace ork
