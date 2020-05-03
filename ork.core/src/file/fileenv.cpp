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
#include <ork/kernel/datablock.inl>

namespace ork {

datablockptr_t datablockFromFileAtPath(const file::Path& path) {

  datablockptr_t rval = nullptr;
  if (FileEnv::GetRef().DoesFileExist(path)) {
    ork::File inputfile(path, ork::EFM_READ);
    size_t length = 0;
    inputfile.GetLength(length);
    rval       = std::make_shared<DataBlock>();
    void* dest = rval->allocateBlock(length);
    inputfile.Read(dest, length);
    inputfile.Close();
  }
  return rval;
}

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
  auto urlbase = env.uriProtoToBase(fileName.GetUrlBase().c_str());
  auto it      = env.uriRegistry().find(urlbase.c_str());
  bool found   = (it != env.uriRegistry().end());
  // printf("GetDeviceForUrl filename<%s> urlbase<%s> found<%d>\n", fileName.c_str(), urlbase.c_str(), int(found));
  if (found)
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

filedevctx_constptr_t FileEnv::contextForUriProto(const std::string& uriproto) {
  static filedevctx_ptr_t default_ctx = std::make_shared<FileDevContext>();
  auto& the_map                       = GetRef()._filedevcontext_map;
  auto it                             = the_map.find(uriproto);
  if (it != the_map.end()) {
    return it->second;
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

file::Path FileEnv::uriProtoToBase(const std::string& uriproto) {
  file::Path urlbase  = "";
  auto find_url_colon = uriproto.find_first_of(':', 0);
  if (uriproto.npos != find_url_colon) {
    if (int(uriproto.size()) > (find_url_colon + 2)) {
      if ((uriproto[find_url_colon + 1] == '/') || (uriproto[find_url_colon + 2] == '/')) {
        urlbase = uriproto.substr(0, find_url_colon + 3).c_str();
      }
    }
  }

  return urlbase;
}

///////////////////////////////////////////////////////////////////////////////

file::Path FileEnv::uriProtoToPath(const std::string& uriproto) {
  file::Path path     = "";
  auto find_url_colon = uriproto.find_first_of(':', 0);
  if (uriproto.npos != find_url_colon) {
    if (int(uriproto.size()) > (find_url_colon + 3)) {
      if ((uriproto[find_url_colon + 1] == '/') || (uriproto[find_url_colon + 2] == '/')) {
        auto urlbase = uriproto.substr(0, find_url_colon);
        auto urlp    = urlbase.c_str();
        if (OldStlSchoolIsInMap(GetRef()._filedevcontext_map, urlp)) {
          auto ipathbase = find_url_colon + 3;
          path           = uriproto.substr(ipathbase, uriproto.size() - ipathbase).c_str();
        }
      }
    }
  }

  if (path.length() == 0) {
    path = uriproto;
  }

  return path;
}

///////////////////////////////////////////////////////////////////////////////

filedevctx_ptr_t FileEnv::createContextForUriBase(
    const std::string& uriproto, //
    const file::Path& base_location) {

  printf(
      "createContextForUriBase proto<%s> baseloc<%s>\n", //
      uriproto.c_str(),
      base_location.c_str());

  auto context      = std::make_shared<FileDevContext>();
  context->_protoid = uriproto;
  context->setFilesystemBaseAbs(base_location);
  GetRef()._filedevcontext_map[uriproto] = context;
  context->SetPrependFilesystemBase(true);

  return context;
}

///////////////////////////////////////////////////////////////////////////////

bool FileEnv::PathIsUrlForm(const file::Path& PathName) {
  return PathName.HasUrlBase();
}

///////////////////////////////////////////////////////////////////////////////

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
  bool isurl   = FileEnv::PathIsUrlForm(ork::file::Path(path.c_str()));
  auto urlbase = FileEnv::uriProtoToBase(path.c_str());
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

//////////////////////////////////////////////////////////

} // namespace ork
