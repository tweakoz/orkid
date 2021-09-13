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
#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/editorsupport/objectmodel.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>

#include <ork/file/path.h>
#include <ork/util/crc.h>
#include <signal.h>
///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::reflect::editor::ObjectModel, "ObjectModel");

namespace ork::reflect::editor {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void ObjectModel::Describe() {
  //////////////////////////////////////////////////////////////////
  // RegisterAutoSignal(ObjectModel, Repaint);
  RegisterAutoSignal(ObjectModel, ModelInvalidated);
  RegisterAutoSignal(ObjectModel, PreNewObject);
  RegisterAutoSignal(ObjectModel, PropertyInvalidated);
  RegisterAutoSignal(ObjectModel, NewObject);
  // RegisterAutoSignal(ObjectModel, SpawnNewGed);
  //////////////////////////////////////////////////////////////////
  RegisterAutoSlot(ObjectModel, NewObject);
  RegisterAutoSlot(ObjectModel, ObjectDeleted);
  RegisterAutoSlot(ObjectModel, ObjectSelected);
  RegisterAutoSlot(ObjectModel, ObjectDeSelected);
  RegisterAutoSlot(ObjectModel, RelayModelInvalidated);
  RegisterAutoSlot(ObjectModel, RelayPropertyInvalidated);
  //////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::SlotRelayPropertyInvalidated(ork::object_ptr_t pobj, const reflect::ObjectProperty* prop) {
  if (_observer)
    _observer->PropertyInvalidated(pobj, prop);
  // Attach( mCurrentObject );
  // SigModelInvalidated();
}

void ObjectModel::SlotRelayModelInvalidated() {
  Attach(mCurrentObject);
  // SigModelInvalidated();
}

orkset<objectmodel_ptr_t> ObjectModel::gAllObjectModels;
objectmodel_ptr_t ObjectModel::create() {
  auto model = std::make_shared<ObjectModel>();
  gAllObjectModels.insert(model);
  return model;
}
void ObjectModel::release(objectmodel_ptr_t model) {
  gAllObjectModels.erase(model);
}

ObjectModel::ObjectModel()
    : mModelInvalidatedInvoker(mSignalModelInvalidated.CreateInvokation())
    , mbEnablePaint(false)
    , ConstructAutoSlot(NewObject)
    , ConstructAutoSlot(RelayModelInvalidated)
    , ConstructAutoSlot(RelayPropertyInvalidated)
    , ConstructAutoSlot(ObjectDeleted)
    , ConstructAutoSlot(ObjectSelected)
    , ConstructAutoSlot(ObjectDeSelected)
    , ConstructAutoSlot(Repaint) {

  AutoConnector::setupSignalsAndSlots(this);
  ///////////////////////////////////////////
  // A Touch Of Class

  // GedFactoryEnum::GetClassStatic();
  // GedFactoryGradient::GetClassStatic();
  // GedFactoryCurve::GetClassStatic();
  // GedFactoryAssetList::GetClassStatic();
  // GedFactoryFileList::GetClassStatic();
  // GedFactoryTransform::GetClassStatic();

  ///////////////////////////////////////////

  object::Connect(&mSignalNewObject, &mSlotNewObject);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::FlushAllQueues() {
  for (auto pmodel : gAllObjectModels)
    pmodel->FlushQueue();
  ork::msleep(0);
}

///////////////////////////////////////////////////////////////////////////////

ObjectModel::~ObjectModel() {
  AutoConnector::disconnectAll(this);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::QueueUpdate() {
  auto lamb = [=]() { FlushQueue(); };
  opq::updateSerialQueue()->enqueue(lamb);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::QueueUpdateAll() {
  auto lamb = [=]() { FlushAllQueues(); };
  opq::updateSerialQueue()->enqueue(lamb);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::FlushQueue() {
  // printf("ObjectModel::FlushQueue\n");
  Attach(CurrentObject());
  SigModelInvalidated();
  // SigNewObject(pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::SigModelInvalidated() {
  // printf("ObjectModel::SigModelInvalidated\n");
  mSignalModelInvalidated(&ObjectModel::SigModelInvalidated);
}
void ObjectModel::SigPreNewObject() {
  auto lamb = [=]() { this->mSignalPreNewObject(&ObjectModel::SigPreNewObject); };
  opq::updateSerialQueue()->enqueue(lamb);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::SigPropertyInvalidated(
    ork::object_ptr_t pobj, //
    const reflect::ObjectProperty* prop) {
  auto lamb = [=]() { this->mSignalPropertyInvalidated(&ObjectModel::SigPropertyInvalidated, pobj, prop); };
  opq::updateSerialQueue()->enqueue(lamb);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::SigRepaint() {
  mSignalRepaint(&ObjectModel::SigRepaint);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::SigNewObject(ork::object_ptr_t pobj) {
  SigPreNewObject();
  mSignalNewObject(&ObjectModel::SigNewObject, pobj);
  SigRepaint();
  SigPostNewObject(pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::SigPostNewObject(ork::object_ptr_t pobj) {
  mSignalPostNewObject(&ObjectModel::SigPostNewObject, pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::SigSpawnNewGed(ork::object_ptr_t pOBJ) {
  mSignalSpawnNewGed(&ObjectModel::SigSpawnNewGed, pOBJ);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::SlotNewObject(ork::object_ptr_t pobj) {
  Attach(mCurrentObject);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::SlotObjectDeleted(ork::object_ptr_t pobj) {
  Attach(0);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::ProcessQueue() {
  if (mQueueObject)
    Attach(mQueueObject);
  mQueueObject = 0;
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::SlotObjectSelected(ork::object_ptr_t pobj) {
  // printf("ObjectModel<%p> Object<%p> selected\n", this, pobj);
  Attach(pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::SlotObjectDeSelected(ork::object_ptr_t pobj) {
  Attach(nullptr);
}

//////////////////////////////////////////////////////////////////////////////
/*objectmodelnode_ptr_t ObjectModel::Recurse(
    ork::object_ptr_t root_object, //
    const char* pname,
    bool binline) {

  objectmodelnode_ptr_t rval = 0;
  ork::object_ptr_t cur_obj       = root_object;
  if (cur_obj) {
    ObjectGedVisitEvent gev;
    cur_obj->Notify(&gev);
  }
  auto objclass = rtti::downcast<object::ObjectClass*>(cur_obj->GetClass());

  ///////////////////////////////////////////////////
  // editor.object.ops
  ///////////////////////////////////////////////////
  auto obj_ops_anno = objclass->Description().classAnnotation("editor.object.ops");

  bool is_const_string = obj_ops_anno.Isset() && obj_ops_anno.isA<ConstString>();
  bool is_op_map       = obj_ops_anno.Isset() && obj_ops_anno.isA<ork::reflect::OpMap*>();

  // ConstString obj_ops = obj_ops_anno.Isset() ? obj_ops_anno.get<ConstString>() : "";
  const char* usename = (pname != 0) ? pname : cur_obj->GetClass()->Name().c_str();
  auto ObjContainerW  = binline //
                           ? objectmodelnode_ptr_t(nullptr)
                           : _observer->createGroup(
                                 this, //
                                 usename,
                                 0,
                                 cur_obj,
                                 true);
  if (cur_obj == root_object) {
    rval = ObjContainerW;
  }
  if (ObjContainerW) {
    _observer->AddChild(ObjContainerW);
    _observer->PushItemNode(ObjContainerW);
  }
  if (is_const_string || is_op_map) {
    OpsNode* popnode = new OpsNode(*this, "ops", 0, cur_obj);
    _observer->AddChild(popnode);
  }
  ///////////////////////////////////////////////////
  // editor.class
  ///////////////////////////////////////////////////

  auto ClassEditorAnno = objclass->Description().classAnnotation("editor.class");
  if (auto as_conststr = ClassEditorAnno.tryAs<ConstString>()) {
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
}*/
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
/*
bool ObjectModel::IsNodeVisible(const reflect::ObjectProperty* prop) {
  ConstString anno_vis       = prop->GetAnnotation("editor.visible");
  ConstString anno_ediftageq = prop->GetAnnotation("editor.iftageq");
  if (anno_vis.length()) {
    if (0 == strcmp(anno_vis.c_str(), "false"))
      return false;
  }
  if (anno_ediftageq.length()) {
    orkvector<std::string> AnnoSplit;
    SplitString(std::string(anno_ediftageq.c_str()), AnnoSplit, ":");
    OrkAssert(AnnoSplit.size() == 2);
    const std::string& key                                 = AnnoSplit[0];
    const std::string& val                                 = AnnoSplit[1];
    GedItemNode* parentnode                                = GetGedWidget()->topItemNode();
    orkmap<std::string, std::string>::const_iterator ittag = parentnode->mTags.find(key);
    if (ittag != parentnode->mTags.end()) {
      if (val != ittag->second) {
        return false;
      }
    }
  }
  return true;
}
*/
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void ObjectModel::EnumerateNodes(
    sortnode& in_node, //
    object::ObjectClass* the_class) {
  object::ObjectClass* walk_class = the_class;
  orkvector<object::ObjectClass*> ClassVect;
  while (walk_class != ork::Object::GetClassStatic()) {
    ClassVect.push_back(walk_class);
    walk_class = rtti::downcast<ork::object::ObjectClass*>(walk_class->Parent());
  }
  int inumclasses = int(ClassVect.size());
  for (int ic = (inumclasses - 1); ic >= 0; ic--) {
    object::ObjectClass* pclass                         = ClassVect[ic];
    ork::reflect::Description::PropertyMapType& propmap = pclass->Description().properties();
    auto eg_anno                                        = pclass->Description().classAnnotation("editor.prop.groups");
    auto as_conststr                                    = eg_anno.tryAs<const char*>();
    const char* eg                                      = "";
    if (as_conststr)
      eg = as_conststr.value();
    if (strlen(eg)) {
      FixedString<1024> str_rep = eg;
      str_rep.replace_in_place("//", "// ");
      tokenlist src_toklist = CreateTokenList(str_rep.c_str(), " ");
      /////////////////////////////////////////////////
      // enumerate groups
      /////////////////////////////////////////////////
      struct yo {
        std::string mUrlType;
        tokenlist mTokens;
      };
      orkvector<yo> Groups;
      tokenlist* curtoklist = 0;
      for (auto group : src_toklist) {
        ork::file::Path aspath(group.c_str());
        if (aspath.HasUrlBase()) { // START NEW GROUP
          Groups.push_back(yo());
          orkvector<yo>::iterator itnew = Groups.end() - 1;
          curtoklist                    = &itnew->mTokens;
          itnew->mUrlType               = group;
        } else { // ADD TO LAST GROUP
          if (curtoklist) {
            curtoklist->push_back(group);
          }
        }
      }
      /////////////////////////////////////////////////
      // process groups
      /////////////////////////////////////////////////
      for (const auto& grp : Groups) {
        const std::string& UrlType    = grp.mUrlType;
        const tokenlist& iter_toklist = grp.mTokens;
        tokenlist::const_iterator itp = iter_toklist.end();
        sortnode* pnode               = 0;
        if (UrlType.find("grp") != std::string::npos) { // GroupNode
          itp                          = iter_toklist.begin();
          const std::string& GroupName = (*itp);
          std::pair<std::string, sortnode*> the_pair;
          the_pair.first  = GroupName;
          the_pair.second = new sortnode;
          in_node.GroupVect.push_back(the_pair);
          pnode       = (in_node.GroupVect.end() - 1)->second;
          pnode->Name = GroupName;
          itp++;
        } else if (UrlType.find("sort") != std::string::npos) { // SortNode
          pnode = &in_node;
          itp   = iter_toklist.begin();
        }
        if (pnode)
          for (; itp != iter_toklist.end(); itp++) {
            const std::string& str                                   = (*itp);
            ork::reflect::Description::PropertyMapType::iterator itf = propmap.find(str.c_str());
            ork::reflect::ObjectProperty* prop                       = (itf != propmap.end()) ? itf->second : 0;
            if (prop) {
              pnode->PropVect.push_back(std::make_pair(str.c_str(), prop));
            }
          }
      }
      /////////////////////////////////////////////////
    } else
      for (auto it : propmap) {
        ///////////////////////////////////////////////////
        // editor.object.props
        ///////////////////////////////////////////////////
        std::set<std::string> allowed_props;
        auto obj_props_anno = the_class->Description().classAnnotation("editor.object.props");
        if (auto as_str = obj_props_anno.tryAs<std::string>()) {
          auto propnameset = as_str.value();
          if (propnameset.length()) {
            auto pvect = ork::SplitString(propnameset, ' ');
            for (const auto& item : pvect)
              allowed_props.insert(item);
          }
        }
        ///////////////////////////////////////////////////
        bool prop_ok            = true;
        const ConstString& Name = it.first;
        if (allowed_props.size()) {
          std::string namstr(Name.c_str());
          prop_ok = allowed_props.find(namstr) != allowed_props.end();
        }
        if (prop_ok) {
          ork::reflect::ObjectProperty* prop = it.second;
          if (prop) {
            in_node.PropVect.push_back(std::make_pair(Name.c_str(), prop));
          }
        }
      }
  }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
/*
GedItemNode* ObjectModel::CreateNode(
    const std::string& Name, //
    const reflect::ObjectProperty* prop,
    object_ptr_t pobject) {
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
  else if (const reflect::Iobject_ptr_t objprop = rtti::autocast(prop)) {
    ork::object_ptr_t psubobj = objprop->Access(pobject);
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
  else if (const reflect::DirectTyped<ork::object_ptr_t>* dobjprop = rtti::autocast(prop)) {
    ork::object_ptr_t psubobj = 0;
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
  return nullptr;
}
*/
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void ObjectModel::Attach(
    ork::object_ptr_t root_object, //
    bool bclearstack,
    objectmodelnode_ptr_t top_root_item) {
  this->mRootObject = root_object;
  bool bnewobj      = (mCurrentObject != mRootObject);

  if (bclearstack) {
    while (false == mBrowseStack.empty())
      mBrowseStack.pop();
  }
  if (mRootObject) {
    mCurrentObject = mRootObject;
    if (bclearstack)
      PushBrowseStack(mCurrentObject);
  }

  if (_observer) {
    _observer->onObjectAttachedToModel(root_object);
  }

  // dont spam refresh, please
  if (mbEnablePaint)
    SigRepaint();
}

//////////////////////////////////////////////////////////////////////////////

void ObjectModel::Detach() {
  mRootObject = 0;
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::PushBrowseStack(ork::object_ptr_t pobj) {
  mBrowseStack.push(pobj);
}

void ObjectModel::PopBrowseStack() {
  mBrowseStack.pop();
}

ork::object_ptr_t ObjectModel::BrowseStackTop() const {
  ork::object_ptr_t rval = 0;
  if (mBrowseStack.size())
    rval = mBrowseStack.top();
  return rval;
}

int ObjectModel::StackSize() const {
  return mBrowseStack.size();
}

///////////////////////////////////////////////////////////////////////////////

void ObjectModel::Dump(const char* header) const {
  orkprintf("OBJMODELDUMP<%s>\n", header);
  std::queue<objectmodelnode_ptr_t> ItemQueue;
  if (_observer) {
    ItemQueue.push(_observer->_rootnode);
  }
  while (!ItemQueue.empty()) {
    objectmodelnode_ptr_t node = ItemQueue.front();
    ItemQueue.pop();
    int inumc = node->GetNumItems();
    for (int ic = 0; ic < inumc; ic++) {
      objectmodelnode_ptr_t pchild = node->GetItem(ic);
      ItemQueue.push(pchild);
    }
    orkprintf("NODE<%p> Name<%s>\n", node.get(), node->_propname.c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::editor
