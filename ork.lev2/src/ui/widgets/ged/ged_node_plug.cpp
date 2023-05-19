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
#include <ork/lev2/ui/ged/ged_factory.h>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/ui/popups.inl>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void GedPlugNode::describeX(class_t* clazz) {
}

GedPlugNode::GedPlugNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
    : GedItemNode(c, name, iodriver) {

      OrkAssert(false);
}

////////////////////////////////////////////////////////////////

void GedPlugNode::DoDraw(lev2::Context* pTARG) {
  auto model   = _container->_model;
  auto skin    = _container->_activeSkin;
  bool is_pick = skin->_is_pickmode;

  skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_2, 100);

  //if (not is_pick) {
    //skin->DrawText(this, miX, miY, _propname.c_str());
  //}
}

////////////////////////////////////////////////////////////////

bool GedPlugNode::OnUiEvent(ui::event_constptr_t ev) {
  int sx = ev->miScreenPosX;
  int sy = ev->miScreenPosY;
  switch (ev->_eventcode) {
    case ui::EventCode::DOUBLECLICK: {
      std::vector<std::string> choices;
      choices.push_back("Plug1");
      choices.push_back("Plug2");
      fvec2 dimensions = ui::ChoiceList::computeDimensions(choices);
      std::string choice = ui::popupChoiceList(
          this->_l2context(), //
          sx - int(dimensions.x)>>1,
          sy - int(dimensions.y)>>1,
          choices,
          dimensions);
      printf("choice<%s>\n", choice.c_str());
      break;
    }
    default:
      break;
  }
  return true;
}

////////////////////////////////////////////////////////////////

void GedNodeFactoryPlug::describeX(class_t* clazz) {
}

GedNodeFactoryPlug::GedNodeFactoryPlug() {
}

geditemnode_ptr_t
GedNodeFactoryPlug::createItemNode(GedContainer* container, const ConstString& Name, newiodriver_ptr_t iodriver) const {
  return std::make_shared<GedPlugNode>(container, Name.c_str(), iodriver);
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged

ImplementReflectionX(ork::lev2::ged::GedPlugNode, "GedPlugNode");
ImplementReflectionX(ork::lev2::ged::GedNodeFactoryPlug, "GedNodeFactoryPlug");
