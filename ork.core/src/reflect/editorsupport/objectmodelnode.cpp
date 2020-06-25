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
#include <ork/reflect/properties/DirectTypedMap.hpp>

#include <ork/file/path.h>
#include <ork/util/crc.h>
#include <signal.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::reflect::editor::ObjectModelNode, "ObjectModelNode");
///////////////////////////////////////////////////////////////////////////////
namespace ork::reflect::editor {
///////////////////////////////////////////////////////////////////////////////
void ObjectModelNode::Describe() {
}
//////////////////////////////////////////////////////////////////////////////
ObjectModelNode::ObjectModelNode(
    objectmodel_ptr_t mdl, //
    const char* name,
    const reflect::ObjectProperty* prop,
    ork::object_ptr_t obj)
    : _objectmodel(mdl)
    , mRoot(mdl->_observer)
    , _propname(name)
    , mOrkProp(prop)
    , mOrkObj(obj)
    , _parent(0)
    , mbInvalid(true) {

  // Init();
}
//////////////////////////////////////////////////////////////////////////////
ObjectModelNode::~ObjectModelNode() {
  releaseChildren();
}
//////////////////////////////////////////////////////////////////////////////
void ObjectModelNode::SigInvalidateProperty() {
  _objectmodel->SigPropertyInvalidated(GetOrkObj(), GetOrkProp());
}
///////////////////////////////////////////////////
ObjectModelNode* ObjectModelNode::parent() const {
  return _parent;
}
///////////////////////////////////////////////////
void ObjectModelNode::SetOrkProp(const reflect::ObjectProperty* prop) {
  mOrkProp = prop;
}
///////////////////////////////////////////////////
const reflect::ObjectProperty* ObjectModelNode::GetOrkProp() const {
  return mOrkProp;
}
///////////////////////////////////////////////////
void ObjectModelNode::SetOrkObj(ork::object_ptr_t obj) { // setObject
  mOrkObj = obj;
}
///////////////////////////////////////////////////
ork::object_ptr_t ObjectModelNode::GetOrkObj() const { // object()
  return mOrkObj;
}
///////////////////////////////////
void ObjectModelNode::Invalidate() {
  mbInvalid = true;
}
///////////////////////////////////////////////////
objectmodelobserver_ptr_t ObjectModelNode::root() const { //
  return mRoot;
}
///////////////////////////////////////////////////
void ObjectModelNode::SetDepth(int id) {
  _depth = id;
}
///////////////////////////////////////////////////
int ObjectModelNode::GetDepth() const {
  return _depth;
}
///////////////////////////////////////////////////
int ObjectModelNode::GetDecoIndex() const {
  return _linearindex;
}
///////////////////////////////////////////////////
void ObjectModelNode::SetDecoIndex(int idx) {
  _linearindex = idx;
}
//////////////////////////////////////////////////////////////////////////////
void ObjectModelNode::releaseChildren() {
  mItems.clear();
}
//////////////////////////////////////////////////////////////////////////////
void ObjectModelNode::AddItem(objectmodelnode_ptr_t w) {
  w->SetDecoIndex(int(mItems.size()));
  mItems.push_back(w);
  w->_parent = this;
}
//////////////////////////////////////////////////////////////////////////////
objectmodelnode_ptr_t ObjectModelNode::GetItem(int idx) const {
  return mItems[idx];
}
//////////////////////////////////////////////////////////////////////////////
int ObjectModelNode::GetNumItems() const {
  return int(mItems.size());
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::editor
