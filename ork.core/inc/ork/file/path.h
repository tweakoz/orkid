////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/orktypes.h>
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

struct CatalogComponents {
  std::string _namespace;
  std::string _asset;
  
  bool isValid() const { 
    return !_namespace.empty() && !_asset.empty(); 
  }
};

class PathMarkers {
  friend class Path;

  unsigned int mUrlBaseLen : 7;     // 0-127 chars for protocol
  unsigned int mFolderLen : 10;     // 0-1023 chars for folder
  unsigned int mFileNameLen : 8;    // 0-255 chars for filename
  unsigned int mExtensionLen : 7;   // 0-127 chars for extension

public:
  unsigned int getUrlBase() const;
  unsigned int getFolderBase() const;
  unsigned int getFileNameBase() const;
  unsigned int getExtensionBase() const;
  unsigned int getUrlLength() const {
    return mUrlBaseLen;
  }
  unsigned int getFolderLength() const {
    return mFolderLen;
  }
  unsigned int getFileNameLength() const {
    return mFileNameLen;
  }
  unsigned int getExtensionLength() const {
    return mExtensionLen;
  }

  PathMarkers();
};

//////////////////////////////////////////////////////////

struct DecomposedPath {
  typedef std::string string_t;

  string_t mProtocol;   // e.g. "data://"
  string_t mFolder;     // e.g. "/path/to/"
  string_t mFile;       // e.g. "filename"
  string_t mExtension;  // e.g. "txt"
};

///////////////////////////////////////////////////////////////////////////////

struct PathSanitizeOptions {
  bool use_env_vars = true;      // Replace paths with env var names
  bool hide_secrets = true;      // Hide paths containing sensitive info
  bool use_project_dirs = true;  // Replace project paths with project env vars
  bool abbreviate_home = true;   // Replace home dir with ~
};

using pathsanitizeoptions_ptr_t = std::shared_ptr<PathSanitizeOptions>;

///////////////////////////////////////////////////////////////////////////////

class Path {
public:
  typedef U32 HashType;

  typedef std::string SmallNameType;
  typedef std::string NameType;

  enum EPathType {
    EPATHTYPE_NATIVE = 0,
    EPATHTYPE_POSIX,
    EPATHTYPE_URL,
    EPATHTYPE_ASSET = EPATHTYPE_URL,
  };

  Path();
  Path(const char* pathName);
  Path(const std::string pathName);
  explicit Path(const PieceString& pathName);
  explicit Path(const boost::filesystem::path& p);
  // explicit Path(const NameType& pathName); // Removed - NameType is now std::string
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

  void set(const char* pathName);

  bool isAbsolute() const;
  bool isRelative() const;
  bool hasUrlBase() const;
  bool hasFolder() const;
  bool hasExtension() const;
  bool hasFile() const;

  //////////////////////////////////////////////

  // Removed 6-parameter decompose/compose - use DecomposedPath versions instead

  void decompose(DecomposedPath& decomposed);
  void compose(const DecomposedPath& decomposed);

  void computeMarkers(char pathsep);

  //////////////////////////////////////////////

  void split(NameType& BeforeQuerySep, NameType& AfterQuerySep, char sep) const;

  //////////////////////////////////////////////

  Path toRelative(EPathType etype = EPATHTYPE_NATIVE) const;
  Path toAbsolute(EPathType etype = EPATHTYPE_NATIVE) const;
  Path toAbsoluteFolder(EPathType etype = EPATHTYPE_NATIVE) const;
  Path toAbsoluteFolderX() const;
  Path resolveRelativeTo(const Path& basePath) const;

  //////////////////////////////////////////////

  SmallNameType getExtension() const;
  SmallNameType getUrlBase() const;

  NameType getName() const;
  NameType getFolder(EPathType etype) const;

  Path stripBasePath(const NameType& base) const;
  Path withExtension(const char* new_ext) const;

  const char* c_str() const;
  std::string toStdString() const;

  boost::filesystem::path toBFS() const;
  void fromBFS(const boost::filesystem::path& p);
  HashType hash() const;

  //////////////////////////////////////

  void eatDoubleSlashes();

  //////////////////////////////////////

  bool doesPathExist() const;
  inline bool exists() const { return doesPathExist(); }
  bool isFile() const;
  bool isFolder() const;
  bool isSymLink() const;
  HashType hashFileContents() const;
  
  // Asset catalog path detection
  bool isAssetCatalogPath() const;
  bool isFilePath() const;
  CatalogComponents getCatalogComponents() const;
  std::string getCatalogNamespace() const;
  std::string getCatalogAssetId() const;
  
  // Directory creation
  bool ensureDirectoryExists() const;

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

  //////////////////////////////////////
  // Path expansion
  //////////////////////////////////////

  // Expand a path string with all supported tokens:
  //   ~                      → home directory
  //   <assetcache>           → ${OBT_STAGE}/assetcache
  //   ${ENV_VAR}             → environment variable lookup
  static std::string expandPathString(const std::string& path);
  
  //////////////////////////////////////
  // Path sanitization
  //////////////////////////////////////
  
  // Sanitize path for display/logging by replacing with env vars where possible
  // Returns sanitized path (e.g., with env var substitutions)
  Path sanitize(pathsanitizeoptions_ptr_t opts = nullptr) const;
  
  //////////////////////////////////////
  // Temporary file/directory creation
  //////////////////////////////////////
  
  // Create a temporary directory (like Python's tempfile.mkdtemp)
  // Returns path to the created directory
  // prefix: optional prefix for the directory name
  // suffix: optional suffix for the directory name
  // dir: optional parent directory (defaults to system temp dir)
  static Path mkdtemp(const std::string& prefix = "ork_",
                      const std::string& suffix = "",
                      const Path& dir = Path());
  
  // Create a temporary file (like Python's tempfile.mkstemp)
  // Returns pair of (file descriptor, path to the created file)
  // prefix: optional prefix for the file name
  // suffix: optional suffix for the file name
  // dir: optional parent directory (defaults to system temp dir)
  static std::pair<int, Path> mkstemp(const std::string& prefix = "ork_",
                                      const std::string& suffix = ".tmp",
                                      const Path& dir = Path());
  
  // Create a named temporary file path (without creating the file)
  // Similar to Python's tempfile.NamedTemporaryFile but doesn't create the file
  // Returns a unique path that can be used for a temporary file
  static Path mktemp(const std::string& prefix = "ork_",
                     const std::string& suffix = ".tmp",
                     const Path& dir = Path());


private:
  //////////////////////////////////////

  std::string _pathstring;
  PathMarkers _markers;

  //////////////////////////////////////
};

using path_ptr_t = std::shared_ptr<Path>;

///////////////////////////////////////////////////////////////////////////////
} // namespace file
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
