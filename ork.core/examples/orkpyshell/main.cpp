#include <ork/python/context.h>
#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/embed.h>
#include <ork/application/application.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/kernel/Thread.h>
#include <ork/kernel/opq.h>

using namespace ork;

class TestApplication : public Application {
  RttiDeclareConcrete(TestApplication, Application);
};

void TestApplication::Describe() {
}

INSTANTIATE_TRANSPARENT_RTTI(TestApplication, "TestApplication");


int main(int argc, const char** argv){

  SetCurrentThreadName("main");

  TestApplication the_app;
  ApplicationStack::Push(&the_app);

  rtti::Class::InitializeClasses();

  /////////////////////////////////////////////
  Thread testthr("testthread");
  int iret      = 0;
  bool testdone = false;
  testthr.start([&](anyp data) {
    while(false==testdone){
      usleep(1000);
    }
  });
  /////////////////////////////////////////////
  Thread updthr("updatethread");
  bool upddone = false;
  updthr.start([&](anyp data) {
    opq::TrackCurrent opqtest(&opq::updateSerialQueue());
    while (false == testdone)
      opq::updateSerialQueue().Process();
    upddone = true;
  });
  /////////////////////////////////////////////
    opq::TrackCurrent opqtest(&opq::mainSerialQueue());
    python::init();
    opq::mainSerialQueue().Process();
    auto orkmodule = python::context().orkidModule();
  static PyCompilerFlags orkpy_cf;
    orkpy_cf.cf_flags = 0;
  while (false == testdone) {
    printf("\npyork>>");
    fflush(stdout);
    PyRun_InteractiveOneFlags(stdin,"orkshell",&orkpy_cf);
  }

  return 0;
}
