////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

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
struct SimpleUiApp final : public OrkEzApp {
  SimpleUiApp(appinitdata_ptr_t qid);
  ~SimpleUiApp() override;
  ui::group_ptr_t _uivp;
};
using simpleuiapp_ptr_t = std::shared_ptr<SimpleUiApp>;

simpleuiapp_ptr_t createSimpleUiApp(appinitdata_ptr_t idata);
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
///////////////////////////////////////////////////////////////////////////////
