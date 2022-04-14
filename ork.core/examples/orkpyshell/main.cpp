#include <ork/python/context.h>
#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/embed.h>
#include <ork/application/application.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/properties/register.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/opq.h>

using namespace ork;

class TestApplication : public Application {
  RttiDeclareConcrete(TestApplication, Application);
};

void TestApplication::Describe() {
}

INSTANTIATE_TRANSPARENT_RTTI(TestApplication, "TestApplication");

int main(int argc, const char** argv) {

  SetCurrentThreadName("main");

  TestApplication the_app;
  StringPoolStack::Push(&the_app);

  rtti::Class::InitializeClasses();

  auto mainq   = opq::mainSerialQueue();
  auto updateq = opq::updateSerialQueue();
  /////////////////////////////////////////////
  Thread testthr("testthread");
  int iret      = 0;
  bool testdone = false;
  testthr.start([&](anyp data) {
    while (false == testdone) {
      usleep(1000);
    }
  });
  /////////////////////////////////////////////
  Thread updthr("updatethread");
  bool upddone = false;
  updthr.start([&](anyp data) {
    opq::TrackCurrent opqtest(updateq);
    while (false == testdone)
      updateq->Process();
    upddone = true;
  });
  /////////////////////////////////////////////
  opq::TrackCurrent opqtest(mainq);
  python::init();
  mainq->Process();
  static PyCompilerFlags orkpy_cf;
  orkpy_cf.cf_flags = 0;
  while (false == testdone) {
    printf("\npyork>>");
    fflush(stdout);
    PyRun_InteractiveOneFlags(stdin, "orkshell", &orkpy_cf);
  }

  return 0;
}
