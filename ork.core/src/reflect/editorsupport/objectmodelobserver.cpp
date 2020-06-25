///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/opq.h>
///////////////////////////////////////////////////////////////////////////////

#include <queue>

#include <ork/reflect/properties/DirectTyped.h>
#include <ork/reflect/properties/IArray.h>
#include <ork/reflect/properties/IMap.h>
#include <ork/reflect/properties/I.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/editorsupport/objectmodel.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/DirectMapTyped.hpp>

#include <ork/file/path.h>
#include <ork/util/crc.h>
#include <signal.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::reflect::editor::ObjectModelObserver, "ObjectModelObserver");
///////////////////////////////////////////////////////////////////////////////
namespace ork::reflect::editor {
///////////////////////////////////////////////////////////////////////////////
void ObjectModelObserver::Describe() {
  RegisterAutoSlot(ObjectModelObserver, ModelInvalidated);
}
ObjectModelObserver::ObjectModelObserver()
    : ConstructAutoSlot(ModelInvalidated) {
}
void ObjectModelObserver::PropertyInvalidated(ork::object_ptr_t pobj, const reflect::I* prop) {
}
void ObjectModelObserver::Attach(ork::object_ptr_t obj) {
}
void ObjectModelObserver::SlotModelInvalidated() {
  onModelInvalidated();
}
//////////////////////////////////////////////////////////////////////////////

objectmodelnode_ptr_t ObjectModelObserver::topItemNode() const {
  return mItemStack.front();
}

//////////////////////////////////////////////////////////////////////////////

void ObjectModelObserver::PushItemNode(objectmodelnode_ptr_t qw) {
  mItemStack.push_front(qw);
  ComputeStackHash();
}

//////////////////////////////////////////////////////////////////////////////

void ObjectModelObserver::PopItemNode(objectmodelnode_ptr_t qw) {
  OrkAssert(mItemStack.front() == qw);
  mItemStack.pop_front();
  ComputeStackHash();
}

//////////////////////////////////////////////////////////////////////////////

void ObjectModelObserver::AddChild(objectmodelnode_ptr_t pw) {
  // printf( "ObjectModelObserver<%p> AddChild<%p>\n", this, pw );

  objectmodelnode_ptr_t TopItem = (mItemStack.size() > 0) ? mItemStack.front() : 0;
  if (TopItem == 0) // our root item widget
  {
  } else {
    TopItem->AddItem(pw);
  }
}

//////////////////////////////////////////////////////////////////////////////

U64 ObjectModelObserver::GetStackHash() const {
  return mStackHash;
}

void ObjectModelObserver::ComputeStackHash() {
  // const orkstack<objectmodelnode_ptr_t>& c = mItemStack._Get_container();

  U64 rval = 0;
  boost::Crc64 the_hash;

  int isize = (int)mItemStack.size();

  the_hash.accumulateItem<ObjectModel*>(_objectmodel.get());
  the_hash.accumulateItem<int>(isize);

  int idx = 0;
  for (auto it = mItemStack.begin(); it != mItemStack.end(); it++) {
    objectmodelnode_ptr_t pnode = *(it);

    const char* pname = pnode->_propname.c_str();

    size_t ilen = pnode->_propname.length();

    the_hash.accumulate(pname, ilen);
    the_hash.accumulateItem<int>(idx);

    idx++;
  }
  the_hash.finish();
  mStackHash = the_hash.result(); // | (the_hash.crc1<<32);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::editor
