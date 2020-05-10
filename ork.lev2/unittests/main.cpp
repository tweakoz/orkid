////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/test/harness.h>
#include <ork/application/application.h>
#include <utpp/UnitTest++.h>
#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/file/file.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/application/application.h>

using namespace ork;
namespace ork::lev2 {
void ClassInit();
void GfxInit(const std::string& gfxlayer);
} // namespace ork::lev2

///////////////////////////////////////////////////////////
// minimal init, just create an app, put it on the appstack
//  and initialize the reflection system
///////////////////////////////////////////////////////////

struct TestApplication final : public Application {

  lev2::stdfilesysinit_p _filesysinit;
  TestApplication(int argc, char** argv) {
    ApplicationStack::Push(this);
    _filesysinit = std::make_shared<lev2::StdFileSystemInitalizer>(argc, argv);

    lev2::ClassInit();
    rtti::Class::InitializeClasses();
    lev2::GfxInit("");
  }

  ~TestApplication() {
    ApplicationStack::Pop();
  }
};

///////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {
  return test::harness(
      argc, //
      argv,
      envp,
      "ork.lev2-unittests",
      [=](svar16_t& scoped_var) { //
        // instantiate a TestApplication on the harness's stack
        scoped_var.makeShared<TestApplication>(argc, argv);
      });
}
