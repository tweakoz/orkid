
#include <ork/application/application.h>

namespace ork::tool {
void init(char** argp);
int toolmain(int& argc, char** argv);
} // namespace ork::tool
int main(int argc, char** argv, char** argp) {

  setenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0", 1);

  class TestApplication final : public ork::Application {
  public:
    TestApplication() {
    }
    ~TestApplication() final {
    }
  };
  TestApplication the_app;
  ApplicationStack::Push(&the_app);

  ork::tool::init(argp);
  return ork::tool::toolmain(argc, argv);
}
