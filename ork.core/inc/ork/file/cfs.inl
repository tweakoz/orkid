#pragma once
#include <boost/filesystem.hpp>
#include "../kernel/environment.h"
#include "../kernel/string/string.h"

namespace ork::file {
//////////////////////////////////////////////////////////////////////////////
inline std::string generateContentTempPath(uint64_t key,std::string ext) {
  using namespace boost::filesystem;
  std::string temp_dir;
  genviron.get("OBT_STAGE", temp_dir);
  temp_dir = temp_dir + "/tempdir";
  if (false == exists(temp_dir)) {
    printf("Making temp_dir folder<%s>\n", temp_dir.c_str());
    create_directory(temp_dir);
  }
  auto temp_path = temp_dir + "/" + FormatString("%zx.%s", key,ext.c_str());
  return temp_path;
}
}
