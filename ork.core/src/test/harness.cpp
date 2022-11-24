////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/test/harness.h>
#include <utpp/UnitTest++.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/timer.h>
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
    appinitdata_ptr_t initdata,
    const char* test_exename,
    test_setupfn_t scoped_test_init_lambda) {
  ork::SetCurrentThreadName("main");
  /////////////////////////////////////////////
  // get orkid root directory from environment variables
  /////////////////////////////////////////////
  ork::Environment environment_vars;
  environment_vars.init_from_envp(initdata->_envp);
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
  appvar_t scoped_var;
  scoped_test_init_lambda(scoped_var);
  /////////////////////////////////////////////
  int iret      = 0;
  int exec_state = 0;
  Thread testthr("testthread");
  /////////////////////////////////////////////
  auto mainq   = opq::mainSerialQueue();
  auto updateq = opq::updateSerialQueue();
  auto conq    = opq::concurrentQueue();
  /////////////////////////////////////////////
  int test_failures = 0;
  auto run_tests = [&,initdata,test_exename](anyp data) {
    /////////////////////////////////////////////
    // default Run All Tests
    /////////////////////////////////////////////
    if (initdata->_argc != 2) {
      printf("%s : usage :\n", test_exename);
      printf("<%s> list : list test names\n", test_exename);
      printf("<%s> testname : run 1 test named 'testname'\n", test_exename);
      printf("<%s> testpat! : run anytest that matches the glob 'testpat*'\n", test_exename);
      printf("<%s> all : run all tests\n", test_exename);
    }
    /////////////////////////////////////////////
    // run a single test (higher signal/noise for debugging)
    /////////////////////////////////////////////
    else if (initdata->_argc == 2) {
      bool blist_tests   = (0 == strcmp(initdata->_argv[1], "list"));
      bool all_tests     = (0 == strcmp(initdata->_argv[1], "all"));
      bool using_pattern = (0 != strstr(initdata->_argv[1], "!"));
      ////////////////////////////////
      printf("//////////////////////////////////\n");
      printf(blist_tests ? "Listing Tests\n" : "Running Tests\n");
      printf("//////////////////////////////////\n");
      ////////////////////////////////
      if (all_tests)
        return UnitTest::RunAllTests();
      ////////////////////////////////
      const char* testname           = initdata->_argv[1];
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
            pattern.replace(initdata->_argv[1], "!", ".*");
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
        //mainq->Process();
      }
    }
    exec_state = 1;
    return 0;
  };
  testthr.start(run_tests);
  /////////////////////////////////////////////
  // update thread (for handling the update opq)
  /////////////////////////////////////////////
  int ok_to_exit_update = false;
  ork::Thread updthr("updatethread");
  bool upddone = false;
  updthr.start([&](anyp data) {
    opq::TrackCurrent opqtest(updateq);
    while (not ok_to_exit_update)
      updateq->Process();
    upddone = true;
  });
  //run_tests(anyp());
  /////////////////////////////////////////////
  // drain all opq's
  /////////////////////////////////////////////
  ork::Timer timer;
  timer.Start();

  bool done_waiting = false;

  while(not done_waiting){
    mainq->Process();
    updateq->drain();
    conq->drain();
    switch(exec_state){
      default:
      case 0:
        break;
      case 1: // test done
        exec_state = 2;
        testthr.join();
        timer.Start();
        break;
      case 2:
        exec_state = 3;
        printf( "Waiting for operation queues to drain...\n");
        break;
      case 3:
        if(timer.SecsSinceStart()>2.0f){
            done_waiting = true;
            exec_state = 4;
        }
        break;
    }
  }
  ok_to_exit_update = true;
  updthr.join();
  /////////////////////////////////////////////
  // wait for test thread exit
  /////////////////////////////////////////////
  /////////////////////////////////////////////
  // final summary
  /////////////////////////////////////////////
  auto deco_string = deco::format(255, 255, 255, "Total Failures: ");
  deco_string += deco::format(255, 255, 0, "%d", test_failures);
  printf("%s\n", deco_string.c_str());
  /////////////////////////////////////////////
  scoped_var = nullptr;
  /////////////////////////////////////////////
  return -test_failures;
} // int harness(
} // namespace ork::test
