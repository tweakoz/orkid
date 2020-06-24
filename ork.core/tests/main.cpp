////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/test/harness.h>
#include <ork/application/application.h>
#include <utpp/UnitTest++.h>
#include "reflectionclasses.inl"

using namespace ork;

///////////////////////////////////////////////////////////
// minimal init, just create an app, put it on the appstack
//  and initialize the reflection system
///////////////////////////////////////////////////////////

struct TestApplication final : public Application {
  TestApplication() {
    ApplicationStack::Push(this);

    SharedTest::GetClassStatic();

    rtti::Class::InitializeClasses();
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
      "ork.core-unittests",
      [=](svar16_t& scoped_var) { //
        // instantiate a TestApplication on the harness's stack
        scoped_var.makeShared<TestApplication>();
      });
}
