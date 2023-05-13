////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/lev2/ui/ged/ged_factory.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/lev2/ui/popups.inl>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/orkpool.h>
#include <ork/reflect/properties/registerX.inl>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

struct GradientEditorImpl{
    GradientEditorImpl(GedGradientNode* node)
        : _node(node)
    {
    }
    GedGradientNode* _node = nullptr;
};

////////////////////////////////////////////////////////////////

void GedGradientNode::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

GedGradientNode::GedGradientNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
      : GedItemNode(c, name, iodriver->_par_prop, iodriver->_object) {

      auto gei = _impl.makeShared<GradientEditorImpl>(this);
  }

////////////////////////////////////////////////////////////////

void GedGradientNode::DoDraw(lev2::Context* pTARG){
  auto model = _container->_model;
  auto skin = _container->_activeSkin;
  bool is_pick = skin->_is_pickmode;

  skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1, 100);

  if( not is_pick ){
     skin->DrawText(this, miX, miY, _propname.c_str());
  }

}

  void GedGradientNode::OnUiEvent(ork::ui::event_constptr_t ev) {

  }
  int GedGradientNode::doComputeHeight() {
    return 100;
  }

////////////////////////////////////////////////////////////////

void GedNodeFactoryGradient::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

geditemnode_ptr_t
GedNodeFactoryGradient::createItemNode(GedContainer* container, const ConstString& Name, newiodriver_ptr_t iodriver) const {
  return std::make_shared<GedGradientNode>(container, Name.c_str(), iodriver);
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {

ImplementReflectionX(ork::lev2::ged::GedGradientNode, "GedGradientNode");
ImplementReflectionX(ork::lev2::ged::GedNodeFactoryGradient, "GedNodeFactoryGradient");
