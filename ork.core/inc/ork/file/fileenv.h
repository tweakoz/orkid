////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <string>
#include <ork/orktypes.h>
#include <ork/file/efileenum.h>
#include <ork/kernel/kernel.h>
#include <ork/kernel/core/singleton.h>
#include <ork/file/path.h>
#include <ork/file/filedevcontext.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class FileDev;
class File;
struct DataBlock;

struct FileEnvDir {
  long handle;                 /* -1 for failed rewind */
  file::Path::NameType result; /* d_name null iff first time */
  file::Path::NameType name;   /* null-terminated file::Path::NameType */

  u8 info;
};

std::shared_ptr<DataBlock> datablockFromFileAtPath(const file::Path& path);

enum ELINFILEMODE {
  ELFM_NONE = 0,
  ELFM_READ,
  ELFM_WRITE,
};

///////////////////////////////////////////////////////////////////////////////

class FileEnv : public NoRttiSingleton<FileEnv> {
  // The constructor of a singleton must be private to show users that they must use GetRef instead.
  // This ugly friend declaration is a side-effect of that. The only alternative is to ditch the template
  // and just use singleton as a pattern.
  friend class NoRttiSingleton<FileEnv>;

  FileDev* mpDefaultDevice;

public:
  typedef orkmap<std::string, filedevctx_ptr_t> filedevctxmap_t;

  FileEnv();

  // These should all be forwarded to FileDev
  FileEnvDir* OpenDir(const char*);
  int CloseDir(FileEnvDir*);
  file::Path::NameType ReadDir(FileEnvDir*);
  void RewindDir(FileEnvDir*);

  const filedevctxmap_t& uriRegistry() const {
    return _filedevcontext_map;
  }

  FileDev* GetDefaultDevice() const {
    return mpDefaultDevice;
  }
  FileDev* GetDeviceForUrl(const file::Path& fileName) const;

  //////////////////////////////////////////

  void SetDefaultDevice(FileDev* pDevice) {
    mpDefaultDevice = pDevice;
  }

  //////////////////////////////////////////
  // Caps And Flags

  static bool CanRead(void);
  static bool CanWrite(void);
  static bool CanReadAsync(void);

  //////////////////////////////////////////
  // misc support

  static bool filespec_isunix(const file::Path::NameType& inspec);
  static bool filespec_isdos(const file::Path::NameType& inspec);

  static file::Path::NameType filespec_to_extension(const file::Path::NameType& inspec);
  static file::Path::NameType filespec_no_extension(const file::Path::NameType& inspec);
  static file::Path::NameType filespec_strip_base(const file::Path::NameType& inspec, const file::Path::NameType& base);

  static orkvector<file::Path::NameType> filespec_separate_terms(const file::Path::NameType& inspec);
  static orkvector<file::Path::NameType> filespec_search(const file::Path::NameType& wildcards, const ork::file::Path& initdir);
  static orkset<file::Path::NameType> filespec_search_sorted(const file::Path::NameType& wildcards, const ork::file::Path& initdir);
  static FileStampH EncodeFileStamp(int year, int month, int day, int hour, int minute, int second);
  static void DecodeFileStamp(FileStampH stamp, int& year, int& month, int& day, int& hour, int& minute, int& second);
  static FileStampH GetFileStamp(const file::Path::NameType& filespec);

  /// This is a helper function for FilespecToContainingDirectory, but hell, feel free to use it
  /// yourself. It simply finds the last occurence of any character from the set of chars that make
  /// up setOfChars, and returns the substring of stringToTruncate up until (but not including) the
  /// last char.
  ///
  /// @param stringToTruncate This is the string that will be be searching and truncating.
  /// @param setOfChars Think of this param as set of independant chars (order does not matter) to search for inside
  ///        stringToTruncate.
  /// @return The truncated string from the first char up until the last char found from setOfChars. If no
  ///         char is found, then stringToTruncate is returned.
  static file::Path::NameType
  TruncateAtFirstCharFromset(const file::Path::NameType& stringToTruncate, const file::Path::NameType& setOfChars);

  /// For a given path, this function returns the folder path which contains the represented file. If path is
  /// a directory then the function will return path. Trailing separators ('/' or '\\') will be stripped. The
  /// function works for UNIX or DOS filenames.
  ///
  /// @param path The string on which to find the containing folder.
  /// @return The containing folder (does not contain a trailing separator).
  /// @pre filespec_isunix(path) || filespec_isdos(path)
  static file::Path::NameType FilespecToContainingDirectory(const file::Path::NameType& path);

  //////////////////////////////////////////

  static bool DoesFileExist(const file::Path& filespec);
  static bool DoesDirectoryExist(const file::Path& filespec);

  static bool IsFileWritable(const file::Path& filespec);

  static void SetPrependFilesystemBase(bool setting);
  static bool GetPrependFilesystemBase(void);
  static void SetFilesystemBase(file::Path::NameType fbase);
  static const file::Path::NameType& GetFilesystemBase(void);

  //////////////////////////////////////////

  static filedevctx_constptr_t contextForUriProto(const std::string& UriProto);
  static filedevctx_ptr_t createContextForUriBase( //
      const std::string& uriproto,        // uri protocol "lev2://", "data://", etc..
      const file::Path& base_location);   // base folder on disk

  //////////////////////////////////////////

  /// Determine is a path has a URL at the front. Note that is does not check if
  /// the URL is registered. This function is purely checking for the *:// pattern
  /// at the front of the path. Use IsUrlBaseRegister to find out if it's actually
  /// registered.
  ///
  /// @param PathName The path to check for a URL
  /// @return Whether or not PathName has a URL
  static bool PathIsUrlForm(const file::Path& PathName);

  static bool IsCharAlpha(char c) {
    return (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')));
  }

  static bool IsCharNumeric(char c) {
    return (((c >= '0') && (c <= '9')));
  }

  static bool IsCharPunc(char c) {
    return ((c == '_') || (c == '.') || (c == ' '));
  }

  static bool IsCharUnix(char c) {
    return ((c == '/') || IsCharAlpha(c) || IsCharNumeric(c) || IsCharPunc(c));
  }
  static bool IsCharDos(char c) {
    return ((c == '\\') || (c == ':') || IsCharAlpha(c) || IsCharNumeric(c) || IsCharPunc(c));
  }

  static file::Path uriProtoToBase(const std::string& uriproto);
  static file::Path uriProtoToPath(const std::string& uriproto);

private:
  /// Strips any URL from a path. The URL does not have to be regsitered.
  ///
  /// @param urlName The path to strip a URL from
  /// @return The path without the URL
  static file::Path::NameType StripUrlFromPath(const file::Path::NameType& urlName);
  filedevctxmap_t _filedevcontext_map;
};

typedef void (*FileAsyncDoneCallback)(void);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
