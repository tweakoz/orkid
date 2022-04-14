////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
#include <boost/filesystem.hpp>
#include "../kernel/string/string.h"

///////////////////////////////////////////////////////////////////////////////
/// Content addressable storage
///////////////////////////////////////////////////////////////////////////////

namespace ork::file {
//////////////////////////////////////////////////////////////////////////////
inline std::string generateContentTempPath(uint64_t key, std::string ext) {
  using namespace boost::filesystem;
  std::string temp_dir;
  temp_dir = getenv("OBT_STAGE");
  temp_dir += "/tempdir";
  if (false == exists(temp_dir)) {
    printf("Making temp_dir folder<%s>\n", temp_dir.c_str());
    create_directory(temp_dir);
  }
  auto temp_path = temp_dir + "/" + FormatString("%zx.%s", key, ext.c_str());
  return temp_path;
}
} // namespace ork::file
