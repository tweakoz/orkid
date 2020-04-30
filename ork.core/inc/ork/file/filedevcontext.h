///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/file/efileenum.h>
#include <ork/file/path.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class FileDev;
class File;

///////////////////////////////////////////////////////////////////////////////

class FileDevContext {
public:
  typedef bool (*path_converter_type)(file::Path& pth);

  FileDevContext();
  FileDevContext(const FileDevContext& oth);

  void AddPathConverter(path_converter_type cvr) {
    mPathConverters.push_back(cvr);
  }
  const orkvector<path_converter_type>& GetPathConverters() const {
    return mPathConverters;
  }

  void setFilesystemBaseAbs(const file::Path& base);
  const file::Path& getFilesystemBaseAbs() const {
    return _absolute_basepath;
  }

  void SetFilesystemBaseEnable(bool bv) {
    mbPrependFilesystemBase = bv;
  }
  bool GetPrependFilesystemBase() const {
    return mbPrependFilesystemBase;
  }
  void SetPrependFilesystemBase(bool bv) {
    mbPrependFilesystemBase = bv;
  }
  FileDev* GetFileDevice() const {
    return mpFileDevice;
  }
  void SetFileDevice(FileDev* pFileDevice) {
    mpFileDevice = pFileDevice;
  }

private:
  file::Path _absolute_basepath;
  bool mbPrependFilesystemBase;
  FileDev* mpFileDevice;
  orkvector<path_converter_type> mPathConverters;
};

typedef std::shared_ptr<FileDevContext> filedevctxptr_t;
typedef std::shared_ptr<const FileDevContext> const_filedevctxptr_t;

} // namespace ork
