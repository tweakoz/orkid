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
