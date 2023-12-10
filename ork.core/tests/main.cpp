////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
    StringPoolStack::push(_stringpoolctx);

    SimpleTest::GetClassStatic();
    AssetTest::GetClassStatic();
    EnumTest::GetClassStatic();
    MathTest::GetClassStatic();
    SharedTest::GetClassStatic();
    MapTest::GetClassStatic();
    ArrayTest::GetClassStatic();
    VectorTest::GetClassStatic();
    TheTestInterface::GetClassStatic();
    InterfaceTest::GetClassStatic();

    rtti::Class::InitializeClasses();
  }

  ~TestApplication() {
    StringPoolStack::pop();
  }
  stringpoolctx_ptr_t _stringpoolctx;

};

///////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);

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
