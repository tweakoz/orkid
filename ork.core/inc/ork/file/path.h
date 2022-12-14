////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/tempstring.h>

namespace boost::filesystem {
class path;
}
///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class PoolString;
class PieceString;

///////////////////////////////////////////////////////////////////////////////
namespace file {
///////////////////////////////////////////////////////////////////////////////

class Path;

class PathMarkers {
  friend class Path;

  unsigned int mDriveLen : 2;       // 2
  unsigned int mUrlBaseLen : 5;     // 7
  unsigned int mFolderLen : 8;      // 15
  unsigned int mFileNameLen : 8;    // 23
  unsigned int mExtensionLen : 8;   // 27
  unsigned int mQueryStringLen : 8; // 32

public:
  unsigned int GetDriveBase() const;
  unsigned int GetUrlBase() const;
  unsigned int GetFolderBase() const;
  unsigned int GetFileNameBase() const;
  unsigned int GetExtensionBase() const;
  unsigned int GetQueryStringBase() const;

  unsigned int GetDriveLength() const {
    return mDriveLen;
  }
  unsigned int GetUrlLength() const {
    return mUrlBaseLen;
  }
  unsigned int GetFolderLength() const {
    return mFolderLen;
  }
  unsigned int GetFileNameLength() const {
    return mFileNameLen;
  }
  unsigned int GetExtensionLength() const {
    return mExtensionLen;
  }
  unsigned int GetQueryStringLength() const {
    return mQueryStringLen;
  }

  PathMarkers();
};

//////////////////////////////////////////////////////////

struct DecomposedPath {
  typedef FixedString<256> string_t;

  string_t mProtocol;
  string_t mHostname;
  string_t mPort;

  string_t mDrive;
  string_t mFolder;
  string_t mFile;
  string_t mExtension;
  string_t mQuery;
};

///////////////////////////////////////////////////////////////////////////////

class Path {
public:
  typedef U32 HashType;

  typedef FixedString<32> SmallNameType;
  typedef FixedString<256> NameType;

  enum EPathType {
    EPATHTYPE_NATIVE = 0,
    EPATHTYPE_DOS,
    EPATHTYPE_POSIX,
    EPATHTYPE_NDS,

    EPATHTYPE_URL,
    EPATHTYPE_ASSET = EPATHTYPE_URL,
  };

  Path();
  Path(const char* pathName);
  Path(const std::string pathName);
  explicit Path(const PieceString& pathName);
  explicit Path(const boost::filesystem::path& p);
  explicit Path(const NameType& pathName);
  explicit Path(const ork::PoolString& pathName);
  explicit Path(const std::vector<std::string>& pathVect);

  ~Path();

  //////////////////////////////////////////////

  void operator=(const Path& oth);
  bool operator==(const Path& oth) const;
  bool operator!=(const Path& oth) const;
  void operator+=(const Path& oth);
  bool operator<(const Path& oth) const;
  Path operator+(const Path& oth) const;
  Path operator/(const Path& rhs) const;
  size_t length() const;
  bool empty() const;

  //////////////////////////////////////////////

  static EPathType GetNative();

  void setFile(const char* filename);
  void appendFile(const char* filename);
  void setFolder(const char* pathName);
  void appendFolder(const char* filename);
  void setExtension(const char* ext);
  void setUrlBase(const char* UrlBase);
  void setDrive(const char* UrlBase);

  void set(const char* pathName);

  bool isAbsolute() const;
  bool isRelative() const;
  bool hasUrlBase() const;
  bool hasFolder() const;
  bool hasDrive() const;
  bool hasQueryString() const;
  bool hasExtension() const;
  bool hasFile() const;

  //////////////////////////////////////////////

  void decompose(SmallNameType& url, SmallNameType& drive, NameType& folder, NameType& file, SmallNameType& ext, NameType& query);

  void compose(
      const SmallNameType& url,
      const SmallNameType& drive,
      const NameType& folder,
      const NameType& file,
      const SmallNameType& ext,
      const NameType& query);

  void decompose(DecomposedPath& decomposed);
  void compose(const DecomposedPath& decomposed);

  void computeMarkers(char pathsep);

  //////////////////////////////////////////////

  void SplitQuery(NameType& BeforeQuerySep, NameType& AfterQuerySep) const;
  void Split(NameType& BeforeQuerySep, NameType& AfterQuerySep, char sep) const;

  //////////////////////////////////////////////

  Path ToRelative(EPathType etype = EPATHTYPE_NATIVE) const;
  Path ToAbsolute(EPathType etype = EPATHTYPE_NATIVE) const;
  Path ToAbsoluteFolder(EPathType etype = EPATHTYPE_NATIVE) const;

  //////////////////////////////////////////////

  SmallNameType GetDrive() const;
  SmallNameType GetExtension() const;
  SmallNameType GetUrlBase() const;

  NameType GetName() const;
  NameType GetQueryString() const;
  NameType GetFolder(EPathType etype) const;

  Path StripBasePath(const NameType& base) const;

  const char* c_str() const;
  std::string toStdString() const;

  boost::filesystem::path toBFS() const;
  void fromBFS(const boost::filesystem::path& p);
  HashType Hash() const;

  //////////////////////////////////////

  void eatDoubleSlashes();

  //////////////////////////////////////

  bool doesPathExist() const;
  bool isFile() const;
  bool isFolder() const;
  bool isSymLink() const;

  //////////////////////////////////////

  void dump(const std::string& idstr) const;

  //////////////////////////////////////

  static Path orkroot_dir();
  static Path stage_dir();
  static Path bin_dir();
  static Path lib_dir();
  static Path dblockcache_dir();
  static Path share_dir();
  static Path temp_dir();
  static Path data_dir();


private:
  //////////////////////////////////////

  NameType _pathstring;
  PathMarkers _markers;

  //////////////////////////////////////
};

using path_ptr_t = std::shared_ptr<Path>;

///////////////////////////////////////////////////////////////////////////////
} // namespace file
///////////////////////////////////////////////////////////////////////////////

typedef file::Path AssetPath;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
