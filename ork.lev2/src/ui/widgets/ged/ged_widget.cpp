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

orkvector<GedSkin*> InstantiateSkins();

////////////////////////////////////////////////////////////////

void GedWidget::IncrementSkin() {
  miSkin++;
  DoResize();
}

////////////////////////////////////////////////////////////////

GedWidget::GedWidget(objectmodel_ptr_t mdl)
    : _model(mdl)
    , mRootItem(0)
    , miW(0)
    , miH(0)
    , miRootH(0)
    , mStackHash(0)
    , miSkin(1)
    , mbDeleteModel(false) {
    //, ConstructAutoSlot(Repaint)
    //, ConstructAutoSlot(ModelInvalidated) {
  //SetupSignalsAndSlots();

  //mdl->SetGedWidget(this);
  mRootItem = std::make_shared<GedRootNode>(mdl.get(), "Root", nullptr, nullptr);
  PushItemNode(mRootItem.get());

  orkvector<GedSkin*> skins = InstantiateSkins();

  for (auto skin : skins) {
    AddSkin(skin);
  }

  /*object::Connect(	& this->GetSigRepaint(),
                      & mCTQT->GetSlotRepaint() );*/
}

////////////////////////////////////////////////////////////////


GedWidget::~GedWidget() {
  //DisconnectAll();
}

////////////////////////////////////////////////////////////////

void GedWidget::PropertyInvalidated(object_ptr_t pobj, 
                                    const reflect::ObjectProperty* prop) {
  if (mRootItem) {
    orkstack<GedItemNode*> stackofnodes;
    stackofnodes.push(mRootItem.get());

    while (false == stackofnodes.empty()) {
      GedItemNode* pnode = stackofnodes.top();
      stackofnodes.pop();

      if (pnode->_object == pobj) {
        if (pnode->_property == prop) {
          pnode->Invalidate();
        }
      }
      int inumkids = pnode->numChildren();
      for (int i = 0; i < inumkids; i++) {
        auto pkid = pnode->_children[i];
        stackofnodes.push(pkid.get());
      }
    }
  }
  SlotRepaint();
}

////////////////////////////////////////////////////////////////

GedItemNode* GedWidget::ParentItemNode() const {
  return _itemstack.front();
}
//////////////////////////////////////////////////////////////////////////////

void GedWidget::PushItemNode(GedItemNode* qw) {
  _itemstack.push_front(qw);
  ComputeStackHash();
}

//////////////////////////////////////////////////////////////////////////////

void GedWidget::PopItemNode(GedItemNode* qw) {
  OrkAssert(_itemstack.front() == qw);
  _itemstack.pop_front();
  ComputeStackHash();
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
