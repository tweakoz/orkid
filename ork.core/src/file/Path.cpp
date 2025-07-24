////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/util/logger.h>
#include <random>
#include <sstream>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

template class ork::fixedvector<ork::file::Path, 8>;
bool gbas1 = true;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace file {

static logchannel_ptr_t logchan_path = logger()->configureChannel("path", fvec3(1,1,.9));

PathMarkers::PathMarkers()
    : mUrlBaseLen(0)
    , mFolderLen(0)
    , mFileNameLen(0)
    , mExtensionLen(0) {
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

unsigned int PathMarkers::getUrlBase() const {
  return unsigned(0);
}
unsigned int PathMarkers::getFolderBase() const {
  return mUrlBaseLen;
}
unsigned int PathMarkers::getFileNameBase() const {
  return getFolderBase() + mFolderLen;
}
unsigned int PathMarkers::getExtensionBase() const {
  bool bdot = (mExtensionLen > 0);

  return getFileNameBase() + (bdot ? mFileNameLen + 1 : mFileNameLen);
}

///////////////////////////////////////////////////////////////////////////////
Path::Path()
    : _pathstring("")
    , _markers() {
}
Path::Path(const PieceString& pathName)
    : _pathstring(pathName.c_str(), pathName.size())
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
// Path(const NameType&) constructor removed - NameType is now std::string
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

Path::HashType Path::hash() const {
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
  rval.computeMarkers('/');
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
  computeMarkers('/');
}

///////////////////////////////////////////////////////////////////////////////

bool Path::operator!=(const Path& oth) const {
  return (0 != strcmp(c_str(), oth.c_str()));
}
bool Path::operator==(const Path& oth) const {
  return (0 == strcmp(c_str(), oth.c_str()));
}

///////////////////////////////////////////////////////////////////////////////


void Path::setUrlBase(const char* newurl) {
  DecomposedPath decomp;
  decompose(decomp);
  decomp.mProtocol = newurl;
  compose(decomp);
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
  std::string tmp(instr);
  //////////////////////////////////////////////
  // normalize path separators to posix format
  dos2unixpathsep xform; // converts \ to /
  for (size_t i = 0; i < tmp.length(); i++) {
    tmp[i] = xform(tmp[i]);
  }
  //////////////////////////////////////////////
  // Replace /./  with /
  size_t pos = 0;
  while ((pos = tmp.find("/./", pos)) != std::string::npos) {
    tmp.replace(pos, 3, "/");
  }
  // Replace // with / (but not after :)
  _pathstring = tmp;
  eatDoubleSlashes();
  //////////////////////////////////////////////
  computeMarkers('/');
}

///////////////////////////////////////////////////////////////////////////////

void Path::appendFolder(const char* folderappend) {
  std::string folder = getFolder(EPATHTYPE_POSIX);
  folder.append(folderappend);
  setFolder(folder.c_str());
}

void Path::appendFile(const char* fileappend) {
  std::string file = getName();
  file.append(fileappend);
  setFile(file.c_str());
}

///////////////////////////////////////////////////////////////////////////////

void Path::setFolder(const char* foldername) {
  DecomposedPath decomp;
  decompose(decomp);
  if (foldername == nullptr) {
    decomp.mFolder = "";
  } else {
    decomp.mFolder = foldername;
  }
  compose(decomp);
}

///////////////////////////////////////////////////////////////////////////////

void Path::setExtension(const char* newext) {
  DecomposedPath decomp;
  decompose(decomp);
  if (newext == nullptr) {
    decomp.mExtension = "";
  } else {
    // Skip leading dot if present
    decomp.mExtension = (newext[0] == '.') ? &newext[1] : newext;
  }
  compose(decomp);
}

///////////////////////////////////////////////////////////////////////////////

void Path::setFile(const char* newfile) {
  DecomposedPath decomp;
  decompose(decomp);
  if (newfile == nullptr) {
    decomp.mFile = "";
  } else {
    decomp.mFile = newfile;
  }
  compose(decomp);
}
///////////////////////////////////////////////////////////////////////////////

Path::EPathType Path::GetNative() {
  return EPATHTYPE_POSIX;
}

///////////////////////////////////////////////////////////////////////////////

bool Path::hasUrlBase() const {
  return _markers.mUrlBaseLen > 0;
}

bool Path::hasFolder() const {
  return _markers.mFolderLen > 0;
}

bool Path::hasExtension() const {
  return _markers.mExtensionLen > 0;
}

bool Path::hasFile() const {
  return _markers.mFileNameLen > 0;
}

///////////////////////////////////////////////////////////////////////////////

// void Path::SetFolder(const ork::StringTableIndex& pathName)
//{
// InitializeMemberVariables(std::string(pathName.c_str()));
//}

///////////////////////////////////////////////////////////////////////////////

bool Path::isAbsolute() const {
  ////////////////
  const char* instr  = c_str();
  int ilen           = int(strlen(instr));
  bool bleadingslash = (ilen > 0) ? instr[0] == '/' : false;
  ////////////////

  return hasUrlBase() || bleadingslash;
}

///////////////////////////////////////////////////////////////////////////////

bool Path::isRelative() const {
  return (isAbsolute() == false);
}

///////////////////////////////////////////////////////////////////////////////
// relative means relative to the working folder

Path Path::toRelative(EPathType etype) const {
  Path rval = toAbsoluteFolder(etype);
  rval += Path(getName());
  if (hasExtension()) {
    rval._pathstring += "." + getExtension();
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

Path Path::toAbsolute(EPathType etype) const {
  // printf( "Path::toAbsolute (begin) inp<%s>\n", this->c_str()  );
  Path tmp = toAbsoluteFolder(etype);
  Path rval;
  if (hasExtension()) {
    rval._pathstring = tmp._pathstring + getName() + "." + getExtension();
  } else {
    rval._pathstring = tmp._pathstring + getName();
  }

  if (etype == EPATHTYPE_NATIVE)
    etype = EPATHTYPE_POSIX;

  rval.eatDoubleSlashes();

  switch (etype) {
    case EPATHTYPE_POSIX: {
      // Replace backslash with forward slash
      for (size_t i = 0; i < rval._pathstring.length(); i++) {
        if (rval._pathstring[i] == '\\') {
          rval._pathstring[i] = '/';
        }
      }
      rval.computeMarkers('/');
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
  //printf("Path::toAbsolute (end) inp<%s> out<%s> tmp<%s>\n", this->c_str(), rval.c_str(), tmp.c_str());
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
// replace redundant double-slashes with slashes
///////////////////////////////////////////////////////////////////////////////

void Path::eatDoubleSlashes() {
  bool keep_going    = true;
  auto& str_contents = _pathstring;
  size_t from        = 0;
  while (keep_going) {
    size_t it_doubleslash = str_contents.find("//", from);
    if (it_doubleslash == std::string::npos) {
      // not found...
      keep_going = false;
    } else if (it_doubleslash == 0) { // leading double slash
      // printf( "a0<%s>\n", str_contents.c_str());
      str_contents = str_contents.substr(1, str_contents.size() - 1);
      // printf( "a1<%s>\n", str_contents.c_str());
    } else if (it_doubleslash > 0) { // not leading
      int prev = it_doubleslash - 1;
      if (str_contents.c_str()[prev] == ':') { // do we have a preceding colon ?
        // if so, skip
        from = it_doubleslash + 1;
      } else { // no preceding slash, remove...
        from = it_doubleslash;
        // printf( "b0<%s> from<%zu>\n", str_contents.c_str(), from);
        auto prefix  = str_contents.substr(0, from);
        auto suffix  = str_contents.substr(from + 1, str_contents.size() - (from + 1));
        str_contents = prefix + suffix;
        // printf( "b1 prefix<%s> suffix<%s> total<%s>\n", prefix.c_str(), suffix.c_str(), str_contents.c_str());
      }
    } else {
      OrkAssert(false);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

Path Path::toAbsoluteFolderX() const{
  namespace bfs = boost::filesystem;
  if(hasUrlBase()){
    return toAbsoluteFolder();
  }
  else{
    auto as_bfs = toBFS();
    auto as_abs = bfs::absolute(as_bfs);
    if(bfs::exists(as_abs) and bfs::is_regular_file(as_abs)){
      // if the path is a file, we need to strip the filename
      as_abs = as_abs.parent_path();
      // if path ends with /. or /.., we need to strip that
      
      while(as_abs.string().back() == '.'){
        as_abs = as_abs.parent_path();
      }
      bool keep_removing_parent = true;
      while(keep_removing_parent){
        auto it_end = as_abs.string().find("..");
        bool is_at_end = (it_end == as_abs.string().size() - 2);
        if(is_at_end){
          as_abs = as_abs.parent_path().parent_path();
        }
        else{
          keep_removing_parent = false;
        }
      }
    }
    Path rval;
    rval.fromBFS(as_abs);
    return rval;
  }
  return Path();
}

///////////////////////////////////////////////////////////////////////////////

Path Path::toAbsoluteFolder(EPathType etype) const {
  // printf( " Path::toAbsoluteFolder (begin) inp<%s>\n", this->c_str()  );

  if (etype == EPATHTYPE_NATIVE)
    etype = Path::GetNative();

  Path rval;

  if (hasUrlBase()) {
    std::string urlbase  = getUrlBase().c_str();
    auto urictx          = ork::FileEnv::contextForUriProto(urlbase.c_str());
    auto basepath        = urictx->getFilesystemBaseAbs();
    std::string thispath = this->c_str();

    std::string stripped = string::replaced(thispath, urlbase, "");

    auto path = basepath; // / stripped;
    if(0)printf(
        " Path::toAbsoluteFolder urlbase<%s> basepath<%s> stripped<%s> pstr<%s>\n", //
        urlbase.c_str(),
        basepath.c_str(),
        stripped.c_str(),
        path.c_str());
    size_t ilen = strlen(path.c_str());

    bool b_ends_with_slash = path.c_str()[ilen - 1] == '/';

    rval._pathstring = b_ends_with_slash ? path.c_str() : std::string(path.c_str()) + "/";
  } else if (isAbsolute()) {
    switch (etype) {
      case EPATHTYPE_NATIVE:
      case EPATHTYPE_URL:
        break;
      case EPATHTYPE_POSIX: {
        rval._pathstring = "/";
        break;
      }
    }
  } else {
    switch (etype) {
      case EPATHTYPE_POSIX:
      case EPATHTYPE_NATIVE:
      case EPATHTYPE_URL:
        break;
    }
  }

  rval._pathstring += getFolder(etype); // getFolder already returns posix format
  switch (etype) {
    default: {
      rval.computeMarkers('/');
      break;
    }
  }
  // printf( " Path::toAbsoluteFolder (end) inp<%s> AbsoluteFolder<%s>\n", this->c_str(), rval.c_str() );
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

Path::SmallNameType Path::getUrlBase() const {
  if (_markers.mUrlBaseLen == 0) {
    return std::string();
  }
  int ibas = int(_markers.getUrlBase());
  int ilen = int(_markers.mUrlBaseLen);
  return _pathstring.substr(ibas, ilen);
}

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

Path::NameType Path::getName() const {
  if (_markers.mFileNameLen == 0) {
    return std::string();
  }
  int ibas = int(_markers.getFileNameBase());
  int ilen = int(_markers.mFileNameLen);
  return _pathstring.substr(ibas, ilen);
}

///////////////////////////////////////////////////////////////////////////////

Path::SmallNameType Path::getExtension() const {
  if (_markers.mExtensionLen == 0) {
    return std::string();
  }
  int ibas = int(_markers.getExtensionBase());
  int ilen = int(_markers.mExtensionLen);
  return _pathstring.substr(ibas, ilen);
}

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

Path::NameType Path::getFolder(EPathType etype) const {
  if (_markers.mFolderLen == 0) {
    return std::string();
  }
  int ibas = _markers.getFolderBase();
  int ilen = _markers.mFolderLen;
  return _pathstring.substr(ibas, ilen);
}

///////////////////////////////////////////////////////////////////////////////

Path Path::stripBasePath(const NameType& base) const {
  Path basePath(base);
  std::string thisString = this->toAbsolute(EPATHTYPE_POSIX).c_str();
  std::string baseString = basePath.toAbsolute(EPATHTYPE_POSIX).c_str();

  if (thisString.find(baseString) == 0)
    return Path(thisString.substr(baseString.length()).c_str());
  else
    return *this;
}

///////////////////////////////////////////////////////////////////////////////

// No longer needed - using standard strrchr instead

//////////////////////////////////////////////////////////////////////////////

void Path::computeMarkers(char pathsep) {
  const char* instr = c_str();
  int ilen = int(strlen(instr));

  //////////////////////////////////////////////
  // find feature markers
  const char* umark  = strstr(instr, "://");
  const char* pmark  = strrchr(instr, '.');
  const char* lsmark = strrchr(instr, pathsep);

  ////////////////////////////////////////////
  // if . before last slash, then it is a folder . and not an ext .
  ////////////////////////////////////////////
  if (pmark && lsmark && pmark < lsmark) {
    pmark = nullptr;
  }

  // Initialize all markers
  _markers.mExtensionLen   = 0;
  _markers.mFileNameLen    = 0;
  _markers.mFolderLen      = 0;
  _markers.mUrlBaseLen     = 0;

  int imarkerstart = 0;

  // Parse URL protocol if present
  if (umark) {
    _markers.mUrlBaseLen = (umark - instr) + 3;
    imarkerstart = _markers.mUrlBaseLen;
  }

  // Parse folder (everything up to and including last slash)
  if (lsmark) {
    int folder_end = (lsmark - instr) + 1;
    if (folder_end > imarkerstart) {
      _markers.mFolderLen = folder_end - imarkerstart;
      imarkerstart = folder_end;
    }
  }

  // Parse filename and extension
  int remaining = ilen - imarkerstart;
  if (remaining > 0) {
    if (pmark && pmark > (instr + imarkerstart)) {
      // Has extension
      _markers.mFileNameLen = (pmark - instr) - imarkerstart;
      _markers.mExtensionLen = ilen - (pmark - instr) - 1;
    } else {
      // No extension
      _markers.mFileNameLen = remaining;
      _markers.mExtensionLen = 0;
    }
  }

  // Verify parsing
  int itot = _markers.mUrlBaseLen + _markers.mFolderLen + _markers.mFileNameLen + 
             (_markers.mExtensionLen > 0 ? _markers.mExtensionLen + 1 : 0);

  if (itot != ilen) {
    printf("Path parsing error: path<%s> calculated<%d> actual<%d>\n", instr, itot, ilen);
    printf("  url:%d folder:%d file:%d ext:%d\n", 
           _markers.mUrlBaseLen, _markers.mFolderLen, 
           _markers.mFileNameLen, _markers.mExtensionLen);
    OrkAssert(false);
  }

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


///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

void Path::compose(const DecomposedPath& decomposed) {
  std::string str;

  if (!decomposed.mProtocol.empty())
    str += decomposed.mProtocol;
  if (!decomposed.mFolder.empty())
    str += decomposed.mFolder;
  if (!decomposed.mFile.empty())
    str += decomposed.mFile;
  if (!decomposed.mExtension.empty()) {
    str += ".";
    str += decomposed.mExtension;
  }
  set(str.c_str());
}

void Path::decompose(DecomposedPath& decomposed) {
  if (hasUrlBase()) {
    decomposed.mProtocol = getUrlBase();
  } else {
    decomposed.mProtocol.clear();
  }
  if (hasFolder()) {
    decomposed.mFolder = getFolder(EPATHTYPE_POSIX);
  } else {
    decomposed.mFolder.clear();
  }
  if (hasFile()) {
    decomposed.mFile = getName();
  } else {
    decomposed.mFile.clear();
  }
  if (hasExtension()) {
    decomposed.mExtension = getExtension();
  } else {
    decomposed.mExtension.clear();
  }
}

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

void Path::split(NameType& preq, NameType& postq, char sep) const {
  size_t sep_pos = _pathstring.rfind(sep);

  if (sep_pos != std::string::npos) {
    preq = _pathstring.substr(0, sep_pos);
    postq = _pathstring.substr(sep_pos + 1);
  } else {
    preq = _pathstring;
    postq.clear();
  }
}

///////////////////////////////////////////////////////////////////////////////

bool Path::doesPathExist() const {
  struct stat file_stat;
  int ist = stat(toAbsolute().c_str(), &file_stat);
   //printf( "stat<%s> : %d\n", c_str(), ist );
  return (ist == 0);
}
bool Path::isFile() const {
  struct stat file_stat;
  int ist = stat(toAbsolute().c_str(), &file_stat);
   //printf( "stat<%s> : %d\n", c_str(), ist );
  return (ist == 0) ? bool(S_ISREG(file_stat.st_mode)) : false;
}
bool Path::isFolder() const {
  struct stat file_stat;
  int ist = stat(toAbsolute().c_str(), &file_stat);
   //printf( "stat<%s> : %d\n", c_str(), ist );
  return (ist == 0) ? bool(((file_stat.st_mode & S_IFMT) == S_IFDIR)) : false;
}
bool Path::isSymLink() const {
  struct stat file_stat;
  int ist = stat(toAbsolute().c_str(), &file_stat);
   //printf( "stat<%s> : %d\n", c_str(), ist );
  return (ist == 0) ? bool(S_ISLNK(file_stat.st_mode)) : false;
}

Path::HashType Path::hashFileContents() const{
  if(not isFile()){
    printf( "FILE<%s> not found!\n", toAbsolute().c_str() );
  }
  OrkAssert(isFile());
  auto abs =   toAbsolute();
  File f(abs,EFM_READ);
  std::vector<uint8_t> bytes;
  size_t length = 0;
  auto status = f.Load(bytes);
  OrkAssert(EFEC_FILE_OK==status);
  U32 uval = Crc32::HashMemory(bytes.data(), length);
  return HashType(uval);
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

void Path::dump(const std::string& idstr) const{
  auto as_abs = toAbsolute();
  auto as_abs_folder = toAbsoluteFolder();
  logchan_path->log("///////////////////////////" );
  logchan_path->log("path dump idstr<%s>", idstr.c_str() );
  logchan_path->log("  rawpath<%s>", c_str() );
  logchan_path->log("  abs<%s>", as_abs.c_str() );
  logchan_path->log("  absfolder<%s>", as_abs_folder.c_str() );
  logchan_path->log("  exists<%d>", int(doesPathExist()) );
  logchan_path->log("  isfile<%d>", int(isFile()) );
  logchan_path->log("  isfolder<%d>", int(isFolder()) );
  logchan_path->log("  issymlink<%d>", int(isSymLink()) );
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
// Temporary file/directory creation
///////////////////////////////////////////////////////////////////////////////

// Generate a random string for temporary names
static std::string generate_random_string(size_t length) {
  static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
  
  std::string result;
  result.reserve(length);
  for (size_t i = 0; i < length; ++i) {
    result += charset[dis(gen)];
  }
  return result;
}

Path Path::mkdtemp(const std::string& prefix,
                   const std::string& suffix,
                   const Path& dir) {
  // Use system temp dir if no directory specified
  Path parent_dir = dir.empty() ? temp_dir() : dir;
  
  // Ensure parent directory exists
  if (!parent_dir.doesPathExist()) {
    // Try to use /tmp as fallback
    parent_dir = Path("/tmp");
    if (!parent_dir.doesPathExist()) {
      // Last resort: current directory
      parent_dir = Path(".");
    }
  }
  
  // Try to create a unique directory
  for (int attempts = 0; attempts < 100; ++attempts) {
    std::string random_part = generate_random_string(8);
    std::string dir_name = prefix + random_part + suffix;
    Path new_dir = parent_dir / dir_name;
    
    // Try to create the directory
    if (mkdir(new_dir.c_str(), 0700) == 0) {
      return new_dir;
    }
    
    // If it failed because it already exists, try again
    if (errno != EEXIST) {
      // Some other error occurred
      logchan_path->log("mkdtemp failed: %s", strerror(errno));
      break;
    }
  }
  
  // Failed to create directory
  return Path();
}

std::pair<int, Path> Path::mkstemp(const std::string& prefix,
                                   const std::string& suffix,
                                   const Path& dir) {
  // Use system temp dir if no directory specified
  Path parent_dir = dir.empty() ? temp_dir() : dir;
  
  // Ensure parent directory exists
  if (!parent_dir.doesPathExist()) {
    // Try to use /tmp as fallback
    parent_dir = Path("/tmp");
    if (!parent_dir.doesPathExist()) {
      // Last resort: current directory
      parent_dir = Path(".");
    }
  }
  
  // Try to create a unique file
  for (int attempts = 0; attempts < 100; ++attempts) {
    std::string random_part = generate_random_string(8);
    std::string file_name = prefix + random_part + suffix;
    Path new_file = parent_dir / file_name;
    
    // Try to create the file exclusively
    int fd = open(new_file.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd >= 0) {
      return std::make_pair(fd, new_file);
    }
    
    // If it failed because it already exists, try again
    if (errno != EEXIST) {
      // Some other error occurred
      logchan_path->log("mkstemp failed: %s", strerror(errno));
      break;
    }
  }
  
  // Failed to create file
  return std::make_pair(-1, Path());
}

Path Path::mktemp(const std::string& prefix,
                  const std::string& suffix,
                  const Path& dir) {
  // Use system temp dir if no directory specified
  Path parent_dir = dir.empty() ? temp_dir() : dir;
  
  // Ensure parent directory exists
  if (!parent_dir.doesPathExist()) {
    // Try to use /tmp as fallback
    parent_dir = Path("/tmp");
    if (!parent_dir.doesPathExist()) {
      // Last resort: current directory
      parent_dir = Path(".");
    }
  }
  
  // Generate a unique name without creating the file
  for (int attempts = 0; attempts < 100; ++attempts) {
    std::string random_part = generate_random_string(8);
    std::string file_name = prefix + random_part + suffix;
    Path new_file = parent_dir / file_name;
    
    // Check if the path already exists
    if (!new_file.doesPathExist()) {
      return new_file;
    }
  }
  
  // As a last resort, add timestamp
  std::stringstream ss;
  ss << prefix << generate_random_string(8) << "_" << time(nullptr) << suffix;
  return parent_dir / ss.str();
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::file
