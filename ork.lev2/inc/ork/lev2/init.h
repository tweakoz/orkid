#pragma once

#include <ork/pch.h>
#include <ork/file/file.h>

namespace ork::lev2 {

struct StdFileSystemInitalizer {
  StdFileSystemInitalizer(int argc, char** argv);
  ~StdFileSystemInitalizer();
};

} // namespace ork::lev2
