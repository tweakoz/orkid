#include <QWindow>
#include <portaudio.h>
#include <ork/application/application.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/file/path.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/kernel/timer.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
struct TestViewport final : public ui::Viewport {
  TestViewport();
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void onUpdateThreadTick(ui::updatedata_ptr_t updata);
  int _updcount = 0;
};
using testvp_ptr_t = std::shared_ptr<TestViewport>;
///////////////////////////////////////////////////////////////////////////////
struct UiTestApp final : public OrkEzQtApp {
  UiTestApp(int& argc, char** argv);
  ~UiTestApp() override;
  ui::group_ptr_t _uivp;
};
using uitestapp_ptr_t = std::shared_ptr<UiTestApp>;

uitestapp_ptr_t createEZapp(
    ui::context_ptr_t uicontext, //
    int& argc,
    char** argv);
