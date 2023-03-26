////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/spawner.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/gfx/dds.h>
#include <ork/lev2/gfx/texman.h>
#include <math.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

void invoke_nvcompress(std::string inpath, std::string outpath, std::string other_args) {
  Spawner s;
#if defined(__APPLE__)
  s.mCommandLine = "/usr/local/bin/nvcompress ";
#else
  s.mEnvironment.prependPath("LD_LIBRARY_PATH", file::Path::lib_dir());
  s.mCommandLine = "nvcompress ";
#endif
  s.mCommandLine += other_args + " ";
  s.mCommandLine += inpath + std::string(" ");
  s.mCommandLine += outpath + std::string(" ");
  s.spawnSynchronous();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
