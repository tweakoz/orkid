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
    hudvp_ptr_t vp, //
    std::string named,
    controllerdata_ptr_t envdata,
    const ui::anchor::Bounds& bounds) {
  auto hudpanel    = std::make_shared<HudPanel>();
  auto envviewitem = vp->makeChild<ui::LayoutGroup>("ymenvview", 0, 0, 0, 0);
  envviewitem.applyBounds(bounds);
  vp->addChild(envviewitem._widget);
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
  auto guideh0 = toplayout->proportionalVerticalGuide(1.0f / 5.0f);
  auto guideh1 = toplayout->proportionalVerticalGuide(2.0f / 5.0f);
  auto guideh2 = toplayout->proportionalVerticalGuide(3.0f / 5.0f);
  auto guideh3 = toplayout->proportionalVerticalGuide(4.0f / 5.0f);
  ////////////////////////////////////////////////
  auto color = fvec4(0.7, 0.1, 0.5, 1);
  //
  auto hdrstr     = FormatString("YmEnv: %s", ymenvdata->_name.c_str());
  auto headeritem = envviewitem._widget->makeChild<ui::Label>("header", color, hdrstr);
  headeritem.applyBounds({guidevt, guidehl, guidev0, guidehr, 2});
  //
  auto atktimeitem  = envviewitem._widget->makeChild<ui::Dial>("atktime", color);
  auto dc1rateitem  = envviewitem._widget->makeChild<ui::Dial>("dc1rate", color);
  auto dc1levelitem = envviewitem._widget->makeChild<ui::Dial>("dc1level", color);
  auto dc2rateitem  = envviewitem._widget->makeChild<ui::Dial>("dc2rate", color);
  auto relrateitem  = envviewitem._widget->makeChild<ui::Dial>("relrate", color);
  //
  atktimeitem.applyBounds({guidev0, guidehl, guidevb, guideh0, 2});
  dc1rateitem.applyBounds({guidev0, guideh0, guidevb, guideh1, 2});
  dc1levelitem.applyBounds({guidev0, guideh1, guidevb, guideh2, 2});
  dc2rateitem.applyBounds({guidev0, guideh2, guidevb, guideh3, 2});
  relrateitem.applyBounds({guidev0, guideh3, guidevb, guidehr, 2});
  //
  atktimeitem._widget->_label  = "AttackTime(s)";
  dc1rateitem._widget->_label  = "Decay1Rate(x)";
  dc1levelitem._widget->_label = "DecayLevel(%%)";
  dc2rateitem._widget->_label  = "Decay2Rate(x)";
  relrateitem._widget->_label  = "ReleaseRata(x)";
  //
  atktimeitem._widget->setParams(251, ymenvdata->_attackTime, 0, 4, 2.0);
  dc1rateitem._widget->setParams(1001, ymenvdata->_decay1Rate, 0.99, 0.9999, 0.1);
  dc1levelitem._widget->setParams(101, ymenvdata->_decay1Level, 0, 100, 1.0);
  dc2rateitem._widget->setParams(1001, ymenvdata->_decay2Rate, 0.99, 0.9999, 0.1);
  relrateitem._widget->setParams(1001, ymenvdata->_releaseRate, 0.99, 0.9999, 0.1);
  //
  atktimeitem._widget->_onupdate  = [ymenvdata](float v) { ymenvdata->_attackTime = v; };
  dc1rateitem._widget->_onupdate  = [ymenvdata](float v) { ymenvdata->_decay1Rate = v; };
  dc1levelitem._widget->_onupdate = [ymenvdata](float v) { ymenvdata->_decay1Level = v; };
  dc2rateitem._widget->_onupdate  = [ymenvdata](float v) { ymenvdata->_decay2Rate = v; };
  relrateitem._widget->_onupdate  = [ymenvdata](float v) { ymenvdata->_releaseRate = v; };
  ///////////////////////////////////////////////////////////////////////
  return hudpanel;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
