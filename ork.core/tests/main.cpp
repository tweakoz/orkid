////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/test/harness.h>
#include <ork/application/application.h>
#include <utpp/UnitTest++.h>
#include "reflectionclasses.inl"
#include <ork/kernel/environment.h>

using namespace ork;

///////////////////////////////////////////////////////////
// minimal init, just create an app, put it on the appstack
//  and initialize the reflection system
///////////////////////////////////////////////////////////

struct TestApplication {
  TestApplication(appinitdata_ptr_t initdata) {
      _stringpoolctx = std::make_shared<StringPoolContext>();
    StringPoolStack::Push(_stringpoolctx);

    SimpleTest::GetClassStatic();
    AssetTest::GetClassStatic();
    EnumTest::GetClassStatic();
    MathTest::GetClassStatic();
    SharedTest::GetClassStatic();
    MapTest::GetClassStatic();
    ArrayTest::GetClassStatic();
    VectorTest::GetClassStatic();

    rtti::Class::InitializeClasses();
  }

  ~TestApplication() {
    StringPoolStack::Pop();
  }
  stringpoolctx_ptr_t _stringpoolctx;

};

///////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
  genviron.init_from_envp(envp);

    svar128_t var, var2;
    var.set<int>(1);
    var2 = var;

    printf( "v2<%d>\n", var2.get<int>());

   // OrkAssert(false);

  return test::harness(
      init_data,
      "ork.core-unittests",
      [=](test::appvar_t& scoped_var) { //
        // instantiate a TestApplication on the harness's stack
        scoped_var.makeShared<TestApplication>(init_data);
      });
}
