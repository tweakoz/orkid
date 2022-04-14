////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/csystem.h>

using namespace ork;
namespace ork::tool {
int toolmain(int& argc, char** argv, char** argp);
} // namespace ork::tool
int main(int argc, char** argv, char** argp) {

  setenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0", 1);

  class TestApplication final : public ork::Application {
  public:
    TestApplication() {
    }
    ~TestApplication() final {
    }
  };
  TestApplication the_app;
  ApplicationStack::Push(&the_app);

  OldSchool::SetGlobalStringVariable("ProjectApplicationClassName", "OrkTool");

  return tool::toolmain(argc, argv, argp);
}
