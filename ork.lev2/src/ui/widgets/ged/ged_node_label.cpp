////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
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
    //GedItemNode::DoDraw(pTARG);
    auto& fontman = lev2::FontMan::GetRef();
    auto font = fontman.GetFont("i16");
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
