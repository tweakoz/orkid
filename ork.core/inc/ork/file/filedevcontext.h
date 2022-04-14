////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/file/efileenum.h>
#include <ork/file/path.h>
#include <ork/kernel/varmap.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class FileDev;
class File;

///////////////////////////////////////////////////////////////////////////////

struct FileDevContext {

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

  file::Path _absolute_basepath;
  std::string _protoid;
  varmap::VarMap _vars;
  bool mbPrependFilesystemBase;
  FileDev* mpFileDevice;
  orkvector<path_converter_type> mPathConverters;
};

typedef std::shared_ptr<FileDevContext> filedevctx_ptr_t;
typedef std::shared_ptr<const FileDevContext> filedevctx_constptr_t;

} // namespace ork
