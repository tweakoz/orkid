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
ui::layoutgroup_ptr_t createParamEdit(
    ui::layoutgroup_ptr_t parent, //
    std::string named,
    fvec4 color,
    const ui::anchor::Bounds& bounds,
    dspparam_ptr_t param) {
  auto paramitem = parent->makeChild<ui::LayoutGroup>(named);
  paramitem.applyBounds(bounds);
  // vp->addChild(pmxviewitem._widget);
  auto toplayout = paramitem._layout;
  auto guidevt   = toplayout->top();
  auto guidehl   = toplayout->left();
  auto guidevb   = toplayout->bottom();

  auto guideh0 = toplayout->fixedVerticalGuide(128);
  auto guideh1 = toplayout->proportionalVerticalGuide(2.0f / 6.0f);
  auto guideh2 = toplayout->proportionalVerticalGuide(3.0f / 6.0f);
  auto guideh3 = toplayout->proportionalVerticalGuide(4.0f / 6.0f);
  auto guideh4 = toplayout->proportionalVerticalGuide(5.0f / 6.0f);
  auto guidehr = toplayout->proportionalVerticalGuide(6.0f / 6.0f);
  ///////////////////////////////////////////////////////////////////////////////
  //
  auto headeritem = paramitem._widget->makeChild<ui::Label>("header", color, named);
  headeritem.applyBounds({guidevt, guidehl, guidevb, guideh0, 2});
  ///////////////////////////////////////////////////////////////////////////////
  auto coarseitem = paramitem._widget->makeChild<ui::Dial>("coarse", color);
  coarseitem.applyBounds({guidevt, guideh0, guidevb, guideh1, 2});
  coarseitem._widget->_label = FormatString("Coarse(%s)", param->_units.c_str());
  coarseitem._widget->setParams(
      param->_edit_coarse_numsteps, //
      param->_coarse,
      param->_edit_coarse_min,
      param->_edit_coarse_max,
      param->_edit_coarse_shape);
  coarseitem._widget->_onupdate = [param](float v) { //
    param->_coarse = v;
  };
  ///////////////////////////////////////////////////////////////////////////////
  auto fineitem = paramitem._widget->makeChild<ui::Dial>("fine", color);
  fineitem.applyBounds({guidevt, guideh1, guidevb, guideh2, 2});
  fineitem._widget->_label = FormatString("Fine");
  fineitem._widget->setParams(
      param->_edit_fine_numsteps, //
      param->_fine,
      param->_edit_fine_min,
      param->_edit_fine_max,
      param->_edit_fine_shape);
  fineitem._widget->_onupdate = [param](float v) { //
    param->_fine = v;
  };
  ///////////////////////////////////////////////////////////////////////////////
  auto keytrackitem = paramitem._widget->makeChild<ui::Dial>("kt", color);
  keytrackitem.applyBounds({guidevt, guideh2, guidevb, guideh3, 2});
  keytrackitem._widget->_label = "Keytrack(units/key)";
  keytrackitem._widget->setParams(
      param->_edit_keytrack_numsteps, //
      param->_keyTrack,
      param->_edit_keytrack_min,
      param->_edit_keytrack_max,
      param->_edit_keytrack_shape);
  keytrackitem._widget->_onupdate = [param](float v) { //
    param->_keyTrack = v;
  };
  ///////////////////////////////////////////////////////////////////////////////
  auto src1src  = param->_mods->_src1;
  auto src1lab  = FormatString("ModSrc<%s>", src1src ? src1src->_name.c_str() : "---");
  auto src1item = paramitem._widget->makeChild<ui::Label>("src1", color, src1lab);
  src1item.applyBounds({guidevt, guideh3, guidevb, guideh4, 2});
  ///////////////////////////////////////////////////////////////////////////////
  auto src1ditem = paramitem._widget->makeChild<ui::Dial>("src1d", color);
  src1ditem.applyBounds({guidevt, guideh4, guidevb, guidehr, 2});
  src1ditem._widget->_label = "Src1Depth";
  src1ditem._widget->setParams(202, param->_mods->_src1Depth, -1, 1, 1.0);
  src1ditem._widget->_onupdate = [param](float v) { //
    param->_mods->_src1Depth = v;
  };
  ///////////////////////////////////////////////////////////////////////////////
  return paramitem._widget;
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
hudpanel_ptr_t createPmxEditView(
    hudvp_ptr_t vp, //
    std::string named,
    fvec4 color,
    dspblkdata_ptr_t dbdata,
    const ui::anchor::Bounds& bounds) {
  auto hudpanel    = std::make_shared<HudPanel>();
  auto pmxviewitem = vp->makeChild<ui::LayoutGroup>("ymenvview", 0, 0, 0, 0);
  pmxviewitem.applyBounds(bounds);
  // vp->addChild(pmxviewitem._widget);
  auto pmxdata = std::dynamic_pointer_cast<PMXData>(dbdata);
  OrkAssert(pmxdata);
  ////////////////////////////////////////////////
  // layout Guides
  ////////////////////////////////////////////////
  auto toplayout = pmxviewitem._layout;
  auto guidevt   = toplayout->top();
  auto guidev0   = toplayout->fixedHorizontalGuide(32);
  auto guidev1   = toplayout->proportionalHorizontalGuide(1.0f / 3.0f);
  auto guidev2   = toplayout->proportionalHorizontalGuide(2.0f / 3.0f);
  auto guidevb   = toplayout->bottom();
  //
  auto guidehl = toplayout->left();
  auto guidehr = toplayout->right();
  //
  ////////////////////////////////////////////////
  //
  auto hdrstr     = FormatString("Pmx: %s", pmxdata->_name.c_str());
  auto headeritem = pmxviewitem._widget->makeChild<ui::Label>("header", color, hdrstr);
  headeritem.applyBounds({guidevt, guidehl, guidev0, guidehr, 2});
  ////////////////////////////////////////////////
  auto pitchitem = createParamEdit(
      pmxviewitem._widget, //
      "pitch",
      color,
      {guidev0, guidehl, guidev1, guidehr, 2},
      dbdata->param(0));
  //
  auto ampitem = createParamEdit(
      pmxviewitem._widget, //
      "amp",
      color,
      {guidev1, guidehl, guidev2, guidehr, 2},
      dbdata->param(1));
  //
  auto fblitem = createParamEdit(
      pmxviewitem._widget, //
      "FBL",
      color,
      {guidev2, guidehl, guidevb, guidehr, 2},
      dbdata->param(2));
  //
  return hudpanel;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
