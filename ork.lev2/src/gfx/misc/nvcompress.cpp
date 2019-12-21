////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/spawner.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/util/ddsfile.h>
#include <ork/lev2/gfx/texman.h>
#include <math.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

void invoke_nvcompress( std::string inpath,
                        std::string outpath,
                        std::string outfmt){
  Spawner s;
#if defined(__APPLE__)
  s.mCommandLine = "/usr/local/bin/nvcompress ";
#else
  s.mEnvironment.prependPath("LD_LIBRARY_PATH",file::Path::lib_dir());
  s.mCommandLine = "nvcompress ";
#endif
  s.mCommandLine += "-" + outfmt + " ";
  s.mCommandLine += inpath + std::string(" ");
  s.mCommandLine += outpath + std::string(" ");
  s.spawnSynchronous();

}

///////////////////////////////////////////////////////////////////////////////

} //namespace ork::lev2 {
