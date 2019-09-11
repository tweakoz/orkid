
#include <ork/application/application.h>

namespace ork::tool {
  void init(char**argp);
  int toolmain(int& argc, char** argv);
}
int main(int argc, char** argv, char** argp){

  class TestApplication final : public ork::Application {
  public:
    TestApplication() {}
    ~TestApplication() final {}
  };
  TestApplication the_app;
  ApplicationStack::Push(&the_app);

  ork::tool::init(argp);
  return ork::tool::toolmain(argc,argv);
}
