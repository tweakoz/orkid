////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
#include <ork/hdl/vcd.h>

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
struct UiTestApp final : public OrkEzApp {
  UiTestApp(int& argc, char** argv);
  ~UiTestApp() override;
  ui::group_ptr_t _uivp;
};
using uitestapp_ptr_t = std::shared_ptr<UiTestApp>;

uitestapp_ptr_t createEZapp(int& argc, char** argv);
