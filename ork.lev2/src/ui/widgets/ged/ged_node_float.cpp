////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/reflect/properties/registerX.inl>
#include "ged_slider.inl"

ImplementReflectionX(ork::lev2::ged::GedFloatNode, "GedFloatNode");

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

using slider_t = Slider<float>;
void GedFloatNode::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

GedFloatNode::GedFloatNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
    : GedItemNode(c, name, iodriver->_par_prop, iodriver->_object){
  _iodriver = iodriver;

  float fmin = 0.0f;
  float fmax = 1.0f;

  auto prop           = iodriver->_par_prop;
  auto anno_range     = prop->annotation("editor.range");
  ConstString annolog = prop->GetAnnotation("editor.range.log");

  if (auto as_range = anno_range.tryAs<float_range>()) {
    const auto& range = as_range.value();
    fmin = range._min;
    fmax = range._max+0.5;
  }

  if (annolog.length()) {
    if (annolog == "true") {
      _is_log_mode = true;
    }
  }

  float ivalue = _iodriver->_abstract_val.get<float>();
  _slider = std::make_shared<slider_t>(this, fmin, fmax, ivalue);
}

////////////////////////////////////////////////////////////////

void GedFloatNode::DoDraw(lev2::Context* pTARG) {
  auto model   = _container->_model;
  auto skin    = _container->_activeSkin;
  bool is_pick = skin->_is_pickmode;

  _slider->resize(miX, miY, miW, miH);

  int ixi = int(_slider->GetIndicPos()) - miX;
  skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
  skin->DrawBgBox(this, miX + 2, miY + 2, miW - 3, miH - 4, GedSkin::ESTYLE_BACKGROUND_2);
  skin->DrawBgBox(this, miX + 2, miY + 3, ixi, miH - 6, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT);

  float ity = get_text_center_y();
  ////////////////////////////////////////////////////////////////

  float fvalue = _iodriver->_abstract_val.get<float>();

  float finp          = _slider->GetTextPos();
  float itxi            = miX + float(finp);
  PropTypeString& str = _slider->ValString();
  skin->DrawText(this, itxi, ity, str.c_str());
  skin->DrawText(this, miX + 4, ity, _propname.c_str());
}

///////////////////////////////////////////////////////////////////////////////

bool GedFloatNode::OnUiEvent(ork::ui::event_constptr_t ev) {
  return _slider->OnUiEvent(ev);
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged
