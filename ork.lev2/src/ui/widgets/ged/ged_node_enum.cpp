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
#include <ork/lev2/ui/popups.inl>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::ged::GedEnumNode, "GedEnumNode");

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

struct GedEnumImpl {
  //////////////////////////////////////////////
  GedEnumImpl(GedEnumNode* node)
      : _node(node) {
    _enum_prop = dynamic_cast<const reflect::DirectEnumBase*>(_node->_iodriver->_par_prop);
    OrkAssert(_enum_prop);
  }
  //////////////////////////////////////////////
  void render(lev2::Context* pTARG) {
    auto container = _node->_container;
    auto model   = container->_model;
    auto skin    = container->_activeSkin;
    bool is_pick = skin->_is_pickmode;
    auto cur_val = _enum_prop->toString(_node->_iodriver->_object);

    if( is_pick )
      skin->DrawBgBox(_node, _node->miX, _node->miY, _node->miW, _node->miH, GedSkin::ESTYLE_BACKGROUND_1, 100);
    else {
      int HW = _node->miW>>1;
      int HX = _node->miX+HW;
      skin->DrawBgBox(_node, HX, _node->miY, HW, _node->miH, GedSkin::ESTYLE_BACKGROUND_3, 100);
      skin->DrawText(_node, _node->miX, _node->miY, _node->_propname.c_str());
      skin->DrawText(_node, HX, _node->miY, cur_val.c_str());
    }
  }
  //////////////////////////////////////////////
  bool onUiEvent(ork::ui::event_constptr_t ev) {
    switch (ev->_eventcode) {
      case ui::EventCode::DOUBLECLICK: {
        auto obj = _node->_iodriver->_object;
        auto items = _enum_prop->enumerateEnumerations(obj);
        printf( "NUMITEMS<%zu>\n", items.size() );
        std::vector<std::string> choices;
        for( auto item : items ) {
          choices.push_back( item->_name );
        }
        int sx = ev->miScreenPosX;
        int sy = ev->miScreenPosY;
        auto dims = ui::ChoiceList::computeDimensions(choices);
        auto choice = ui::popupChoiceList( _node->_l2context(), sx, sy, choices, dims );
        printf( "CHOICE<%s>\n", choice.c_str() );
        _enum_prop->setFromString(obj, choice);
        break;
      }
      default:
        break;
    }
    return _node->GedItemNode::OnUiEvent(ev);
  }
  //////////////////////////////////////////////
  const reflect::DirectEnumBase* _enum_prop = nullptr;
  GedEnumNode* _node                        = nullptr;
};

////////////////////////////////////////////////////////////////

void GedEnumNode::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

GedEnumNode::GedEnumNode(GedContainer* c, const char* name, newiodriver_ptr_t iodriver)
    : GedItemNode(c, name, iodriver) {
  _impl.makeShared<GedEnumImpl>(this);
}

////////////////////////////////////////////////////////////////

void GedEnumNode::DoDraw(lev2::Context* pTARG) {
  _impl.getShared<GedEnumImpl>()->render(pTARG);
}

////////////////////////////////////////////////////////////////

bool GedEnumNode::OnUiEvent(ork::ui::event_constptr_t ev) {
  return _impl.getShared<GedEnumImpl>()->onUiEvent(ev);
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged
