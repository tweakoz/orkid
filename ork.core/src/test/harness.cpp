////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/test/harness.h>
#include <utpp/UnitTest++.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/environment.h>
#include <ork/file/path.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/string/deco.inl>
#include <unistd.h>
#include <regex>

using namespace ork;
using namespace ork::deco;
namespace ork::test {

int harness(
    int argc, //
    char** argv,
    char** envp,
    const char* test_exename,
    test_setupfn_t scoped_test_init_lambda) {
  ork::SetCurrentThreadName("main");
  /////////////////////////////////////////////
  // get orkid root directory from environment variables
  /////////////////////////////////////////////
  ork::Environment environment_vars;
  environment_vars.init_from_envp(envp);
  /////////////////////////////////////////////
  // chdir to test data directory
  /////////////////////////////////////////////
  std::string orkid_workspace_dir;
  if (environment_vars.get("ORKID_WORKSPACE_DIR", orkid_workspace_dir)) {
    file::Path test_data_dir(orkid_workspace_dir);
    test_data_dir = test_data_dir / "ork.data" / "tests";
    chdir(test_data_dir.c_str());
  }
  /////////////////////////////////////////////
  // call specific test executable's setup lambda
  //  this may instantiate something (opaque)
  //   into scoped_var, which would then
  //  be destroyed when leaving harness()
  /////////////////////////////////////////////
  svar16_t scoped_var;
  scoped_test_init_lambda(scoped_var);
  /////////////////////////////////////////////
  int iret      = 0;
  bool testdone = false;
  Thread testthr("testthread");
  /////////////////////////////////////////////
  auto mainq   = opq::mainSerialQueue();
  auto updateq = opq::updateSerialQueue();
  auto conq    = opq::concurrentQueue();
  /////////////////////////////////////////////
  // update thread (for handling the update opq)
  /////////////////////////////////////////////
  ork::Thread updthr("updatethread");
  bool upddone = false;
  updthr.start([&](anyp data) {
    opq::TrackCurrent opqtest(updateq);
    while (false == testdone)
      updateq->Process();
    upddone = true;
  });
  /////////////////////////////////////////////
  int test_failures = 0;
  testthr.start([&](anyp data) {
    /////////////////////////////////////////////
    // default Run All Tests
    /////////////////////////////////////////////
    if (argc != 2) {
      printf("%s : usage :\n", test_exename);
      printf("<%s> list : list test names\n", test_exename);
      printf("<%s> testname : run 1 test named 'testname'\n", test_exename);
      printf("<%s> testpat! : run anytest that matches the glob 'testpat*'\n", test_exename);
      printf("<%s> all : run all tests\n", test_exename);
    }
    /////////////////////////////////////////////
    // run a single test (higher signal/noise for debugging)
    /////////////////////////////////////////////
    else if (argc == 2) {
      bool blist_tests   = (0 == strcmp(argv[1], "list"));
      bool all_tests     = (0 == strcmp(argv[1], "all"));
      bool using_pattern = (0 != strstr(argv[1], "!"));
      ////////////////////////////////
      printf("//////////////////////////////////\n");
      printf(blist_tests ? "Listing Tests\n" : "Running Tests\n");
      printf("//////////////////////////////////\n");
      ////////////////////////////////
      if (all_tests)
        return UnitTest::RunAllTests();
      ////////////////////////////////
      const char* testname           = argv[1];
      const UnitTest::TestList& List = UnitTest::Test::GetTestList();
      const UnitTest::Test* ptest    = List.GetHead();
      int itest                      = 0;
      ////////////////////////////////
      while (ptest) {
        const UnitTest::TestDetails& Details = ptest->m_details;
        auto decotestname                    = decorate(fvec3::Yellow(), Details.testName);
        if (blist_tests) {
          auto sindex = decorate(fvec3::White(), ork::FormatString("%d", itest));
          printf("%s : %s\n", sindex.c_str(), decotestname.c_str());
        } else { // run tests
          ////////////////////////////////
          bool do_test = (0 == strcmp(testname, Details.testName));
          ////////////////////////////////
          // if using a pattern
          //  match against the pattern
          ////////////////////////////////
          if (using_pattern) {
            ork::fxstring<256> pattern;
            pattern.replace(argv[1], "!", ".*");
            std::regex rgx(pattern.c_str());
            do_test = std::regex_match(Details.testName, rgx);
          }
          ////////////////////////////////
          // run the test (if selected)
          ////////////////////////////////
          if (do_test) {
            UnitTest::TestResults res;
            ptest->Run(res);
            int num_failed   = res.GetFailedTestCount();
            bool ok          = res.GetFailureCount() == 0;
            auto deco_failed = deco::format(255, 255, 0, "%d", num_failed);
            auto deco_status = ok //
                                   ? decorate(fvec3::Green() * 0.5, "PASSED")
                                   : decorate(fvec3::Red(), "FAILED");
            auto deco_string = deco::format(255, 255, 255, "Test: ");
            deco_string += decotestname;
            deco_string += deco::format(255, 255, 255, " Failures: ");
            deco_string += deco_failed;
            deco_string += deco::format(255, 255, 255, " Status: ");
            deco_string += deco_status;
            printf("%s\n", deco_string.c_str());
            test_failures += num_failed;
          }
          ////////////////////////////////
        } // } else { // run tests
        ptest = ptest->next;
        itest++;
      }
    }
    testdone = true;
    return 0;
  }); // testthr.start([&](anyp data){}())
  /////////////////////////////////////////////
  // meanwhile, back on the main thread..
  //  process main thread opq
  //  while tests are running
  /////////////////////////////////////////////
  while (false == testdone) {
    opq::TrackCurrent opqtest(mainq);
    mainq->Process();
  }
  /////////////////////////////////////////////
  // drain all opq's
  /////////////////////////////////////////////
  for (int i = 0; i < 10; i++) {
    mainq->drain();
    updateq->drain();
    conq->drain();
  }
  /////////////////////////////////////////////
  // wait for test thread exit
  /////////////////////////////////////////////
  testthr.join();
  /////////////////////////////////////////////
  // final summary
  /////////////////////////////////////////////
  auto deco_string = deco::format(255, 255, 255, "Total Failures: ");
  deco_string += deco::format(255, 255, 0, "%d", test_failures);
  printf("%s\n", deco_string.c_str());
  /////////////////////////////////////////////
  return -test_failures;
} // int harness(
} // namespace ork::test
