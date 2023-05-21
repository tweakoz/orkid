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

ImplementReflectionX(ork::lev2::ged::GedEnumNode, "GedEnumNode");

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void GedEnumNode::describeX(class_t* clazz) {
}

GedEnumNode::GedEnumNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
      : GedItemNode(c, name, iodriver) {
  }

////////////////////////////////////////////////////////////////

void GedEnumNode::DoDraw(lev2::Context* pTARG){
  auto model = _container->_model;
  auto skin = _container->_activeSkin;
  bool is_pick = skin->_is_pickmode;

  skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_3, 100);

  if( not is_pick ){
     skin->DrawText(this, miX, miY, _propname.c_str());
  }

}

////////////////////////////////////////////////////////////////

bool GedEnumNode::OnUiEvent( ork::ui::event_constptr_t ev ){
    return GedItemNode::OnUiEvent(ev);
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
