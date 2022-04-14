////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/slashnode.h>
#include <ork/util/crc.h>
#include <ork/application/application.h>
#include <ork/kernel/Array.h>
#include <ork/kernel/Array.hpp>
#include <ork/util/stl_ext.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <boost/filesystem.hpp>

template class ork::fixedvector<ork::file::Path, 8>;
bool gbas1 = true;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace file {

PathMarkers::PathMarkers()
    : mDriveLen(0)
    , mUrlBaseLen(0)
    , mFolderLen(0)
    , mFileNameLen(0)
    , mExtensionLen(0)
    , mQueryStringLen(0) {
}

///////////////////////////////////////////////////////////////////////////////
//
// yo							file
// yo.txt						file ext
// yo.txt/						folder
// data://yo/dude?dude=yo		url folder file query
// data://yo					url file
// data://yo/					url folder
// data://yo.ext				url file
// data://yo.ext/				url folder
// data://yo/dude				url folder file
// data://yo?dude=yo			url file query
//
///////////////////////////////////////////////////////////////////////////////

unsigned int PathMarkers::GetDriveBase() const {
  return unsigned(0);
}
unsigned int PathMarkers::GetUrlBase() const {
  return unsigned(0);
}
unsigned int PathMarkers::GetFolderBase() const {
  OrkAssert(false == ((mDriveLen > 0) && (mUrlBaseLen > 0)));
  unsigned int ib = (mDriveLen > mUrlBaseLen) ? mDriveLen : mUrlBaseLen;
  return ib;
}
unsigned int PathMarkers::GetFileNameBase() const {
  return GetFolderBase() + mFolderLen;
}
unsigned int PathMarkers::GetExtensionBase() const {
  bool bdot = (mExtensionLen > 0);

  return GetFileNameBase() + (bdot ? mFileNameLen + 1 : mFileNameLen);
}
unsigned int PathMarkers::GetQueryStringBase() const {
  unsigned iebas = GetExtensionBase();
  unsigned ielen = mExtensionLen;
  return (ielen > 0) ? iebas + ielen + 1 // extension base + extension length +  ?
                     : iebas + 1;        // extension base + ?
}

///////////////////////////////////////////////////////////////////////////////
Path::Path()
    : _pathstring("")
    , _markers() {
}
Path::Path(const PieceString& pathName)
    : _pathstring(pathName.c_str(), int(pathName.size()))
    , _markers() {
  set(_pathstring.c_str());
}
Path::Path(const char* pathName)
    : _pathstring("")
    , _markers() {
  set(pathName);
}
Path::Path(const std::string pathName)
    : Path(pathName.c_str()) {
}
Path::Path(const std::vector<std::string>& pathVect) {
  auto j = JoinString(pathVect, "/");
  j      = j.substr(0, j.size() - 1); // remove trailing /
  set(j.c_str());
}
Path::Path(const ork::PoolString& pathName)
    : _pathstring("")
    , _markers() {
  set(pathName.c_str());
}
Path::Path(const NameType& pathName)
    : _pathstring("")
    , _markers() {
  set(pathName.c_str());
}
///////////////////////////////////////////////////////////////////////////////
Path::~Path() {
}
///////////////////////////////////////////////////////////////////////////////
const char* Path::c_str() const {
  return _pathstring.c_str();
}
std::string Path::toStdString() const {
  return std::string(c_str());
}
///////////////////////////////////////////////////////////////////////////////

Path::HashType Path::Hash() const {
  NameType copy = _pathstring;
  //////////////////
  // 1st pass hash
  U32 uval = Crc32::HashMemory(copy.c_str(), int(strlen(copy.c_str())));
  // orkprintf( "HashPath path<%s> hash<%08x>\n", copy.c_str(), uval );
  //////////////////
  return file::Path::HashType(uval);
}

///////////////////////////////////////////////////////////////////////////////

size_t Path::length() const {
  return _pathstring.length();
}

///////////////////////////////////////////////////////////////////////////////

bool Path::empty() const {
  return _pathstring.empty();
}

///////////////////////////////////////////////////////////////////////////////

Path Path::operator+(const Path& oth) const {
  Path rval(*this);
  rval._pathstring += oth._pathstring;
  rval.ComputeMarkers('/');
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Path::operator=(const Path& oth) {
  _pathstring = oth._pathstring;
  _markers    = oth._markers;
}

///////////////////////////////////////////////////////////////////////////////

bool Path::operator<(const Path& oth) const {
  return (strcmp(c_str(), oth.c_str()) > 0);
}

///////////////////////////////////////////////////////////////////////////////

void Path::operator+=(const Path& oth) {
  _pathstring += oth._pathstring;
  ComputeMarkers('/');
}

///////////////////////////////////////////////////////////////////////////////

bool Path::operator!=(const Path& oth) const {
  return (0 != strcmp(c_str(), oth.c_str()));
}
bool Path::operator==(const Path& oth) const {
  return (0 == strcmp(c_str(), oth.c_str()));
}

///////////////////////////////////////////////////////////////////////////////

void Path::SetDrive(const char* Drv) {
  SmallNameType url, drive, ext;
  NameType folder, file, query;
  DeCompose(url, drive, folder, file, ext, query);

  if (strlen(Drv) == 0) {
    if (url.length() != 0) {
      url.set("");
    }
  }

  drive = Drv;

  Compose(url, drive, folder, file, ext, query);
}

void Path::SetUrlBase(const char* newurl) {
  SmallNameType url, drive, ext;
  NameType folder, file, query;
  DeCompose(url, drive, folder, file, ext, query);
  url = newurl;
  Compose(url, drive, folder, file, ext, query);
}

///////////////////////////////////////////////////////////////////////////////

static bool PathPred(const char* src, const char* loc, size_t isrclen) {
  if ((loc - src) > 1) {
    if (strncmp(loc - 1, "://", 3) == 0) {
      return false;
    }
  }

  return true;
}

void Path::set(const char* instr) {
  NameType tmp, tmp2;
  tmp.set(instr);
  //////////////////////////////////////////////
  // convert pathseps to internal format (posix)
  size_t ilen       = tmp.length();
  const char* begin = tmp.c_str();
  dos2unixpathsep xform;
  for (size_t i = 0; i < ilen; i++) {
    tmp.SetChar(i, xform(begin[i]));
  }
  //////////////////////////////////////////////
  tmp2.replace(tmp.c_str(), "/./", "/");
  _pathstring.replace(tmp2.c_str(), "//", "/", PathPred);
  //////////////////////////////////////////////
  ComputeMarkers('/');
}

///////////////////////////////////////////////////////////////////////////////

void Path::AppendFolder(const char* folderappend) {
  NameType Folder = GetFolder(EPATHTYPE_POSIX);
  Folder.append(folderappend, strlen(folderappend));
  SetFolder(Folder.c_str());
}

void Path::AppendFile(const char* fileappend) {
  NameType File = GetName();
  File.append(fileappend, strlen(fileappend));
  SetFile(File.c_str());
}

///////////////////////////////////////////////////////////////////////////////

void Path::SetFolder(const char* foldername) {
  if (0 == foldername) {
    SmallNameType url, drive, ext;
    NameType folder, file, query;
    DeCompose(url, drive, folder, file, ext, query);
    folder = "";
    Compose(url, drive, folder, file, ext, query);
  } else {
    size_t newlen = strlen(foldername);
    if (HasFolder() || (newlen > 0)) {
      SmallNameType url, drive, ext;
      NameType folder, file, query;
      DeCompose(url, drive, folder, file, ext, query);
      folder.set(foldername);
      Compose(url, drive, folder, file, ext, query);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Path::SetExtension(const char* newext) {
  if (0 == newext) {
    SmallNameType url, drive, ext;
    NameType folder, file, query;
    DeCompose(url, drive, folder, file, ext, query);
    ext = "";
    Compose(url, drive, folder, file, ext, query);
  } else {
    size_t newlen = strlen(newext);
    if (HasExtension() || (newlen > 0)) {
      SmallNameType url, drive, ext;
      NameType folder, file, query;
      DeCompose(url, drive, folder, file, ext, query);
      ext = (newext[0] == '.') ? SmallNameType(&newext[1]) : SmallNameType(newext);
      Compose(url, drive, folder, file, ext, query);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Path::SetFile(const char* newfile) {
  if (0 == newfile) {
    SmallNameType url, drive, ext;
    NameType folder, file, query;
    DeCompose(url, drive, folder, file, ext, query);
    file = "";
    Compose(url, drive, folder, file, ext, query);
  } else {
    size_t newlen = strlen(newfile);
    if (HasFile() || (newlen > 0)) {
      SmallNameType url, drive, ext;
      NameType folder, file, query;
      DeCompose(url, drive, folder, file, ext, query);
      file.set(newfile);
      Compose(url, drive, folder, file, ext, query);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////

Path::EPathType Path::GetNative() {
#if defined(_WIN32)
  return EPATHTYPE_DOS;
#elif defined(NITRO)
  return EPATHTYPE_NDS;
#else
  return EPATHTYPE_POSIX;
#endif
}

///////////////////////////////////////////////////////////////////////////////

bool Path::HasDrive() const {
  return _markers.mDriveLen > 0;
}

bool Path::HasUrlBase() const {
  return _markers.mUrlBaseLen > 0;
}

bool Path::HasFolder() const {
  return _markers.mFolderLen > 0;
}

bool Path::HasQueryString() const {
  return _markers.mQueryStringLen > 0;
}

bool Path::HasExtension() const {
  return _markers.mExtensionLen > 0;
}

bool Path::HasFile() const {
  return _markers.mFileNameLen > 0;
}

///////////////////////////////////////////////////////////////////////////////

// void Path::SetFolder(const ork::StringTableIndex& pathName)
//{
// InitializeMemberVariables(std::string(pathName.c_str()));
//}

///////////////////////////////////////////////////////////////////////////////

bool Path::IsAbsolute() const {
  ////////////////
  const char* instr  = c_str();
  int ilen           = int(strlen(instr));
  bool bleadingslash = (ilen > 0) ? instr[0] == '/' : false;
  ////////////////

  return HasUrlBase() || HasDrive() || bleadingslash;
}

///////////////////////////////////////////////////////////////////////////////

bool Path::IsRelative() const {
  return (IsAbsolute() == false);
}

///////////////////////////////////////////////////////////////////////////////
// relative means relative to the working folder

Path Path::ToRelative(EPathType etype) const {
  Path rval = ToAbsoluteFolder(etype);
  rval += Path(GetName());
  rval += GetExtension().c_str();
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

Path Path::ToAbsolute(EPathType etype) const {
  // printf( "Path::ToAbsolute (begin) inp<%s>\n", this->c_str()  );
  Path tmp = ToAbsoluteFolder(etype);
  Path rval;
  if (HasExtension()) {
    OrkHeapCheck();
    char buffer[1024];

    Path::NameType fname     = GetName();
    Path::SmallNameType fext = GetExtension();
    const char* ptmp         = tmp.c_str();
    const char* pname        = fname.c_str();
    const char* pext         = fext.c_str();

    snprintf(buffer, sizeof(buffer), "%s%s.%s", ptmp, pname, pext);
    rval._pathstring = Path::NameType(&buffer[0]); //.format( "%s%s.%s",  );
    OrkHeapCheck();
  } else {
    rval._pathstring.format("%s%s", tmp.c_str(), GetName().c_str());
  }

#if defined(WIN32)
  if (etype == EPATHTYPE_NATIVE)
    etype = EPATHTYPE_DOS;
#else
  if (etype == EPATHTYPE_NATIVE)
    etype = EPATHTYPE_POSIX;
#endif

  switch (etype) {
    case EPATHTYPE_DOS: {
      Path::NameType nt = rval._pathstring;
      rval._pathstring.replace(nt.c_str(), '/', '\\');
      rval._pathstring.replace(nt.c_str(), '/', '\\');
      Path::NameType nt2;
      nt2.replace(rval._pathstring.c_str(), "\\\\", "\\");
      rval._pathstring = nt2;
      rval.ComputeMarkers('\\');
      break;
    }
    case EPATHTYPE_POSIX: {
      Path::NameType nt = rval._pathstring;
      rval._pathstring.replace(nt.c_str(), '\\', '/');
      rval.ComputeMarkers('/');
      break;
    }
    default:
      OrkAssert(false);
  }
  // printf("Path::ToAbsolute (end) inp<%s> out<%s> tmp<%s>\n", this->c_str(), rval.c_str(), tmp.c_str());
  return rval;
}

Path Path::ToAbsoluteFolder(EPathType etype) const {
  // printf( " Path::ToAbsoluteFolder (begin) inp<%s>\n", this->c_str()  );

  if (etype == EPATHTYPE_NATIVE)
    etype = Path::GetNative();

  Path rval;

  if (HasUrlBase()) {
    std::string urlbase  = GetUrlBase().c_str();
    auto urictx          = ork::FileEnv::contextForUriProto(urlbase.c_str());
    auto basepath        = urictx->getFilesystemBaseAbs();
    std::string thispath = this->c_str();

    std::string stripped = string::replaced(thispath, urlbase, "");

    auto path = basepath; // / stripped;
    /*printf(
        " Path::ToAbsoluteFolder urlbase<%s> basepath<%s> stripped<%s> pstr<%s>\n", //
        urlbase.c_str(),
        basepath.c_str(),
        stripped.c_str(),
        path.c_str());*/
    size_t ilen = strlen(path.c_str());

    bool b_ends_with_slash = path.c_str()[ilen - 1] == '/';

    rval._pathstring.format(b_ends_with_slash ? "%s" : "%s/", path.c_str());
  } else if (HasDrive()) {
    // rval._pathstring.format("%s", FileEnv::GetPathFromUrlExt(GetDrive().c_str()).c_str());
  } else if (IsAbsolute()) {
    switch (etype) {
      case EPATHTYPE_NATIVE:
      case EPATHTYPE_URL:
        break;
      case EPATHTYPE_DOS: {
        if (_markers.mDriveLen == 3) {
          rval._pathstring.format("%.3s", rval._pathstring.c_str());
        }
        break;
      }
      case EPATHTYPE_POSIX: {
        rval._pathstring.format("/");
        break;
      }
      case EPATHTYPE_NDS: {
        // rval._pathstring.format( "rom:%s/", _pathstring.c_str() );
        rval._pathstring.format("%s", _pathstring.c_str());
        break;
      }
    }
  } else {
    switch (etype) {
      case EPATHTYPE_POSIX:
      case EPATHTYPE_DOS:
      case EPATHTYPE_NATIVE:
      case EPATHTYPE_URL:
        break;
      case EPATHTYPE_NDS: {
        rval._pathstring.format("");
        break;
      }
    }
  }

  rval._pathstring += GetFolder(etype); // GetFolder already does tonative pathsep
  switch (etype) {
    default: {
      rval.ComputeMarkers('/');
      break;
    }
  }
  // printf( " Path::ToAbsoluteFolder (end) inp<%s> AbsoluteFolder<%s>\n", this->c_str(), rval.c_str() );
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

Path::SmallNameType Path::GetUrlBase() const {
  Path::SmallNameType rval;
  int ilen = int(_markers.mUrlBaseLen);
  int ibas = int(_markers.GetUrlBase());
  for (int i = 0; i < ilen; i++) {
    rval.SetChar(int(i), c_str()[ibas + i]);
  }
  rval.SetChar(ilen, 0);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

Path::SmallNameType Path::GetDrive() const {
  Path::SmallNameType rval;
  int ilen = _markers.mDriveLen;
  int ibas = _markers.GetDriveBase();
  for (int i = 0; i < ilen; i++) {
    rval.SetChar(i, c_str()[ibas + i]);
    // strncpy( rval.c_str(), c_str()+ibas, ilen );
  }
  rval.SetChar(ilen, 0);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

Path::NameType Path::GetName() const {
  Path::NameType rval;
  int ilen = int(_markers.mFileNameLen);
  int ibas = int(_markers.GetFileNameBase());
  for (int i = 0; i < ilen; i++) {
    rval.SetChar(i, c_str()[ibas + i]);
    // strncpy( rval.c_str(), c_str()+ibas, ilen );
  }
  rval.SetChar(ilen, 0);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

Path::SmallNameType Path::GetExtension() const {
  Path::SmallNameType rval;
  int ilen = int(_markers.mExtensionLen);
  int ibas = int(_markers.GetExtensionBase());
  for (int i = 0; i < ilen; i++) {
    rval.SetChar(i, c_str()[ibas + i]);
    // strncpy( rval.c_str(), c_str()+ibas, ilen );
  }
  rval.SetChar(ilen, 0);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

Path::NameType Path::GetQueryString() const {
  if (_markers.mQueryStringLen) {
    // orkprintf( "yo\n" );
  }
  Path::NameType rval;
  int ilen = _markers.mQueryStringLen;
  int ibas = _markers.GetQueryStringBase();
  if (ilen) {
    for (int i = 0; i < ilen; i++) {
      rval.SetChar(i, c_str()[ibas + i]);
      // strncpy( rval.c_str(), c_str()+ibas, ilen );
    }
  }
  rval.SetChar(ilen, 0);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

Path::NameType Path::GetFolder(EPathType etype) const {
  Path::NameType rval;
  int ilen = _markers.mFolderLen;
  int ibas = _markers.GetFolderBase();
  for (int i = 0; i < ilen; i++) {
    rval.SetChar(i, c_str()[ibas + i]);
    // strncpy( rval.c_str(), c_str()+ibas, ilen );
  }
  rval.SetChar(ilen, 0);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

Path Path::StripBasePath(const NameType& base) const {
  Path basePath(base);
  file::Path::NameType thisString = this->ToAbsolute(EPATHTYPE_POSIX).c_str();
  file::Path::NameType baseString = basePath.ToAbsolute(EPATHTYPE_POSIX).c_str();

  if (thisString.find(baseString) == 0)
    return Path(thisString.substr(baseString.length()).c_str());
  else
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

const char* FindLastCharBefore(const char* src, int isrclen, const char search, const char b4) {
  const char* rval = 0;

  for (int i = 0; i < isrclen; i++) {
    if (src[i] == search) {
      rval = src + i;
    } else if (src[i] == b4) {
      return rval;
    }
  }
  return rval;
}

//////////////////////////////////////////////////////////////////////////////

void Path::ComputeMarkers(char pathsep) {
  const char* instr = c_str();

  int ilen = int(strlen(instr));

  //////////////////////////////////////////////
  // find feature markers

  const char* qmark  = strstr(instr, "?");
  const char* umark  = strstr(instr, "://");
  const char* dmark  = strstr(instr, ":");
  const char* pmark  = FindLastCharBefore(instr, ilen, '.', '?');
  const char* lsmark = FindLastCharBefore(instr, ilen, pathsep, '?');

  ////////////////////////////////////////////
  // if . before last slash, then it is a folder . and not an ext .
  ////////////////////////////////////////////
  if (pmark < lsmark) {
    pmark = 0;
  }
  ////////////////////////////////////////////

  const char* folder_end_slash = qmark ? lsmark : strrchr(instr, pathsep);
  // const char* folder_beg_slash = strchr( instr, '/' );

  _markers.mDriveLen       = 0;
  _markers.mExtensionLen   = 0;
  _markers.mFileNameLen    = 0;
  _markers.mFolderLen      = 0;
  _markers.mQueryStringLen = 0;
  _markers.mUrlBaseLen     = 0;

  int istate = 0;

  /////////////////////////////////////////////
  // compute initial state based on presences of certain characters
  /////////////////////////////////////////////

  if (umark)
    istate = 0;
  else if (dmark) {
    if (qmark) {
      if (dmark < qmark) // colon before query sep ?
      {
        istate = 1;
      }
    } else {
      istate = 1;
    }
  }

  if ((0 == qmark) && (0 == umark) && (0 == dmark) && (0 == lsmark)) {
    istate = 3;
  } else if ((0 != lsmark) && (0 == dmark) && (0 == umark)) {
    istate = 2;
  }
  /////////////////////////////////////////////
  // simple length parsing here
  /////////////////////////////////////////////
  if (qmark) {
    _markers.mQueryStringLen = ilen - (qmark - instr) + 1;
  }
  /////////////////////////////////////////////
  // update marker loop
  /////////////////////////////////////////////

  int imarkerstart = 0;

  for (int ic = 0; ic < ilen; ic++) {
    const char* ch = instr + ic;

    switch (istate) {
      case 0: // url
        if (*ch == ':') {
          _markers.mUrlBaseLen = ic + 3;
          imarkerstart         = _markers.mUrlBaseLen;
          istate               = 2;
          ic                   = imarkerstart;
          // folder_beg_slash = strchr( instr+imarkerstart, '/' );
        }
        break;
      case 1: // drive
        if (*ch == ':') {
          _markers.mDriveLen = ic + 2;
          imarkerstart       = _markers.mDriveLen;
          istate             = 2;
          ic                 = imarkerstart;
          // folder_beg_slash = strchr( instr+imarkerstart, '/' );
        }
        break;
      case 2: // folder
      {
        intptr_t ilastslashp = (folder_end_slash - instr);

        if (imarkerstart) {
          if (strchr(instr + imarkerstart, pathsep) == 0) {
            istate++;
            ic -= 2;
            continue;
          }
        }
        OrkAssert(folder_end_slash != 0);
        if (*ch == pathsep && (ch == folder_end_slash)) {
          _markers.mFolderLen = (ic + 1 - imarkerstart);
          imarkerstart += _markers.mFolderLen;
          istate = 3;
        }
        break;
      }
      case 3: // file
        if (pmark) {
          _markers.mFileNameLen = (pmark - ch);
          imarkerstart += _markers.mFileNameLen;
          istate = 4;
        } else if (qmark) {
        } else {
          _markers.mFileNameLen++;
          imarkerstart++;
        }
        break;
      case 4: // ext
        if (qmark) {
          _markers.mExtensionLen = (ilen - imarkerstart) - _markers.mQueryStringLen;
          imarkerstart += _markers.mExtensionLen;
          istate++;
        } else {
          _markers.mExtensionLen = ilen - imarkerstart;
          imarkerstart += _markers.mExtensionLen;
          istate++;
        }
        break;
      case 5: // query
      {
        // if( qmark )
        //{
        // _markers.mQueryStringLen = ilen - (qmark-instr);
        //}
        istate++;
        break;
      }
      case 6: // end
        break;
    }
  }

  int itot = _markers.mDriveLen + _markers.mUrlBaseLen + _markers.mFolderLen + _markers.mFileNameLen + _markers.mExtensionLen +
             _markers.mQueryStringLen;

  if (itot != ilen) {
    printf("instr<%s> itot<%d> ilen<%d>\n", instr, itot, ilen);

    while (1) {
      usleep(1000);
    }
  }

  // assert(gbas1);
  OrkAssert(itot == ilen);

  /*
      /////////////////////////////////
      // url seperator
      int iumark = (umark==0) ? -1 : umark-instr;

      /////////////////////////////////
      // drive seperator
      int idmark = (dmark==0) ? -1 : (umark==0) ? dmark-instr : -1;

      /////////////////////////////////
      // LAST extension seperator BEFORE query string

      //while( qmark && (pmark>qmark) )
      //{
      //	pmark = strrchr( pmark, '/' );
      //}
      int ipmark = (pmark==0) ? -1 : pmark-instr;
      if( ipmark < iumark+3 ) ipmark=-1;
      if( ipmark < idmark+2 ) ipmark=-1;

      /////////////////////////////////
      // LAST path seperator BEFORE query string

      //while( qmark && (lsmark>qmark) )
      //{
      //	lsmark = strrchr( lsmark, '/' );
      //}
      int ilsmark = (lsmark==0) ? -1 : lsmark-instr;
      if( ilsmark < iumark+3 ) ilsmark=-1;
      if( ilsmark < idmark+2 ) ilsmark=-1;

      //////////////////////////////////////////////

      int icuepos = 0;

      //////////////////////////////////////////////
      // first the url or drive OR leading /

      bool bleadingslash = (firstslash==instr);

      if( iumark>=0 )
      {
          _markers.mUrlBaseLen = iumark+3;
          _markers.mDriveLen = 0;
          icuepos += _markers.mUrlBaseLen;
      }
      else if( idmark>=0 )
      {
          OrkAssert( idmark==1 ); // if its a drive letter, it better be the second character
          OrkAssert( instr[2] == pathsep );
          _markers.mDriveLen = 3;
          _markers.mUrlBaseLen = 0;
          icuepos += _markers.mDriveLen;
      }
      else if( bleadingslash )
      {
          _markers.mDriveLen = 1;
          _markers.mUrlBaseLen = 0;
          icuepos++;
      }
      else
      {
          _markers.mDriveLen = 0;
          _markers.mUrlBaseLen = 0;
      }

      /////////////////////////////////
      // then the last slash

      if( ilsmark>=0 )
      {
          if( ipmark>=0 ) // make sure last slash BEFORE
          {
              OrkAssert( lsmark<pmark );
          }
          _markers.mFolderLen = (ilsmark-icuepos)+1;
          icuepos += _markers.mFolderLen;
      }
      else
      {
          _markers.mFolderLen = 0;
      }

      /////////////////////////////////
      // query seperator
      /////////////////////////////////
      if( qmark )
      {
          //orkprintf( "yo\n" );
      }
      int iqmark = (qmark==0) ? -1 : qmark-instr;

      /////////////////////////////////
      // then the file (between last_slash and ( ext | query | end ))

      int ifilbeg = icuepos;
      int ifilend = ilen;
      ifilend = ((ipmark>=0)&&(ipmark<ifilend)) ? ipmark : ifilend;
      ifilend = ((iqmark>=0)&&(iqmark<ifilend)) ? iqmark : ifilend;
      _markers.mFileNameLen = (ifilend-ifilbeg);
      icuepos += _markers.mFileNameLen;

      /////////////////////////////////
      // then the extension

      if( ipmark>=0 )
      {
          int iextbeg = ipmark+1;
          int iextend = ((iqmark>=0)&&(iqmark<ilen)) ? iqmark : ilen;
          _markers.mExtensionLen = (iextend-iextbeg);
          icuepos += _markers.mExtensionLen+1;
      }
      else
      {
          _markers.mExtensionLen = 0;
      }

      /////////////////////////////////
      // then the querystring

      if( iqmark>=0 )
      {
          int iqrybeg = iqmark+1;
          int iqryend = ilen;
          _markers.mQueryStringLen = (iqryend-iqrybeg);
          icuepos += _markers.mQueryStringLen+1;
      }
      else
      {
          _markers.mQueryStringLen = 0;
      }

      /////////////////////////////////

      OrkAssert( icuepos == ilen );*/
}

///////////////////////////////////////////////////////////////////////////////

void Path::Compose(
    const ork::file::Path::SmallNameType& url,
    const ork::file::Path::SmallNameType& drive,
    const ork::file::Path::NameType& folder,
    const ork::file::Path::NameType& file,
    const ork::file::Path::SmallNameType& ext,
    const ork::file::Path::NameType& query) {
  size_t iul = url.length();
  size_t idl = drive.length();
  size_t ifl = folder.length();
  size_t igl = file.length();
  size_t iel = ext.length();
  size_t iql = query.length();

  OrkAssert(false == ((iul > 0) && (idl > 0)));
  OrkAssert((idl == 0) || (idl == 3));

  NameType str;

  if (iul)
    str.append(url.c_str(), iul);
  if (idl)
    str.append(drive.c_str(), idl);
  if (ifl)
    str.append(folder.c_str(), ifl);
  if (igl)
    str.append(file.c_str(), igl);
  if (iel) {
    str.append(".", 1);
    str.append(ext.c_str(), iel);
  }
  if (iql) {
    str.append("?", 1);
    str.append(query.c_str(), iql);
  }
  set(str.c_str());
}

///////////////////////////////////////////////////////////////////////////////

void Path::DeCompose(
    ork::file::Path::SmallNameType& url,
    ork::file::Path::SmallNameType& drive,
    ork::file::Path::NameType& folder,
    ork::file::Path::NameType& file,
    ork::file::Path::SmallNameType& ext,
    ork::file::Path::NameType& query) {

  OrkAssert(false == (HasUrlBase() && HasDrive()));
  if (HasUrlBase()) { // strncpy( url.c_str(), c_str()+_markers.GetUrlBase(), _markers.GetUrlLength() );
    // url.c_str()[ _markers.GetUrlLength() ] = 0;
    url.set(c_str() + _markers.GetUrlBase(), _markers.GetUrlLength());
    url.SetChar(_markers.GetUrlLength(), 0);
  } else {
    // url.c_str()[0] = 0;
    url.SetChar(0, 0);
  }
  if (HasDrive()) {
    // strncpy( drive.c_str(), c_str()+_markers.GetDriveBase(), _markers.GetDriveLength() );
    // drive.c_str()[ _markers.GetDriveLength() ] = 0;
    drive.set(c_str() + _markers.GetDriveBase(), _markers.GetDriveLength());
    drive.SetChar(_markers.GetDriveLength(), 0);
  } else {
    // drive.c_str()[0] = 0;
    drive.SetChar(0, 0);
  }
  if (HasFolder()) {
    // strncpy( folder.c_str(), c_str()+_markers.GetFolderBase(), _markers.GetFolderLength() );
    // folder.c_str()[ _markers.GetFolderLength() ] = 0;
    folder.set(c_str() + _markers.GetFolderBase(), _markers.GetFolderLength());
    folder.SetChar(_markers.GetFolderLength(), 0);
  } else {
    // folder.c_str()[0] = 0;
    folder.SetChar(0, 0);
  }
  if (HasFile()) {
    // strncpy( file.c_str(), c_str()+_markers.GetFileNameBase(), _markers.GetFileNameLength() );
    // file.c_str()[ _markers.GetFileNameLength() ] = 0;
    file.set(c_str() + _markers.GetFileNameBase(), _markers.GetFileNameLength());
    file.SetChar(_markers.GetFileNameLength(), 0);
  } else {
    // file.c_str()[0] = 0;
    file.SetChar(0, 0);
  }
  if (HasExtension()) {
    // strncpy( ext.c_str(), c_str()+_markers.GetExtensionBase(), _markers.GetExtensionLength() );
    // ext.c_str()[ _markers.GetExtensionBase() ] = 0;
    int ibase = _markers.GetExtensionBase();
    ext.set(c_str() + ibase, _markers.GetExtensionLength());
    ext.SetChar(_markers.GetExtensionLength(), 0);
  } else {
    ext.SetChar(0, 0);
    // ext.c_str()[0] = 0;
  }
  if (HasQueryString()) {
    // strncpy( query.c_str(), c_str()+_markers.GetQueryStringBase(), _markers.GetQueryStringLength() );
    // query.c_str()[ _markers.GetQueryStringLength() ] = 0;
    query.set(c_str() + _markers.GetQueryStringBase(), _markers.GetQueryStringLength());
    query.SetChar(_markers.GetQueryStringLength(), 0);
  } else {
    query.SetChar(0, 0);
    // query.c_str()[0] = 0;
  }
}

///////////////////////////////////////////////////////////////////////////////

void Path::Compose(const DecomposedPath& decomposed) {
  size_t iul = decomposed.mProtocol.length();
  size_t idl = decomposed.mDrive.length();
  size_t ifl = decomposed.mFolder.length();
  size_t igl = decomposed.mFile.length();
  size_t iel = decomposed.mExtension.length();
  size_t iql = decomposed.mQuery.length();

  OrkAssert(false == ((iul > 0) && (idl > 0)));
  OrkAssert((idl == 0) || (idl == 3));

  NameType str;

  if (iul)
    str.append(decomposed.mProtocol.c_str(), iul);
  if (idl)
    str.append(decomposed.mDrive.c_str(), idl);
  if (ifl)
    str.append(decomposed.mFolder.c_str(), ifl);
  if (igl)
    str.append(decomposed.mFile.c_str(), igl);
  if (iel) {
    str.append(".", 1);
    str.append(decomposed.mExtension.c_str(), iel);
  }
  if (iql) {
    str.append("?", 1);
    str.append(decomposed.mQuery.c_str(), iql);
  }
  set(str.c_str());
}

void Path::DeCompose(DecomposedPath& decomposed) {
  OrkAssert(false == (HasUrlBase() && HasDrive()));
  if (HasUrlBase()) {
    // strncpy( url.c_str(), c_str()+_markers.GetUrlBase(), _markers.GetUrlLength() );
    // url.c_str()[ _markers.GetUrlLength() ] = 0;
    decomposed.mProtocol.set(c_str() + _markers.GetUrlBase(), _markers.GetUrlLength());
    decomposed.mProtocol.SetChar(_markers.GetUrlLength(), 0);
  } else {
    // url.c_str()[0] = 0;
    decomposed.mProtocol.SetChar(0, 0);
  }
  if (HasDrive()) {
    // strncpy( drive.c_str(), c_str()+_markers.GetDriveBase(), _markers.GetDriveLength() );
    // drive.c_str()[ _markers.GetDriveLength() ] = 0;
    decomposed.mDrive.set(c_str() + _markers.GetDriveBase(), _markers.GetDriveLength());
    decomposed.mDrive.SetChar(_markers.GetDriveLength(), 0);
  } else {
    // drive.c_str()[0] = 0;
    decomposed.mDrive.SetChar(0, 0);
  }
  if (HasFolder()) {
    // strncpy( folder.c_str(), c_str()+_markers.GetFolderBase(), _markers.GetFolderLength() );
    // folder.c_str()[ _markers.GetFolderLength() ] = 0;
    decomposed.mFolder.set(c_str() + _markers.GetFolderBase(), _markers.GetFolderLength());
    decomposed.mFolder.SetChar(_markers.GetFolderLength(), 0);
  } else {
    // folder.c_str()[0] = 0;
    decomposed.mFolder.SetChar(0, 0);
  }
  if (HasFile()) {
    // strncpy( file.c_str(), c_str()+_markers.GetFileNameBase(), _markers.GetFileNameLength() );
    // file.c_str()[ _markers.GetFileNameLength() ] = 0;
    decomposed.mFile.set(c_str() + _markers.GetFileNameBase(), _markers.GetFileNameLength());
    decomposed.mFile.SetChar(_markers.GetFileNameLength(), 0);
  } else {
    // file.c_str()[0] = 0;
    decomposed.mFile.SetChar(0, 0);
  }
  if (HasExtension()) {
    // strncpy( ext.c_str(), c_str()+_markers.GetExtensionBase(), _markers.GetExtensionLength() );
    // ext.c_str()[ _markers.GetExtensionBase() ] = 0;
    int ibase = _markers.GetExtensionBase();
    decomposed.mExtension.set(c_str() + ibase, _markers.GetExtensionLength());
    decomposed.mExtension.SetChar(_markers.GetExtensionLength(), 0);
  } else {
    decomposed.mExtension.SetChar(0, 0);
    // ext.c_str()[0] = 0;
  }
  if (HasQueryString()) {
    // strncpy( query.c_str(), c_str()+_markers.GetQueryStringBase(), _markers.GetQueryStringLength() );
    // query.c_str()[ _markers.GetQueryStringLength() ] = 0;
    decomposed.mQuery.set(c_str() + _markers.GetQueryStringBase(), _markers.GetQueryStringLength());
    decomposed.mQuery.SetChar(_markers.GetQueryStringLength(), 0);
  } else {
    decomposed.mQuery.SetChar(0, 0);
    // query.c_str()[0] = 0;
  }
}

///////////////////////////////////////////////////////////////////////////////

void Path::SplitQuery(NameType& preq, NameType& postq) const {
  if (HasQueryString()) {
    unsigned qpos = _markers.GetQueryStringBase();
    preq.SetChar(0, 0);
    preq.append(c_str(), int(qpos - 1));
    postq.SetChar(0, 0);
    postq.append(c_str() + qpos, int(_markers.GetQueryStringLength()));

  } else {
    preq = _pathstring;
    postq.SetChar(0, 0);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Path::Split(NameType& preq, NameType& postq, char sep) const {
  const char* c_str   = _pathstring.c_str();
  const char* sep_loc = strrchr(c_str, sep);

  if (sep_loc) {
    size_t p = (sep_loc - c_str);
    preq.SetChar(0, 0);
    preq.append(c_str, p);
    postq.SetChar(0, 0);
    postq.append(c_str + p + 1, strlen(c_str) - p - 1);

  } else {
    preq = _pathstring;
    postq.SetChar(0, 0);
  }
}

///////////////////////////////////////////////////////////////////////////////

bool Path::DoesPathExist() const {
  struct stat file_stat;
  int ist = stat(c_str(), &file_stat);
  // printf( "stat<%s> : %d\n", c_str(), ist );
  return (ist == 0);
}
bool Path::IsFile() const {
  struct stat file_stat;
  int ist = stat(c_str(), &file_stat);
  // printf( "stat<%s> : %d\n", c_str(), ist );
  return (ist == 0) ? bool(S_ISREG(file_stat.st_mode)) : false;
}
bool Path::IsFolder() const {
  struct stat file_stat;
  int ist = stat(c_str(), &file_stat);
  // printf( "stat<%s> : %d\n", c_str(), ist );
  return (ist == 0) ? bool(S_ISREG(file_stat.st_mode)) : false;
}
bool Path::IsSymLink() const {
  struct stat file_stat;
  int ist = stat(c_str(), &file_stat);
  // printf( "stat<%s> : %d\n", c_str(), ist );
  return (ist == 0) ? bool(S_ISREG(file_stat.st_mode)) : false;
}

///////////////////////////////////////////////////////////////////////////////
// using BFS goes against ork::Path's memory policy of not using the
//  heap, but were not trying to run on the DS or PSP anymore
//  so it does not matter. Probably should start using heap allocated strings
//  for Path anyway.. Paths tend not to be used in performance critical areas
//  anyway.
///////////////////////////////////////////////////////////////////////////////

Path::Path(const boost::filesystem::path& p) {
  this->set(p.c_str());
}

boost::filesystem::path Path::toBFS() const {
  return boost::filesystem::path(c_str());
}
void Path::fromBFS(const boost::filesystem::path& p) {
  set(p.c_str());
}
Path Path::operator/(const Path& rhs) const {
  auto a = this->toBFS();
  auto b = rhs.toBFS();
  auto c = a / b;
  Path rval;
  rval.set(c.c_str());
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
// standard path retrieval
///////////////////////////////////////////////////////////////////////////////

Path Path::orkroot_dir() {
  const char* ORKROOT_DIR = getenv("ORKID_WORKSPACE_DIR");
  Path p(ORKROOT_DIR);
  return p;
}
Path Path::stage_dir() {
  const char* STAGE_DIR = getenv("OBT_STAGE");
  Path p(STAGE_DIR);
  return p;
}
Path Path::data_dir() {
  return (orkroot_dir() / "ork.data");
}
Path Path::bin_dir() {
  return (stage_dir() / "bin");
}
Path Path::lib_dir() {
  return (stage_dir() / "lib");
}
Path Path::dblockcache_dir() {
  return (stage_dir() / "dblockcache");
}
Path Path::share_dir() {
  return (stage_dir() / "share");
}
Path Path::temp_dir() {
  return (stage_dir() / "tempdir");
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::file
