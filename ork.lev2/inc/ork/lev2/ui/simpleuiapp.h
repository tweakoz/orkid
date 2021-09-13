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
namespace ork::ui{
///////////////////////////////////////////////////////////////////////////////
struct SimpleUiViewport final : public ui::Viewport {
  SimpleUiViewport();
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void onUpdateThreadTick(ui::updatedata_ptr_t updata);
  int _updcount = 0;
};
using simpleuivp_ptr_t = std::shared_ptr<SimpleUiViewport>;
///////////////////////////////////////////////////////////////////////////////
struct SimpleUiApp final : public OrkEzQtApp {
  SimpleUiApp(int& argc, char** argv, AppInitData& qid);
  ~SimpleUiApp() override;
  ui::group_ptr_t _uivp;
};
using simpleuiapp_ptr_t = std::shared_ptr<SimpleUiApp>;

simpleuiapp_ptr_t createSimpleUiApp(int& argc, char** argv, AppInitData& qid);
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
///////////////////////////////////////////////////////////////////////////////
