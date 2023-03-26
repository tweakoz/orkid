////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>

ImplementReflectionX(ork::lev2::ged::ObjModel, "GedObjModel");

// template class ork::object::Signal<void,ork::lev2::ged::ObjModel>;

namespace ork::lev2::ged {

orkset<objectmodel_ptr_t> ObjModel::gAllObjectModels;

/* // connect.h line 183
ork::object::Signal::operator()<
  void,
  ObjModel,
  std::shared_ptr<Object>>
        (void (ObjModel::*)(std::shared_ptr<Object>), std::shared_ptr<Object>
*/

void ObjModel::describeX(object::ObjectClass* clazz) {
  ///////////////////////////////////////////
  // A Touch Of Class

  // GedFactoryEnum::GetClassStatic();
  // GedFactoryGradient::GetClassStatic();
  // GedFactoryCurve::GetClassStatic();
  // GedFactoryAssetList::GetClassStatic();
  // GedFactoryFileList::GetClassStatic();
  // GedFactoryTransform::GetClassStatic();

  //////////////////////////////////////////////////////////////////
  /*RegisterAutoSignal(ObjModel, Repaint);
  RegisterAutoSignal(ObjModel, ModelInvalidated);
  RegisterAutoSignal(ObjModel, PreNewObject);
  RegisterAutoSignal(ObjModel, PropertyInvalidated);
  RegisterAutoSignal(ObjModel, NewObject);
  RegisterAutoSignal(ObjModel, SpawnNewGed);
  //////////////////////////////////////////////////////////////////
  RegisterAutoSlot(ObjModel, NewObject);
  RegisterAutoSlot(ObjModel, ObjectDeleted);
  RegisterAutoSlot(ObjModel, ObjectSelected);
  RegisterAutoSlot(ObjModel, ObjectDeSelected);
  RegisterAutoSlot(ObjModel, RelayModelInvalidated);
  RegisterAutoSlot(ObjModel, RelayPropertyInvalidated);*/
  //////////////////////////////////////////////////////////////////
}

objectmodel_ptr_t ObjModel::createShared(opq::opq_ptr_t updateopq){
    auto objmodel = std::make_shared<ObjModel>(updateopq);
   AutoConnector::setupSignalsAndSlots(objmodel);
   gAllObjectModels.insert(objmodel);
   return objmodel;
}

ObjModel::ObjModel(opq::opq_ptr_t updateopq)
    : _enablePaint(true) {
    //, mModelInvalidatedInvoker(mSignalModelInvalidated.CreateInvokation())
    //, ConstructAutoSlot(NewObject)
    //, ConstructAutoSlot(RelayModelInvalidated)
    //, ConstructAutoSlot(RelayPropertyInvalidated)
    //, ConstructAutoSlot(ObjectDeleted)
    //, ConstructAutoSlot(ObjectSelected)
    //, ConstructAutoSlot(ObjectDeSelected)
    //, ConstructAutoSlot(Repaint) {

  _updateOPQ = updateopq ? updateopq : opq::updateSerialQueue();

  _persistMapContainer = std::make_shared<PersistMapContainer>();

  ///////////////////////////////////////////

  // object::Connect(&mSignalNewObject, &mSlotNewObject);

}

///////////////////////////////////////////////////////////////////////////////

ObjModel::~ObjModel() {
  // DisconnectAll();
  // gAllObjectModels.erase(this);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotRelayPropertyInvalidated(object_ptr_t pobj, const reflect::ObjectProperty* prop) {
  // if (_gedWidget)
  //_gedWidget->PropertyInvalidated(pobj, prop);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotRelayModelInvalidated() {
  attach(_currentObject);
  // SigModelInvalidated();
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigModelInvalidated() {
  // printf("ObjModel::SigModelInvalidated\n");
  //mSignalModelInvalidated(&ObjModel::SigModelInvalidated); // << operator() instantiated here
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigPreNewObject() {
  _updateOPQ->enqueue([=]() { //
    //this->mSignalPreNewObject(&ObjModel::SigPreNewObject);
  });
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigPropertyInvalidated(object_ptr_t pobj, const reflect::ObjectProperty* prop) {
  _updateOPQ->enqueue([=]() { //
    //this->mSignalPropertyInvalidated(&ObjModel::SigPropertyInvalidated, pobj, prop);
  });
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigRepaint() {
  //mSignalRepaint(&ObjModel::SigRepaint);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigNewObject(object_ptr_t pobj) {
  //SigPreNewObject();
  //mSignalNewObject(&ObjModel::SigNewObject, pobj);
  //SigRepaint();
  //SigPostNewObject(pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigPostNewObject(object_ptr_t pobj) {
  //mSignalPostNewObject(&ObjModel::SigPostNewObject, pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigSpawnNewGed(object_ptr_t pOBJ) {
  //mSignalSpawnNewGed(&ObjModel::SigSpawnNewGed, pOBJ);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotNewObject(object_ptr_t pobj) {
  attach(_currentObject);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotObjectDeleted(object_ptr_t pobj) {
  attach(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotObjectSelected(object_ptr_t pobj) {
  // printf("ObjModel<%p> Object<%p> selected\n", this, pobj);
  attach(pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotObjectDeSelected(object_ptr_t pobj) {
  attach(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::enqueueObject(object_ptr_t obj) {
  _enqueuedObject = obj;
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::enqueueUpdate() {
  auto lamb = [=]() { queueFlush(); };
  _updateOPQ->enqueue(lamb);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::enqueueUpdateAll() {
  auto lamb = [=]() { queueFlushAll(); };
  _updateOPQ->enqueue(lamb);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::queueFlush() {
  attach(_currentObject);
  SigModelInvalidated();
}

void ObjModel::queueFlushAll() {
  for (auto pmodel : gAllObjectModels)
    pmodel->queueFlush();
  ork::msleep(0);
}

///////////////////////////////////////////////////////////////////////////////

persistantmap_ptr_t ObjModel::persistMapForHash(const PersistHashContext& Ctx) {
  persistantmap_ptr_t prval = 0;
  int key                   = Ctx.hash();
  auto& the_map             = _persistMapContainer->_prop_persist_map;
  auto it                   = the_map.find(key);
  if (it == the_map.end()) {
    prval = std::make_shared<PersistantMap>();
    the_map[key]=prval;
  } else
    prval = it->second;
  return prval;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void ObjModel::attach(
    object_ptr_t root_object,          //
    bool bclearstack,                  //
    geditemnode_ptr_t top_root_item) { //

  this->_rootObject = root_object;
  bool bnewobj      = (_currentObject != _rootObject);

  if (bclearstack) {
    while (not _browseStack.empty())
      _browseStack.pop();
  }
  if (_rootObject) {
    _currentObject = _rootObject;
    if (bclearstack)
      browseStackPush(_currentObject);
  }

  if (_gedWidget) {
    if (top_root_item) { // partial tree (starting at top_root_item) ?
      //_gedWidget->PushItemNode(top_root_item);
      recurse(root_object);
      //_gedWidget->PopItemNode(top_root_item);
    } else { // full tree
      //_gedWidget->GetRootItem()->DestroyChildren();
      if (root_object) {
        //_gedWidget->PushItemNode(_gedWidget->GetRootItem());
        recurse(root_object);
        //_gedWidget->PopItemNode(_gedWidget->GetRootItem());
      }
      detach();
    }
    // if (_gedWidget->GetViewport()) {
    // if (bnewobj)
    //_gedWidget->GetViewport()->ResetScroll();
    //}
    //_gedWidget->DoResize();
  }
  // dont spam refresh, please
  if (_enablePaint)
    SigRepaint();
}

//////////////////////////////////////////////////////////////////////////////

void ObjModel::detach() {
  _rootObject = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::browseStackPush(object_ptr_t pobj) {
  _browseStack.push(pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::browseStackPop() {
  _browseStack.pop();
}

///////////////////////////////////////////////////////////////////////////////

object_ptr_t ObjModel::browseStackTop() const {
  object_ptr_t rval = 0;
  if (_browseStack.size())
    rval = _browseStack.top();
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

int ObjModel::browseStackSize() const {
  return _browseStack.size();
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::dump(const char* header) const {
  printf("OBJMODELDUMP<%s>\n", header);

  std::queue<geditemnode_ptr_t> item_queue;
  if (_gedWidget) {
    // item_queue.push(qw->GetRootItem());
  }
  while (!item_queue.empty()) {
    // geditemnode_ptr_t node = item_queue.front();
    item_queue.pop();
    // int inumc = node->GetNumItems();
    // for (int ic = 0; ic < inumc; ic++) {
    // geditemnode_ptr_t pchild = node->GetItem(ic);
    // item_queue.push(pchild);
    //}
    // printf("NODE<%08x> Name<%s>\n", node, node->_propname.c_str());
  }
}

//////////////////////////////////////////////////////////////////////////////

geditemnode_ptr_t ObjModel::recurse(object_ptr_t root_object, //
                                    const char* pname, //
                                    bool binline) {

  /*GedItemNode* rval    = 0;
  ork::Object* cur_obj = root_object;
  if (cur_obj) {
    ObjectGedVisitEvent gev;
    cur_obj->Notify(&gev);
  }
  auto objclass = rtti::downcast<object::ObjectClass*>(cur_obj->GetClass());

  ///////////////////////////////////////////////////
  // editor.object.ops
  ///////////////////////////////////////////////////
  auto obj_ops_anno = objclass->Description().classAnnotation("editor.object.ops");

  bool is_const_string = obj_ops_anno.IsSet() && obj_ops_anno.IsA<ConstString>();
  bool is_op_map       = obj_ops_anno.IsSet() && obj_ops_anno.IsA<ork::reflect::OpMap*>();

  // ConstString obj_ops = obj_ops_anno.IsSet() ? obj_ops_anno.Get<ConstString>() : "";
  const char* usename         = (pname != 0) ? pname : cur_obj->GetClass()->Name().c_str();
  GedGroupNode* ObjContainerW = binline ? 0 : new GedGroupNode(*this, usename, 0, cur_obj, true);
  if (cur_obj == root_object) {
    rval = ObjContainerW;
  }
  if (ObjContainerW) {
    GetGedWidget()->AddChild(ObjContainerW);
    GetGedWidget()->PushItemNode(ObjContainerW);
  }
  if (is_const_string || is_op_map) {
    OpsNode* popnode = new OpsNode(*this, "ops", 0, cur_obj);
    GetGedWidget()->AddChild(popnode);
  }
  ///////////////////////////////////////////////////
  // editor.class
  ///////////////////////////////////////////////////

  auto ClassEditorAnno = objclass->Description().classAnnotation("editor.class");
  if (auto as_conststr = ClassEditorAnno.TryAs<ConstString>()) {
    ConstString anno_edclass = as_conststr.value();
    if (anno_edclass.length()) {
      rtti::Class* AnnoEditorClass = rtti::Class::FindClass(anno_edclass);
      if (AnnoEditorClass) {
        ork::object::ObjectClass* pclass = rtti::safe_downcast<ork::object::ObjectClass*>(AnnoEditorClass);
        OrkAssert(pclass != nullptr);
        ork::rtti::ICastable* factory = pclass->CreateObject();
        GedFactory* qf                = rtti::safe_downcast<GedFactory*>(factory);
        OrkAssert(qf != nullptr);
        if (qf) {
          if (pname == 0)
            pname = anno_edclass.c_str();

          GetGedWidget()->AddChild(qf->CreateItemNode(*this, pname, 0, root_object));

          if (ObjContainerW) {
            GetGedWidget()->PopItemNode(ObjContainerW);
            ObjContainerW->CheckVis();
          }
          GetGedWidget()->DoResize();
          return rval;
        }
      }
    }
  }

  ///////////////////////////////////////////////////
  // walk classes to root class
  // mark properties, optionally sorting them by "editor.prop.groups" annotation
  ///////////////////////////////////////////////////
  sortnode root_sortnode;
  EnumerateNodes(root_sortnode, objclass);
  ///////////////////////////////////////////////////
  // walk marked properties
  ///////////////////////////////////////////////////
  std::queue<const sortnode*> sort_stack;
  sort_stack.push(&root_sortnode);
  int igcount = 0;
  while (sort_stack.empty() == false) {
    const sortnode* snode = sort_stack.front();
    sort_stack.pop();
    ////////////////////////////////////////////////////////////////////////////////////////
    GedGroupNode* PropGroupNode = 0;
    if (igcount) {
      const std::string& GroupName = snode->Name;
      PropGroupNode                = new GedGroupNode(*this, GroupName.c_str(), 0, cur_obj);
      GetGedWidget()->AddChild(PropGroupNode);
      GetGedWidget()->PushItemNode(PropGroupNode);
    }
    ////////////////////////////////////////////////////////////////////////////////////////
    { // Possibly In Group
      ////////////////////////////////////////////////////////////////////////////////////////
      for (auto item : snode->PropVect) {
        const std::string& Name              = item.first;
        const reflect::ObjectProperty* prop = item.second;
        GedItemNode* PropContainerW          = 0;
        if (0 == prop)
          continue;
        if (false == IsNodeVisible(prop))
          continue;
        //////////////////////////////////////////////////
        PropContainerW = CreateNode(Name, prop, cur_obj);
        //////////////////////////////////////////////////
        if (PropContainerW)
          GetGedWidget()->AddChild(PropContainerW);
        //////////////////////////////////////////////////
      }
      ////////////////////////////////////////////////////////////////////////////////////////
    } // Possibly In Group
    ////////////////////////////////////////////////////////////////////////////////////////
    if (PropGroupNode) {
      GetGedWidget()->PopItemNode(PropGroupNode);
      PropGroupNode->CheckVis();
    }
    ////////////////////////////////////////////////////////////////////////////////////////
    for (const auto& it : snode->GroupVect) {
      const std::string& Name     = it.first;
      const sortnode* child_group = it.second;
      sort_stack.push(child_group);
      igcount++;
    }
    ////////////////////////////////////////////////////////////////////////////////////////
  }
  if (ObjContainerW) {
    GetGedWidget()->PopItemNode(ObjContainerW);
    ObjContainerW->CheckVis();
  }
  GetGedWidget()->DoResize();
  return rval;
  */
   return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////

geditemnode_ptr_t ObjModel::createNode(
    const std::string& Name,             //
    const reflect::ObjectProperty* prop, //
    object_ptr_t pobject) {
  /*
  rtti::Class* AnnoEditorClass = 0;
  /////////////////////////////////////////////////////////////////////////
  // check editor class anno on property
  /////////////////////////////////////////////////////////////////////////
  ConstString anno_edclass = prop->GetAnnotation("editor.class");
  if (anno_edclass.length()) {
    AnnoEditorClass = rtti::Class::FindClass(anno_edclass);
  }
  /////////////////////////////////////////////////////////////////////////
  if (AnnoEditorClass) {
    ork::object::ObjectClass* pclass = rtti::safe_downcast<ork::object::ObjectClass*>(AnnoEditorClass);
    ork::rtti::ICastable* factory    = pclass->CreateObject();
    GedFactory* qf                   = rtti::safe_downcast<GedFactory*>(factory);
    if (qf) {
      return qf->CreateItemNode(*this, Name.c_str(), prop, pobject);
    }
  }
  /////////////////////////////////////////////////////////////////////////
  const reflect::IArray* ArrayProp = rtti::autocast(prop);
  /////////////////////////////////////////////////////////////////////////
  ConstString anno_ucdclass  = prop->GetAnnotation("ged.userchoice.delegate");
  bool HasUserChoiceDelegate = (anno_ucdclass.length());
  /////////////////////////////////////////////////////////////////////////
  if (const reflect::ITyped<Char8>* c8prop = rtti::autocast(prop))
    return new GedLabelNode(*this, Name.c_str(), prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::ITyped<PoolString>* psprop = rtti::autocast(prop))
    return new GedSimpleNode<GedIoDriver<PoolString>, PoolString>(*this, Name.c_str(), psprop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::ITyped<bool>* boolprop = rtti::autocast(prop))
    return new GedBoolNode<PropSetterObj>(*this, Name.c_str(), boolprop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::ITyped<float>* floatprop = rtti::autocast(prop))
    return new GedFloatNode<GedIoDriver<float>>(*this, Name.c_str(), floatprop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::ITyped<fvec4>* vec4prop = rtti::autocast(prop))
    return new GedSimpleNode<GedIoDriver<fvec4>, fvec4>(*this, Name.c_str(), vec4prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::ITyped<fvec3>* vec3prop = rtti::autocast(prop))
    return new GedSimpleNode<GedIoDriver<fvec3>, fvec3>(*this, Name.c_str(), vec3prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::ITyped<fvec2>* vec2prop = rtti::autocast(prop))
    return new GedSimpleNode<GedIoDriver<fvec2>, fvec2>(*this, Name.c_str(), vec2prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::ITyped<fmtx4>* mtx44prop = rtti::autocast(prop))
    return new GedSimpleNode<GedIoDriver<fmtx4>, fmtx4>(*this, Name.c_str(), mtx44prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::ITyped<TransformNode>* xfprop = rtti::autocast(prop)) {
    return new GedSimpleNode<GedIoDriver<TransformNode>, TransformNode>(*this, Name.c_str(), xfprop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::ITyped<ork::rtti::ICastable*>* castprop = rtti::autocast(prop)) {
    return new GedObjNode<PropSetterObj>(*this, Name.c_str(), prop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::ITyped<int>* intprop = rtti::autocast(prop)) {
    return HasUserChoiceDelegate ? (GedItemNode*)new GedSimpleNode<GedIoDriver<int>, int>(*this, Name.c_str(), intprop, pobject)
                                 : (GedItemNode*)new GedIntNode<GedIoDriver<int>>(*this, Name.c_str(), intprop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObject* objprop = rtti::autocast(prop)) {
    ork::Object* psubobj = objprop->Access(pobject);
    if (psubobj)
      Recurse(psubobj, Name.c_str());
    else
      return new GedObjNode<PropSetterObj>(*this, Name.c_str(), prop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::DirectTypedMap<ent::SceneData::SystemDataLut>* MapProp = rtti::autocast(prop)) {
    auto mapprop = rtti::downcast<const reflect::DirectTypedMap<ent::SceneData::SystemDataLut>*>(prop);
    if (mapprop)
      return new GedMapNode(*this, Name.c_str(), mapprop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::DirectTyped<ork::Object*>* dobjprop = rtti::autocast(prop)) {
    ork::Object* psubobj = 0;
    dobjprop->Get(psubobj, pobject);
    if (psubobj)
      Recurse(psubobj);
    return new GedObjNode<PropSetterObj>(*this, Name.c_str(), prop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IMap* MapProp = rtti::autocast(prop)) {
    auto mapprop = rtti::downcast<const reflect::IMap*>(prop);
    if (mapprop)
      return new GedMapNode(*this, Name.c_str(), mapprop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else
    return new GedLabelNode(*this, Name.c_str(), prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  */
  return nullptr;
}
  
} // namespace ork::lev2::ged