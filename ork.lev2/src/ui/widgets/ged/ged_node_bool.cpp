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

ImplementReflectionX(ork::lev2::ged::GedBoolNode, "GedBoolNode");

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void GedBoolNode::describeX(class_t* clazz) {
}

GedBoolNode::GedBoolNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver )
      : GedItemNode(c, name, iodriver->_par_prop, iodriver->_object){
  _iodriver = iodriver;
  }

////////////////////////////////////////////////////////////////

static const int CHECKBOX_MARGIN = 2;
#define CHECKBOX_SIZE(height) (height - CHECKBOX_MARGIN * 2)
#define CHECKBOX_X(x, width, SIZE) (x + width - SIZE - CHECKBOX_MARGIN)
#define CHECKBOX_Y(y) (y + CHECKBOX_MARGIN)

void GedBoolNode::DoDraw(lev2::Context* pTARG){
  auto model = _container->_model;
  auto skin = _container->_activeSkin;
  bool is_pick = skin->_is_pickmode;

  const int SIZE = CHECKBOX_SIZE(miH);
  const int X    = CHECKBOX_X(miX, miW, SIZE);
  const int Y    = CHECKBOX_Y(miY);

  skin->DrawBgBox(this, X, Y, SIZE, SIZE, GedSkin::ESTYLE_BACKGROUND_2);

  bool value = _iodriver->_abstract_val.get<bool>();

  if (value)
    skin->DrawCheckBox(this, miX + miW - SIZE - CHECKBOX_MARGIN, miY + CHECKBOX_MARGIN, SIZE, SIZE);

  skin->DrawText(this, miX + 4, miY + 2, _propname.c_str());
}

///////////////////////////////////////////////////////////////////////////////

bool GedBoolNode::OnMouseReleased(ork::ui::event_constptr_t ev) {
  int evx = ev->miX;
  int evy = ev->miY;

  const int SIZE = CHECKBOX_SIZE(miH);
  const int X    = CHECKBOX_X(miX, miW, SIZE);
  const int Y    = CHECKBOX_Y(miY);

  if (evx > X && evx < X + SIZE && evy > Y && evy < Y + SIZE) {
    bool value = _iodriver->_abstract_val.get<bool>();
    _iodriver->_abstract_val.set<bool>(!value);
    _iodriver->_onValueChanged();
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool GedBoolNode::OnMouseDoubleClicked(ork::ui::event_constptr_t ev) {
  bool value = _iodriver->_abstract_val.get<bool>();
  _iodriver->_abstract_val.set<bool>(!value);
  _iodriver->_onValueChanged();
  return true;
}


////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
