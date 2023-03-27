////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/kernel/core_interface.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
GedGroupNode::GedGroupNode(
    ObjModel* mdl,
    const char* name,
    const reflect::ObjectProperty* prop,
    object_ptr_t obj,
    bool is_obj_node)
    : GedItemNode(mdl, name, prop, obj)
    , mbCollapsed(false == is_obj_node)
    , mIsObjNode(is_obj_node) {

  std::string fixname = name;
  /////////////////////////////////////////////////////////////////
  // localize collapse states to instances of properties underneath other properties
  GedItemNode* parent = mdl->_gedWidget->ParentItemNode();
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
  auto pmap = mdl->persistMapForHash(HashCtx);
  ///////////////////////////////////////////

  const std::string& str_collapse = pmap->value(mPersistID.c_str());

  if (str_collapse == "false") {
    mbCollapsed = false;
  }

  CheckVis();
}
///////////////////////////////////////////////////////////////////////////////
void GedGroupNode::OnMouseDoubleClicked(ork::ui::event_constptr_t ev) {
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
          auto top = _model->browseStackTop();
          if (top == _object) {
            _model->browseStackPop();
            top = _model->browseStackTop();
            if (top) {
              _model->attach(top, false);
            }
          } else {
            _model->browseStackPush(_object);
            _model->attach(_object, false);
          }
        }

      } else if (ix >= il2 && ix < il2 + idim && mIsObjNode) {
        if (_object) {
          // spawn new window here
          _model->SigSpawnNewGed(_object);
        }
      } else if (ix >= koff && ix <= kdim) // drop down
      {
        mbCollapsed = !mbCollapsed;

        ///////////////////////////////////////////
        PersistHashContext HashCtx;
        auto pmap = _model->persistMapForHash(HashCtx);
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
                child_as_group->CheckVis();
                pmap->setValue(child_as_group->mPersistID.c_str(), mbCollapsed ? "true" : "false");
              }
            }
          }
        }

        CheckVis();
        return;
      }
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void GedGroupNode::CheckVis() {
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
  _model->_gedWidget->DoResize();
}
////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
