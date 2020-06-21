#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/dsp_pmx.h>
#include <ork/lev2/ui/box.h>
#include <ork/lev2/ui/label.h>
#include <ork/lev2/ui/dial.h>

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
hudpanel_ptr_t createPmxEditView(
    hudvp_ptr_t vp, //
    std::string named,
    dspblkdata_ptr_t dbdata,
    const ui::anchor::Bounds& bounds) {
  auto hudpanel    = std::make_shared<HudPanel>();
  auto pmxviewitem = vp->makeChild<ui::LayoutGroup>("ymenvview", 0, 0, 0, 0);
  pmxviewitem.applyBounds(bounds);
  vp->addChild(pmxviewitem._widget);
  auto pmxdata = std::dynamic_pointer_cast<PMXData>(dbdata);
  OrkAssert(pmxdata);
  ////////////////////////////////////////////////
  // layout Guides
  ////////////////////////////////////////////////
  auto toplayout = pmxviewitem._layout;
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
  auto color = fvec4(0.7, 0.1, 0.5, 1);
  //
  auto hdrstr     = FormatString("Pmx: %s", pmxdata->_name.c_str());
  auto headeritem = pmxviewitem._widget->makeChild<ui::Label>("header", color, hdrstr);
  headeritem.applyBounds({guidevt, guidehl, guidev0, guidehr, 2});
  //
  return hudpanel;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
