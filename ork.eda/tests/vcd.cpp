////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/hdl/vcd.h>
#include <utpp/UnitTest++.h>

using namespace ork;
using namespace ork::hdl;

TEST(vcd_load) {

  std::string orkdir = getenv("ORKID_WORKSPACE_DIR");

  auto inppath = ork::file::Path(orkdir) //
                 / "ork.data"            //
                 / "tests"               //
                 / "test.vcd";

  vcd::File vcdfile;

  vcdfile.parse(inppath);
}
