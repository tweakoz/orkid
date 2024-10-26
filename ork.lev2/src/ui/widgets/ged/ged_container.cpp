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
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/kernel/core_interface.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

orkvector<GedSkin*> instantiateSkins(ork::lev2::Context* ctx);

void GedContainer::IncrementSkin() {
  _skin_index++;
  DoResize();
}

////////////////////////////////////////////////////////////////

gedcontainer_ptr_t GedContainer::createShared(objectmodel_ptr_t mdl){
  auto widget = std::make_shared<GedContainer>(mdl);
  return widget;
}

////////////////////////////////////////////////////////////////

GedContainer::GedContainer(objectmodel_ptr_t mdl)
    : _model(mdl){
  mdl->_gedContainer = this;
  mRootItem = std::make_shared<GedRootNode>(this, "Root", nullptr, nullptr);
  PushItemNode(mRootItem.get());

  _connection_modelinvalidated = _model->_sigModelInvalidated.connect([this](){
      //this->MarkSurfaceDirty();

  }); // Connect the signal to the slot
  
  /*object::Connect(	& this->GetSigRepaint(),
                      & mCTQT->GetSlotRepaint() );*/
}

////////////////////////////////////////////////////////////////


GedContainer::~GedContainer() {
  _connection_modelinvalidated.disconnect();
}

////////////////////////////////////////////////////////////////

void GedContainer::gpuInit(lev2::Context* context){
  mSkins = instantiateSkins(context);
  _activeSkin = mSkins[1];
}

////////////////////////////////////////////////////////////////

void GedContainer::PropertyInvalidated(object_ptr_t pobj, 
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

GedItemNode* GedContainer::ParentItemNode() const {
  return _itemstack.front();
}
//////////////////////////////////////////////////////////////////////////////

void GedContainer::PushItemNode(GedItemNode* qw) {
  _itemstack.push_front(qw);
  ComputeStackHash();
}

//////////////////////////////////////////////////////////////////////////////

void GedContainer::PopItemNode(GedItemNode* qw) {
  OrkAssert(_itemstack.front() == qw);
  _itemstack.pop_front();
  ComputeStackHash();
}

//////////////////////////////////////////////////////////////////////////////

void GedContainer::ComputeStackHash() {
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

void GedContainer::SlotRepaint() {
  // printf( "GedContainer::SlotRepaint\n" );
  _viewport->MarkSurfaceDirty();
}

////////////////////////////////////////////////////////////////

void GedContainer::DoResize() {
  if (mRootItem) {
    int inum = mRootItem->numChildren();
    miRootH  = mRootItem->computeHeight();
    mRootItem->Layout(2, 2, miW - 4, miRootH - 4);
    if(0)printf( "GedContainer<%p>::DoResize() dims<%d %d> miRootH<%d> inumitems<%d>\n", this, miW, miH, miRootH, inum );
  } else {
    miRootH = 0;
  }
}

//////////////////////////////////////////////////////////////////////////////

void GedContainer::AddChild(geditemnode_ptr_t child_node) {
  // printf( "GedContainer<%p> AddChild<%p>\n", this, pw );

  GedItemNode* TopItem = (_itemstack.size() > 0) //
                       ? _itemstack.front() //
                       : nullptr;

  if (TopItem  ) {
    TopItem->addChild(child_node);
  }
}

//////////////////////////////////////////////////////////////////////////////

void GedContainer::SetDims(int iw, int ih) {
  miW = iw;
  miH = ih;
  DoResize();
}

//////////////////////////////////////////////////////////////////////////////

void GedContainer::Draw(lev2::Context* context, int iw, int ih, int iscrolly) {
  _activeSkin->_scrollY = iscrolly;
  _activeSkin->Begin(context, _viewport); { //
  ///////////////////////////////////////////////
  orkstack<geditemnode_ptr_t> node_stack;
  node_stack.push(GetRootItem());
  ///////////////////////////////////////////////
    while (false == node_stack.empty()) {
      auto item = node_stack.top();
      node_stack.pop();
      int id = item->GetDepth();
      ///////////////////////////////////////////////
      int inumc = item->numChildren();
      for (int ic = 0; ic < inumc; ic++) {
        auto child = item->child(ic);
        if (child->IsVisible()) {
          child->SetDepth(id + 1);
          node_stack.push(child);
        }
      }
      ///////////////////////////////////////////////
      if (item->IsVisible()) {
        item->Draw(context);
      }
      ///////////////////////////////////////////////
    }
  }
  _activeSkin->End(context);
  ///////////////////////////////////////////////
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
