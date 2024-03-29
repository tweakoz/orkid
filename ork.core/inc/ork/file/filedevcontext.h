////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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


  FileDevContext();
  FileDevContext(const FileDevContext& oth);

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
};

typedef std::shared_ptr<FileDevContext> filedevctx_ptr_t;
typedef std::shared_ptr<const FileDevContext> filedevctx_constptr_t;

} // namespace ork
