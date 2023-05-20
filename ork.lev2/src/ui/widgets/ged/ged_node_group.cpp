////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/kernel/core_interface.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::ged::GedGroupNode, "GedGroupNode");

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void GedGroupNode::describeX(class_t* clazz) {
}

GedGroupNode::GedGroupNode(
    GedContainer* container, //
    const char* name,
    const reflect::ObjectProperty* prop,
    object_ptr_t obj,
    bool is_obj_node)
    : GedItemNode(container, name, prop, obj)
    , mbCollapsed(false == is_obj_node)
    , mIsObjNode(is_obj_node) {

    auto model = _container->_model;
  std::string fixname = name;
  /////////////////////////////////////////////////////////////////
  // localize collapse states to instances of properties underneath other properties
  GedItemNode* parent = model->_gedContainer->ParentItemNode();
  if (parent) {
    const char* parname = parent->_propname.c_str();
    if (parname) {
      fixname += CreateFormattedString("_%s_", parname);
    }
  }
  /////////////////////////////////////////////////////////////////

  int ilen = (int)fixname.length();
  for (int i = 0; i < ilen; i++) {
    switch (fixname[i]) {
      case ':':
      case '<':
      case '>':
      case ' ':
        fixname[i] = ' ';
        break;
    }
  }

  mPersistID.format("%s_group_collapse", fixname.c_str());

  ///////////////////////////////////////////
  PersistHashContext HashCtx;
  auto pmap = model->persistMapForHash(HashCtx);
  ///////////////////////////////////////////

  const std::string& str_collapse = pmap->value(mPersistID.c_str());

  if (str_collapse == "false") {
    mbCollapsed = false;
  }

  updateVisibility();
}
///////////////////////////////////////////////////////////////////////////////
bool GedGroupNode::OnMouseDoubleClicked(ork::ui::event_constptr_t ev) {

  auto model = _container->_model;
  int inumitems = numChildren();

  bool isCTRL = ev->mbCTRL;

  const int kdim = get_charh();

  printf("GedGroupNode<%p>::mouseDoubleClickEvent inumitems<%d>\n", this, inumitems);

  if (inumitems) {
    int ix = ev->miX;
    int iy = (ev->miY);

    //////////////////////////////
    // spawn/stack
    //////////////////////////////

    int ioff = koff;
    int idim = get_charh();
    int dby1 = miY + ioff;
    int dby2 = dby1 + idim;
    int il   = miW - (idim * 2);
    int iw   = idim - 1;
    int ih   = idim - 2;

    int il2 = il + idim + 1;
    int iy2 = dby2 - 1;
    int ihy = dby1 + (ih / 2);
    int ihx = il + (iw / 2);

    printf("iy<%d> KOFF<%d> KDIM<%d>\n", iy, koff, kdim);
    if (iy >= koff && iy <= kdim) {
      if (ix >= il && ix < il2 && mIsObjNode) {
        if (_object) {
          auto top = model->browseStackTop();
          if (top == _object) {
            model->browseStackPop();
            top = model->browseStackTop();
            if (top) {
              model->attach(top, false);
            }
          } else {
            model->browseStackPush(_object);
            model->attach(_object, false);
          }
        }

      } else if (ix >= il2 && ix < il2 + idim && mIsObjNode) {
        if (_object) {
          // spawn new window here
          model->SigSpawnNewGed(_object);
        }
      } else if (ix >= koff && ix <= kdim) // drop down
      {
        mbCollapsed = !mbCollapsed;

        ///////////////////////////////////////////
        PersistHashContext HashCtx;
        auto pmap = model->persistMapForHash(HashCtx);
        ///////////////////////////////////////////

        pmap->setValue(mPersistID.c_str(), mbCollapsed ? "true" : "false");

        if (isCTRL) { // also do siblings
          if (_parent) {
            int inumc = _parent->numChildren();
            for (int i = 0; i < inumc; i++) {
              auto child    = _parent->_children[i];
              auto child_as_group = std::dynamic_pointer_cast<GedGroupNode>(child);
              if (child_as_group) {
                child_as_group->mbCollapsed = mbCollapsed;
                child_as_group->updateVisibility();
                pmap->setValue(child_as_group->mPersistID.c_str(), mbCollapsed ? "true" : "false");
              }
            }
          }
        }

        updateVisibility();
        return true;
      }
    }
  }
  return false;
}
///////////////////////////////////////////////////////////////////////////////
void GedGroupNode::updateVisibility() {

  int inumitems = numChildren();

  if (inumitems) {
    if (mbCollapsed) {
      for (int it = 0; it < inumitems; it++) {
        _children[it]->SetVisible(false);
      }
    } else {
      for (int it = 0; it < inumitems; it++) {
        _children[it]->SetVisible(true);
      }
    }
  }
  _container->DoResize();
}
///////////////////////////////////////////////////////////////////////////////
int GedGroupNode::doComputeHeight() const {
  return GedItemNode::doComputeHeight() + 1;
}
///////////////////////////////////////////////////////////////////////////////
void GedGroupNode::DoDraw(lev2::Context* pTARG) {

  auto model = _container->_model;
  auto skin = _container->_activeSkin;
  int inumitems   = numChildren();
  int stack_depth = model->browseStackSize();

  /////////////////
  // drop down box
  /////////////////
  int icentery = get_text_center_y();

  int ioff = koff;
  int idim = get_charh();

  int dbx1 = miX + ioff;
  int dbx2 = dbx1 + idim;
  int dby1 = miY + ioff;
  int dby2 = dby1 + idim;

  int labw = this->propnameWidth();
  int labx = miX + 12;
  if (labx < dbx2 + 3)
    labx = dbx2 + 3;

  ////////////////////////////////

  int guide_R = miX + miW - (idim * 2);
  int iw = idim - 1;
  int ih = idim - 2;

  int il2 = guide_R + idim + 1;
  int iy2 = dby2 - 1;
  int ihy = dby1 + (ih / 2);
  int ihx = guide_R + (iw / 2);

  //skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
  skin->DrawText(this, labx, icentery-1, _propname.c_str());
  skin->DrawBgBox(this, miX, miY, miW-2, skin->_bannerHeight, GedSkin::ESTYLE_BACKGROUND_GROUP_LABEL);

  ////////////////////////////////
  // draw stack depth indicator on top node
  ////////////////////////////////

  if ((stack_depth - 1) && (_object == model->browseStackTop()) && mIsObjNode) {
    std::string arrs;
    for (int i = 0; i < (stack_depth - 1); i++)
      arrs += "<";
    skin->DrawText(this, guide_R - idim - ((stack_depth - 1) * get_charw()), icentery, arrs.c_str());
  }

  ////////////////////////////////

  if (inumitems) {
    if (mbCollapsed) {
      skin->DrawRightArrow(this, dbx1+3, dby1+3, GedSkin::ESTYLE_BUTTON_OUTLINE);
    } else {
      skin->DrawDownArrow(this, dbx1+3, dby1+3, GedSkin::ESTYLE_BUTTON_OUTLINE);
    }
  }

  ////////////////////////////////
  // draw newged button
  ////////////////////////////////

  if (_object && mIsObjNode) {
    int boxw = iw-3;
    int boxh = ih-2;
    skin->DrawUpArrow( this,  guide_R+4, dby1+1, GedSkin::ESTYLE_BUTTON_OUTLINE );
  }

  ////////////////////////////////
}
////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
