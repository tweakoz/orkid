////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if 1
#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/application/application.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/init.h>
#include <utpp/UnitTest++.h>
#include <ork/file/file.h>

using namespace ork;

extern void WaitForOpqExit();

namespace ork::lev2 {
void ClassInit();
void GfxInit(const std::string& gfxlayer);
} // namespace ork::lev2

class TestApplication : public ork::Application {
  RttiDeclareConcrete(TestApplication, ork::Application);
};

void TestApplication::Describe() {
}

INSTANTIATE_TRANSPARENT_RTTI(TestApplication, "TestApplication");

int main(int argc, char** argv) {
  ork::SetCurrentThreadName("main");

  TestApplication the_app;
  ApplicationStack::Push(&the_app);

  static ork::lev2::StdFileSystemInitalizer filesysteminit(argc, argv);

  ork::lev2::ClassInit();
  ork::rtti::Class::InitializeClasses();
  ork::lev2::GfxInit("");

  /////////////////////////////////////////////
  ork::Thread testthr("testthread");
  int iret      = 0;
  bool testdone = false;
  testthr.start([&](anyp data) {
    /////////////////////////////////////////////
    // default Run All Tests
    /////////////////////////////////////////////
    if (argc != 2) {
      printf("ork.lev2 unit test : usage :\n");
      printf("<exename> list : list test names\n");
      printf("<exename> testname : run 1 test named\n");
      printf("<exename> all : run all tests\n");
    }
    /////////////////////////////////////////////
    // run a single test (higher signal/noise for debugging)
    /////////////////////////////////////////////
    else if (argc == 2) {
      bool blist_tests     = (0 == strcmp(argv[1], "list"));
      bool all_tests       = (0 == strcmp(argv[1], "all"));
      const char* testname = argv[1];

      if (all_tests) {
        UnitTest::RunAllTests();
        testdone = true;
        return;
      }

      if (blist_tests) {
        printf("//////////////////////////////////\n");
        printf("Listing Tests\n");
        printf("//////////////////////////////////\n");
      }

      auto test_list = UnitTest::Test::GetTestList();
      auto ptest     = test_list.GetHead();
      int itest      = 0;

      while (ptest) {
        auto& Details = ptest->m_details;

        if (blist_tests) {
          printf("Test<%d:%s>\n", itest, Details.testName);
        } else if (0 == strcmp(testname, Details.testName)) {
          printf("Running Test<%s>\n", Details.testName);
          UnitTest::TestResults testResults;
          ptest->Run(testResults);
        }
        ptest = ptest->next;
        itest++;
      }
    }
    printf("//////////////////////////////////\n");
    printf("Tests finished!\n");
    printf("//////////////////////////////////\n");
    testdone = true;
  });
  /////////////////////////////////////////////
  ork::Thread updthr("updatethread");
  bool upddone = false;
  updthr.start([&](anyp data) {
    opq::TrackCurrent opqtest(&opq::updateSerialQueue());
    while (false == testdone)
      opq::updateSerialQueue().Process();
    upddone = true;
  });
  /////////////////////////////////////////////
  while (false == testdone) {
    opq::TrackCurrent opqtest(&opq::mainSerialQueue());
    opq::mainSerialQueue().Process();
  }
  /////////////////////////////////////////////

  opq::mainSerialQueue().drain();
  opq::updateSerialQueue().drain();

  ApplicationStack::Pop();

  return iret;
}
#endif
