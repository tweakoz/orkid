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

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

GedLabelNode::GedLabelNode(GedContainer* c, const char* name, const reflect::ObjectProperty* prop, object_ptr_t obj)
      : GedItemNode(c, name, prop, obj) {
  }

////////////////////////////////////////////////////////////////

void GedLabelNode::DoDraw(lev2::Context* pTARG){
  auto model = _container->_model;
  auto skin = _container->_activeSkin;
  bool is_pick = true; //pTARG->FBI()->isPickState();

  skin->DrawBgBox(this, miX, miY, 10, 10, GedSkin::ESTYLE_BACKGROUND_2, 0);

  if( not is_pick ){
     skin->DrawText(this, miX, miY, _propname.c_str());
  }

}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
