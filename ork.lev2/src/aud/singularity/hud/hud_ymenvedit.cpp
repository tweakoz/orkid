////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/ui/box.h>
#include <ork/lev2/ui/label.h>
#include <ork/lev2/ui/dial.h>

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
hudpanel_ptr_t createEnvYmEditView(
    uilayoutgroup_ptr_t vp, //
    std::string named,
    fvec4 color,
    controllerdata_ptr_t envdata,
    const ui::anchor::Bounds& bounds) {
  auto hudpanel    = std::make_shared<HudPanel>();
  auto envviewitem = vp->makeChild<ui::LayoutGroup>("ymenvview", 0, 0, 0, 0);
  envviewitem.applyBounds(bounds);
  vp->addChild(envviewitem.typedWidget());
  auto ymenvdata = std::dynamic_pointer_cast<YmEnvData>(envdata);
  OrkAssert(ymenvdata);
  ////////////////////////////////////////////////
  // layout Guides
  ////////////////////////////////////////////////
  auto toplayout = envviewitem._layout;
  auto guidevt   = toplayout->top();
  auto guidehl   = toplayout->left();
  auto guidevb   = toplayout->bottom();
  auto guidehr   = toplayout->right();
  //
  auto guidev0 = toplayout->fixedHorizontalGuide(32);
  auto guidev1 = toplayout->fixedHorizontalGuide(-32);
  //
  auto guideh0 = toplayout->proportionalVerticalGuide(1.0f / 6.0f);
  auto guideh1 = toplayout->proportionalVerticalGuide(2.0f / 6.0f);
  auto guideh2 = toplayout->proportionalVerticalGuide(3.0f / 6.0f);
  auto guideh3 = toplayout->proportionalVerticalGuide(4.0f / 6.0f);
  auto guideh4 = toplayout->proportionalVerticalGuide(5.0f / 6.0f);
  ////////////////////////////////////////////////
  //
  auto hdrstr     = FormatString("YmEnv: %s", ymenvdata->_name.c_str());
  auto headeritem = envviewitem.typedWidget()->makeChild<ui::Label>("header", color, hdrstr);
  headeritem.applyBounds({guidevt, guidehl, guidev0, guidehr, 2});
  //
  auto atkshapeitem = envviewitem.typedWidget()->makeChild<ui::Dial>("atkshape", color);
  auto atktimeitem  = envviewitem.typedWidget()->makeChild<ui::Dial>("atktime", color);
  auto dc1rateitem  = envviewitem.typedWidget()->makeChild<ui::Dial>("dc1rate", color);
  auto dc1levelitem = envviewitem.typedWidget()->makeChild<ui::Dial>("dc1level", color);
  auto dc2rateitem  = envviewitem.typedWidget()->makeChild<ui::Dial>("dc2rate", color);
  auto relrateitem  = envviewitem.typedWidget()->makeChild<ui::Dial>("relrate", color);
  //
  atkshapeitem.applyBounds({guidev0, guidehl, guidevb, guideh0, 2});
  atktimeitem.applyBounds({guidev0, guideh0, guidevb, guideh1, 2});
  dc1rateitem.applyBounds({guidev0, guideh1, guidevb, guideh2, 2});
  dc1levelitem.applyBounds({guidev0, guideh2, guidevb, guideh3, 2});
  dc2rateitem.applyBounds({guidev0, guideh3, guidevb, guideh4, 2});
  relrateitem.applyBounds({guidev0, guideh4, guidevb, guidehr, 2});
  //
  atkshapeitem.typedWidget()->_label = "AttackShape";
  atktimeitem.typedWidget()->_label  = "AttackRate(x)";
  dc1rateitem.typedWidget()->_label  = "Decay1Rate(x)";
  dc1levelitem.typedWidget()->_label = "DecayLevel";
  dc2rateitem.typedWidget()->_label  = "Decay2Rate(x)";
  relrateitem.typedWidget()->_label  = "ReleaseRata(x)";
  atktimeitem.typedWidget()->_font   = "i13";
  dc1rateitem.typedWidget()->_font   = "i13";
  dc1levelitem.typedWidget()->_font  = "i13";
  dc2rateitem.typedWidget()->_font   = "i13";
  relrateitem.typedWidget()->_font   = "i13";
  //
  atkshapeitem.typedWidget()->setParams(251, ymenvdata->_attackShape, 0, 10, 2.0);
  atktimeitem.typedWidget()->setParams(251, ymenvdata->_attackRate, 0, 60, 2.0);
  dc1rateitem.typedWidget()->setParams(1001, ymenvdata->_decay1Rate, 0.99, 0.9999, 0.1);
  dc1levelitem.typedWidget()->setParams(101, ymenvdata->_decay1Level, 0, 1, 1.0);
  dc2rateitem.typedWidget()->setParams(1001, ymenvdata->_decay2Rate, 0.99, 0.9999, 0.1);
  relrateitem.typedWidget()->setParams(1001, ymenvdata->_releaseRate, 0.99, 0.9999, 0.1);
  //
  atkshapeitem.typedWidget()->_onupdate = [ymenvdata](float v) { ymenvdata->_attackShape = v; };
  atktimeitem.typedWidget()->_onupdate  = [ymenvdata](float v) { ymenvdata->_attackRate = v; };
  dc1rateitem.typedWidget()->_onupdate  = [ymenvdata](float v) { ymenvdata->_decay1Rate = v; };
  dc1levelitem.typedWidget()->_onupdate = [ymenvdata](float v) { ymenvdata->_decay1Level = v; };
  dc2rateitem.typedWidget()->_onupdate  = [ymenvdata](float v) { ymenvdata->_decay2Rate = v; };
  relrateitem.typedWidget()->_onupdate  = [ymenvdata](float v) { ymenvdata->_releaseRate = v; };
  ///////////////////////////////////////////////////////////////////////
  return hudpanel;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
