////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/lev2/ui/ged/ged_widget.h>
#include <ork/kernel/core_interface.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

orkvector<GedSkin*> InstantiateSkins() {
  orkvector<GedSkin*> skins;
  /*
  while (0 == lev2::GfxEnv::GetRef().loadingContext()) {
    ork::msleep(100);
  }
  auto targ = lev2::GfxEnv::GetRef().loadingContext();
  FontMan::gpuInit(targ);
  skins.push_back(new GedSkin0());
  skins.push_back(new GedSkin1());*/
  
  return skins;
}

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

//////////////////////////////////////////////////////////////////////////////

void GedWidget::ComputeStackHash() {
  // const orkstack<GedItemNode*>& c = _itemstack._Get_container();

  U64 rval = 0;
  boost::Crc64 the_hash;

  int isize = (int)_itemstack.size();

  the_hash.accumulateItem<ObjModel*>(_model.get());
  the_hash.accumulateItem<int>(isize);

  int idx = 0;
  for (std::deque<GedItemNode*>::const_iterator it = _itemstack.begin(); it != _itemstack.end(); it++) {
    const GedItemNode* pnode = *(it);

    const char* pname = pnode->_propname.c_str();

    size_t ilen = pnode->_propname.length();

    the_hash.accumulate(pname, ilen);
    the_hash.accumulateItem<int>(idx);

    idx++;
  }
  the_hash.finish();
  mStackHash = the_hash.result(); // | (the_hash.crc1<<32);
}

////////////////////////////////////////////////////////////////

void GedWidget::SlotRepaint() {
  // printf( "GedWidget::SlotRepaint\n" );
  _viewport->MarkSurfaceDirty();
}

////////////////////////////////////////////////////////////////

void GedWidget::DoResize() {
  if (mRootItem) {
    int inum = mRootItem->numChildren();
    miRootH  = mRootItem->CalcHeight();
    mRootItem->Layout(2, 2, miW - 4, miH - 4);
    // printf( "GedWidget<%p>::DoResize() dims<%d %d> miRootH<%d> inumitems<%d>\n", this, miW, miH, miRootH, inum );
  } else {
    miRootH = 0;
  }
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
