#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/dsp_pmx.h>
#include <ork/lev2/ui/box.h>
#include <ork/lev2/ui/label.h>
#include <ork/lev2/ui/dial.h>

using namespace ork;
using namespace ork::ui;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
widget_ptr_t createParamHeader(
    layoutgroup_ptr_t parent, //
    const anchor::Bounds& bounds) {
  auto headergroupitem = parent->makeChild<LayoutGroup>("");
  headergroupitem.applyBounds(bounds);

  auto toplayout = headergroupitem._layout;
  auto guidevt   = toplayout->top();
  auto guidevc   = toplayout->proportionalHorizontalGuide(0.5);
  auto guidevb   = toplayout->bottom();
  auto guideh0   = toplayout->left();
  auto guideh1   = toplayout->fixedVerticalGuide(128);
  auto guideh2   = toplayout->proportionalVerticalGuide(2.0f / 6.0f);
  auto guideh3   = toplayout->proportionalVerticalGuide(3.0f / 6.0f);
  auto guideh4   = toplayout->proportionalVerticalGuide(4.0f / 6.0f);
  auto guideh5   = toplayout->proportionalVerticalGuide(5.0f / 6.0f);
  auto guideh6   = toplayout->proportionalVerticalGuide(6.0f / 6.0f);

  auto headeritem = headergroupitem._widget->makeChild<Label>("header", fvec4(), "params");
  headeritem.applyBounds({guidevt, guideh0, guidevc, guideh6, 2});

  auto coarseitem = headergroupitem._widget->makeChild<Label>("coarse", fvec4(), "Coarse");
  coarseitem.applyBounds({guidevc, guideh1, guidevb, guideh2, 2});

  auto fineitem = headergroupitem._widget->makeChild<Label>("fine", fvec4(), "Fine");
  fineitem.applyBounds({guidevc, guideh2, guidevb, guideh3, 2});

  auto ktitem = headergroupitem._widget->makeChild<Label>("kt", fvec4(), "KeyTrack");
  ktitem.applyBounds({guidevc, guideh3, guidevb, guideh4, 2});

  auto s1item = headergroupitem._widget->makeChild<Label>("s1", fvec4(), "ModSrc1");
  s1item.applyBounds({guidevc, guideh4, guidevb, guideh5, 2});

  auto s1ditem = headergroupitem._widget->makeChild<Label>("s1d", fvec4(), "Src1Depth");
  s1ditem.applyBounds({guidevc, guideh5, guidevb, guideh6, 2});

  return headergroupitem._widget;
}
///////////////////////////////////////////////////////////////////////////////
layoutgroup_ptr_t createParamEdit(
    layoutgroup_ptr_t parent, //
    std::string named,
    fvec4 color,
    const anchor::Bounds& bounds,
    dspparam_ptr_t param) {
  auto paramitem = parent->makeChild<LayoutGroup>(named);
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
  auto namee      = FormatString("%s(%s)", named.c_str(), param->_units.c_str());
  auto headeritem = paramitem._widget->makeChild<Label>("header", color, namee);
  headeritem.applyBounds({guidevt, guidehl, guidevb, guideh0, 2});
  ///////////////////////////////////////////////////////////////////////////////
  auto coarseitem = paramitem._widget->makeChild<Dial>("coarse", color);
  coarseitem.applyBounds({guidevt, guideh0, guidevb, guideh1, 2});
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
  auto fineitem = paramitem._widget->makeChild<Dial>("fine", color);
  fineitem.applyBounds({guidevt, guideh1, guidevb, guideh2, 2});
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
  auto keytrackitem = paramitem._widget->makeChild<Dial>("kt", color);
  keytrackitem.applyBounds({guidevt, guideh2, guidevb, guideh3, 2});
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
  auto src1lab  = src1src ? src1src->_name.c_str() : "---";
  auto src1item = paramitem._widget->makeChild<Label>("src1", color, src1lab);
  src1item.applyBounds({guidevt, guideh3, guidevb, guideh4, 2});
  ///////////////////////////////////////////////////////////////////////////////
  auto src1ditem = paramitem._widget->makeChild<Dial>("src1d", color);
  src1ditem.applyBounds({guidevt, guideh4, guidevb, guidehr, 2});
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
    const anchor::Bounds& bounds) {
  auto hudpanel    = std::make_shared<HudPanel>();
  auto pmxviewitem = vp->makeChild<LayoutGroup>("ymenvview", 0, 0, 0, 0);
  pmxviewitem.applyBounds(bounds);
  // vp->addChild(pmxviewitem._widget);
  auto pmxdata = std::dynamic_pointer_cast<PMXData>(dbdata);
  OrkAssert(pmxdata);
  ////////////////////////////////////////////////
  // layout Guides
  ////////////////////////////////////////////////
  auto toplayout = pmxviewitem._layout;
  auto topvt     = toplayout->top();
  auto topv0     = toplayout->fixedHorizontalGuide(32);
  auto topv1     = toplayout->fixedHorizontalGuide(64);
  auto topvb     = toplayout->bottom();
  //
  auto guidehl = toplayout->left();
  auto guidehr = toplayout->right();
  //
  ////////////////////////////////////////////////
  //
  auto hdrstr     = FormatString("Pmx: %s", pmxdata->_name.c_str());
  auto headeritem = pmxviewitem._widget->makeChild<Label>("header", color, hdrstr);
  headeritem.applyBounds({topvt, guidehl, topv0, guidehr, 2});
  ////////////////////////////////////////////////
  //
  auto header2item = createParamHeader(
      pmxviewitem._widget, //
      {topv0, guidehl, topv1, guidehr, 2});
  ////////////////////////////////////////////////
  auto bodyitem = pmxviewitem._widget->makeChild<LayoutGroup>("bodygroup");
  bodyitem.applyBounds({topv1, guidehl, topvb, guidehr, 2});
  auto bglayout = bodyitem._layout;
  auto bgv0     = bglayout->proportionalHorizontalGuide(0.0f / 3.0f);
  auto bgv1     = bglayout->proportionalHorizontalGuide(1.0f / 3.0f);
  auto bgv2     = bglayout->proportionalHorizontalGuide(2.0f / 3.0f);
  auto bgv3     = bglayout->proportionalHorizontalGuide(3.0f / 3.0f);
  auto bgl      = bglayout->left();
  auto bgr      = bglayout->right();
  ////////////////////////////////////////////////
  auto pitchitem = createParamEdit(
      bodyitem._widget, //
      "pitch",
      color,
      {bgv0, bgl, bgv1, bgr, 2},
      dbdata->param(0));
  //
  auto ampitem = createParamEdit(
      bodyitem._widget, //
      "amp",
      color,
      {bgv1, bgl, bgv2, bgr, 2},
      dbdata->param(1));
  //
  auto fblitem = createParamEdit(
      bodyitem._widget, //
      "FBL",
      color,
      {bgv2, bgl, bgv3, bgr, 2},
      dbdata->param(2));
  ////////////////////////////////////////////////
  //
  return hudpanel;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
