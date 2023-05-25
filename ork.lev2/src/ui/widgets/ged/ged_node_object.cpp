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
#include <ork/lev2/ui/popups.inl>

ImplementReflectionX(ork::lev2::ged::GedObjNode, "GedObjNode");

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void GedObjNode::describeX(class_t* clazz) {
}

GedObjNode::GedObjNode(GedContainer* c, const char* name, const reflect::ObjectProperty* prop, object_ptr_t obj)
    : GedItemNode(c, name, prop, obj) {
}

////////////////////////////////////////////////////////////////

void GedObjNode::DoDraw(lev2::Context* pTARG) {
  auto model   = _container->_model;
  auto skin    = _container->_activeSkin;
  bool is_pick = skin->_is_pickmode;

  skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1, 100);

  if (not is_pick) {
    skin->DrawText(this, miX, miY, _propname.c_str());
  }
}

////////////////////////////////////////////////////////////////

bool GedObjNode::OnUiEvent(ui::event_constptr_t ev) {
    switch (ev->_eventcode) {
      case ui::EventCode::DOUBLECLICK: {
        auto obj = _iodriver->_object;
        int sx = ev->miScreenPosX;
        int sy = ev->miScreenPosY;
        auto ctx = _l2context();
        const int kdim  = 256;
        int W = kdim;
        int H = kdim;
        auto prop    = dynamic_cast<const reflect::DirectTyped<fvec4>*>(_iodriver->_par_prop);
        fvec4 initial_color;
        //prop->get(initial_color,_iodriver->_object);
        fvec4 selected = ui::popupColorEdit(ctx, sx, sy, W, H, initial_color);
        //prop->set(selected,_iodriver->_object);
        break;
      }
      default:
        break;
    }
    return GedItemNode::OnUiEvent(ev);}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged
