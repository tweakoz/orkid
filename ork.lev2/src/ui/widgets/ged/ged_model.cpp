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
#include <ork/kernel/core_interface.h>

namespace ork::lev2::ged {

orkset<objectmodel_ptr_t> ObjModel::gAllObjectModels;

objectmodel_ptr_t ObjModel::createShared(opq::opq_ptr_t updateopq) {
  auto objmodel = std::make_shared<ObjModel>(updateopq);
  // AutoConnector::setupSignalsAndSlots(objmodel);
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
  queueFlushAll();
  // gAllObjectModels.erase(this);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotRelayPropertyInvalidated(object_ptr_t pobj, const reflect::ObjectProperty* prop) {
  // if (_gedContainer)
  //_gedContainer->PropertyInvalidated(pobj, prop);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotRelayModelInvalidated() {
  attach(_currentObject);
  // SigModelInvalidated();
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigModelInvalidated() {
  printf("ObjModel::SigModelInvalidated\n");
  _sigModelInvalidated();
  // mSignalModelInvalidated(&ObjModel::SigModelInvalidated); // << operator() instantiated here
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigPreNewObject() {
  _updateOPQ->enqueue([=]() { //
    // this->mSignalPreNewObject(&ObjModel::SigPreNewObject);
  });
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigPropertyInvalidated(object_ptr_t pobj, const reflect::ObjectProperty* prop) {
  _updateOPQ->enqueue([=]() { //
    // this->mSignalPropertyInvalidated(&ObjModel::SigPropertyInvalidated, pobj, prop);
  });
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::emitRepaint() {
  _sigRepaint();
  // mSignalRepaint(&ObjModel::SigRepaint);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigNewObject(object_ptr_t pobj) {
  // SigPreNewObject();
  // mSignalNewObject(&ObjModel::SigNewObject, pobj);
  // SigRepaint();
  // SigPostNewObject(pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigPostNewObject(object_ptr_t pobj) {
  // mSignalPostNewObject(&ObjModel::SigPostNewObject, pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigSpawnNewGed(object_ptr_t pOBJ) {
  // mSignalSpawnNewGed(&ObjModel::SigSpawnNewGed, pOBJ);
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
    prval        = std::make_shared<PersistantMap>();
    the_map[key] = prval;
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

  if (_gedContainer) {
    if (top_root_item) { // partial tree (starting at top_root_item) ?
      _gedContainer->PushItemNode(top_root_item.get());
      recurse(root_object);
      _gedContainer->PopItemNode(top_root_item.get());
    } else { // full tree
      _gedContainer->GetRootItem()->_children.clear();
      if (root_object) {
        _gedContainer->PushItemNode(_gedContainer->GetRootItem().get());
        recurse(root_object);
        _gedContainer->PopItemNode(_gedContainer->GetRootItem().get());
      }
      detach();
    }
    if (_gedContainer->_viewport) {
      if (bnewobj)
        _gedContainer->_viewport->ResetScroll();
    }
    _gedContainer->DoResize();
  } // if(_gedContainer)

  // dont spam refresh, please
  if (_enablePaint)
    emitRepaint();
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
  if (_gedContainer) {
    item_queue.push(_gedContainer->GetRootItem());
  }
  while (!item_queue.empty()) {
    geditemnode_ptr_t node = item_queue.front();
    item_queue.pop();
    int inumc = node->numChildren();
    for (int ic = 0; ic < inumc; ic++) {
      geditemnode_ptr_t pchild = node->_children[ic];
      item_queue.push(pchild);
    }
    // printf("NODE<%08x> Name<%s>\n", node, node->_propname.c_str());
  }
}

//////////////////////////////////////////////////////////////////////////////

geditemnode_ptr_t ObjModel::recurse(
    object_ptr_t root_object, //
    const char* pname,        //
    bool binline) {

  auto cur_obj = root_object;
  if (cur_obj) {
    cur_obj->notifyX<ObjectGedVisitEvent>();
  }

  auto objclass         = cur_obj->objectClass();
  const auto& classdesc = objclass->Description();

  geditemnode_ptr_t rval = nullptr;

  ///////////////////////////////////////////////////
  // editor.object.ops
  ///////////////////////////////////////////////////
  auto obj_ops_anno = classdesc.classAnnotation("editor.object.ops");

  bool is_const_string = obj_ops_anno.isSet() and obj_ops_anno.isA<ConstString>();
  bool is_op_map       = obj_ops_anno.isSet() and obj_ops_anno.isA<ork::reflect::OpMap*>();

  // ConstString obj_ops = obj_ops_anno.isSet() ? obj_ops_anno.Get<ConstString>() : "";
  const char* usename          = (pname != 0) ? pname : objclass->Name().c_str();
  gedgroupnode_ptr_t groupnode = binline            //
                                     ? nullptr      //
                                     : std::make_shared<GedGroupNode>(
                                           _gedContainer,    // mdl
                                           usename, // name
                                           nullptr, // property
                                           cur_obj, // object
                                           true);   // is_obj_node
  if (cur_obj == root_object) {
    rval = groupnode;
  }
  if (groupnode) {
    _gedContainer->AddChild(groupnode);
    _gedContainer->PushItemNode(groupnode.get());
  }
  if (is_const_string || is_op_map) {
    // OpsNode* popnode = new OpsNode(*this, "ops", 0, cur_obj);
    //_gedContainer->AddChild(popnode);
  }

  ///////////////////////////////////////////////////
  // editor.class
  ///////////////////////////////////////////////////

  auto anno_editor_class = classdesc.classAnnotation("editor.class");

  if (auto as_conststr = anno_editor_class.tryAs<ConstString>()) {
    auto anno_edclass = as_conststr.value();
    if (anno_edclass.length()) {
      rtti::Class* AnnoEditorClass = rtti::Class::FindClass(anno_edclass.c_str());
      if (AnnoEditorClass) {
        auto clazz = rtti::safe_downcast<ork::object::ObjectClass*>(AnnoEditorClass);
        OrkAssert(clazz != nullptr);
        auto factory       = clazz->createShared();
        auto typed_factory = std::dynamic_pointer_cast<GedFactory>(factory);
        OrkAssert(typed_factory != nullptr);
        if (typed_factory) {
          if (pname == 0)
            pname = anno_edclass.c_str();

          auto child = typed_factory->createItemNode(_gedContainer, pname, nullptr, root_object);
          _gedContainer->AddChild(child);

          if (groupnode) {
            _gedContainer->PopItemNode(groupnode.get());
            groupnode->CheckVis();
          }
          _gedContainer->DoResize();
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
    gedgroupnode_ptr_t groupnode = nullptr;
    if (igcount) {
      const std::string& GroupName = snode->Name;
      groupnode                    = std::make_shared<GedGroupNode>(_gedContainer, GroupName.c_str(), nullptr, cur_obj);
      _gedContainer->AddChild(groupnode);
      _gedContainer->PushItemNode(groupnode.get());
    }
    ////////////////////////////////////////////////////////////////////////////////////////
    { // Possibly In Group
      ////////////////////////////////////////////////////////////////////////////////////////
      for (auto item : snode->PropVect) {
        const std::string& Name             = item.first;
        const reflect::ObjectProperty* prop = item.second;
        geditemnode_ptr_t propnode          = nullptr;
        if (0 == prop)
          continue;
        if (false == IsNodeVisible(prop))
          continue;
        //////////////////////////////////////////////////
        propnode = createNode(Name, prop, cur_obj);
        //////////////////////////////////////////////////
        if (propnode)
          _gedContainer->AddChild(propnode);
        //////////////////////////////////////////////////
      }
      ////////////////////////////////////////////////////////////////////////////////////////
    } // Possibly In Group
    ////////////////////////////////////////////////////////////////////////////////////////
    if (groupnode) {
      _gedContainer->PopItemNode(groupnode.get());
      groupnode->CheckVis();
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
  if (groupnode) {
    _gedContainer->PopItemNode(groupnode.get());
    groupnode->CheckVis();
  }
  _gedContainer->DoResize();
  return rval;
}

////////////////////////////////////////////////////////////////////////////////////////////

geditemnode_ptr_t ObjModel::createNode(
    const std::string& Name,             //
    const reflect::ObjectProperty* prop, //
    object_ptr_t pobject) {

  rtti::Class* AnnoEditorClass = 0;
  /////////////////////////////////////////////////////////////////////////
  // check editor class anno on property
  /////////////////////////////////////////////////////////////////////////
  ConstString anno_edclass = prop->GetAnnotation("editor.class");
  if (anno_edclass.length()) {
    AnnoEditorClass = rtti::Class::FindClass(anno_edclass.c_str());
  }
  /////////////////////////////////////////////////////////////////////////
  if (AnnoEditorClass) {
    auto clazz = rtti::safe_downcast<ork::object::ObjectClass*>(AnnoEditorClass);
    auto factory    = clazz->createShared();
    auto typed_factory = std::dynamic_pointer_cast<GedFactory>(factory);
    if (typed_factory) {
      return typed_factory->createItemNode(_gedContainer, Name.c_str(), prop, pobject);
    }
  }
  /////////////////////////////////////////////////////////////////////////
  auto array_prop = dynamic_cast<const reflect::IArray*>(prop);
  /////////////////////////////////////////////////////////////////////////
  ConstString anno_ucdclass  = prop->GetAnnotation("ged.userchoice.delegate");
  bool HasUserChoiceDelegate = (anno_ucdclass.length());
  /////////////////////////////////////////////////////////////////////////
  /*
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
  return std::make_shared<GedLabelNode>(_gedContainer, Name.c_str(), prop, pobject);
}

////////////////////////////////////////////////////////////////////////////////////////////

bool ObjModel::IsNodeVisible(const reflect::ObjectProperty* prop) {
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
    GedItemNode* parentnode                                = _gedContainer->ParentItemNode();
    orkmap<std::string, std::string>::const_iterator ittag = parentnode->mTags.find(key);
    if (ittag != parentnode->mTags.end()) {
      if (val != ittag->second) {
        return false;
      }
    }
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////////

void ObjModel::EnumerateNodes(
    sortnode& in_node,                //
    object::ObjectClass* the_class) { //

  object::ObjectClass* walk_class = the_class;
  orkvector<object::ObjectClass*> ClassVect;
  while (walk_class != ork::Object::GetClassStatic()) {
    ClassVect.push_back(walk_class);
    walk_class = rtti::downcast<ork::object::ObjectClass*>(walk_class->Parent());
  }
  int inumclasses = int(ClassVect.size());
  for (int ic = (inumclasses - 1); ic >= 0; ic--) {
    object::ObjectClass* clazz = ClassVect[ic];
    const auto& class_desc     = clazz->Description();
    auto& propmap              = class_desc.properties();
    auto eg_anno               = class_desc.classAnnotation("editor.prop.groups");
    auto as_conststr           = eg_anno.tryAs<const char*>();
    const char* eg             = "";
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
        if (aspath.hasUrlBase()) { // START NEW GROUP
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
            auto itf = propmap.find(str.c_str());
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

} // namespace ork::lev2::ged