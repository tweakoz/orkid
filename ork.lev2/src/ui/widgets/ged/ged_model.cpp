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
#include <ork/lev2/ui/ged/ged_factory.h>
#include <ork/kernel/core_interface.h>
#include <ork/reflect/properties/DirectEnum.h>

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

  _gedContainer->_viewport->onInvalidate();

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
    ///////////////////////////////////////////////////////
    // partial tree (starting at top_root_item) ?
    ///////////////////////////////////////////////////////
    if (top_root_item) {
      _gedContainer->PushItemNode(top_root_item.get());
      recurse(root_object);
      _gedContainer->PopItemNode(top_root_item.get());
    }
    ///////////////////////////////////////////////////////
    // full tree
    ///////////////////////////////////////////////////////
    else {
      _gedContainer->GetRootItem()->_children.clear();
      if (root_object) {
        _gedContainer->PushItemNode(_gedContainer->GetRootItem().get());
        recurse(root_object);
        _gedContainer->PopItemNode(_gedContainer->GetRootItem().get());
      }
      detach();
    }
    ///////////////////////////////////////////////////////
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

  if (0)
    printf("recurse<%p> obj<%p> class<%s>\n", this, cur_obj.get(), objclass->Name().c_str());

  ///////////////////////////////////////////////////
  // editor.class
  ///////////////////////////////////////////////////

  if (auto as_nodefactory_class = objclass->annotationTyped<ConstString>("editor.ged.node.factory")) {
    auto nodefactory_class = as_nodefactory_class.value();
    if (nodefactory_class.length()) {
      rtti::Class* anno_clazz = rtti::Class::FindClass(nodefactory_class.c_str());
      if (0)
        printf("nodefactory_class<%s> anno_clazz<%p>\n", nodefactory_class.c_str(), (void*)anno_clazz);
      if (anno_clazz) {
        auto object_clazz = dynamic_cast<ork::object::ObjectClass*>(anno_clazz);
        OrkAssert(object_clazz != nullptr);
        auto factory       = object_clazz->createShared();
        auto typed_factory = std::dynamic_pointer_cast<GedNodeFactory>(factory);
        OrkAssert(typed_factory != nullptr);
        if (typed_factory) {

          if (pname == 0)
            pname = nodefactory_class.c_str();

          auto group_name = FormatString("grp-%s", pname);

          auto groupnode = std::make_shared<GedGroupNode>(
              _gedContainer,      // mdl
              group_name.c_str(), // name
              nullptr,            // property
              cur_obj,            // object
              true);              // is_obj_node

          _gedContainer->AddChild(groupnode);
          _gedContainer->PushItemNode(groupnode.get());

          /////////////////////////////////////
          // create node with factory
          /////////////////////////////////////

          auto iodriver     = std::make_shared<NewIoDriver>();
          iodriver->_object = root_object;

          auto child = typed_factory->createItemNode(_gedContainer, pname, iodriver);

          _gedContainer->AddChild(child);

          /////////////////////////////////////

          _gedContainer->PopItemNode(groupnode.get());
          groupnode->updateVisibility();

          _gedContainer->DoResize();
          return groupnode;
        }
      }
    }
  }

  ///////////////////////////////////////////////////
  // editor.object.ops
  ///////////////////////////////////////////////////
  auto obj_ops_anno = classdesc.classAnnotation("editor.object.ops");

  bool is_const_string = obj_ops_anno.isSet() and obj_ops_anno.isA<ConstString>();
  bool is_op_map       = obj_ops_anno.isSet() and obj_ops_anno.isA<ork::reflect::OpMap*>();

  // ConstString obj_ops = obj_ops_anno.isSet() ? obj_ops_anno.Get<ConstString>() : "";
  const char* usename          = (pname != 0) ? pname : objclass->Name().c_str();
  gedgroupnode_ptr_t groupnode = binline       //
                                     ? nullptr //
                                     : std::make_shared<GedGroupNode>(
                                           _gedContainer, // mdl
                                           usename,       // name
                                           nullptr,       // property
                                           cur_obj,       // object
                                           true);         // is_obj_node
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
    gedgroupnode_ptr_t groupnode_2 = nullptr;
    if (igcount) {
      const std::string& GroupName = snode->Name;
      groupnode_2                  = std::make_shared<GedGroupNode>(_gedContainer, GroupName.c_str(), nullptr, cur_obj);
      _gedContainer->AddChild(groupnode_2);
      _gedContainer->PushItemNode(groupnode_2.get());
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
        propnode = createObjPropNode(Name, prop, cur_obj);
        //////////////////////////////////////////////////
        if (propnode)
          _gedContainer->AddChild(propnode);
        //////////////////////////////////////////////////
      }
      ////////////////////////////////////////////////////////////////////////////////////////
    } // Possibly In Group
    ////////////////////////////////////////////////////////////////////////////////////////
    if (groupnode_2) {
      _gedContainer->PopItemNode(groupnode_2.get());
      groupnode_2->updateVisibility();
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
    groupnode->updateVisibility();
  }
  _gedContainer->DoResize();
  return rval;
}

////////////////////////////////////////////////////////////////////////////////////////////

geditemnode_ptr_t ObjModel::createAbstractNode(const std::string& Name,
                                               newiodriver_ptr_t iodriver) { //

  if (auto as_obj = iodriver->_abstract_val.tryAs<object_ptr_t>()) {
    ///////////////////////////////////////////////////////
    // object is not null, recurse into it.
    ///////////////////////////////////////////////////////
    if (as_obj.value()) {
      recurse(as_obj.value(), Name.c_str());
    } 
    ///////////////////////////////////////////////////////
    // as_obj is null
    //  see if there is a object factory registered for this property
    //  if so, create a object factory node
    ///////////////////////////////////////////////////////
    else { 
    ///////////////////////////////////////////////////////
      auto anno_edclass = iodriver->_par_prop->GetAnnotation("editor.factorylistbase");
      if (anno_edclass.length()) {
        auto base_clazz   = rtti::Class::FindClass(anno_edclass.c_str());
        auto as_obj_clazz = dynamic_cast<ork::object::ObjectClass*>(base_clazz);
        if (as_obj_clazz) {
          factory_class_set_t factory_set;
          enumerateFactories(as_obj_clazz, factory_set);
          for (auto clazz : factory_set) {
            auto clazz_name = clazz->Name();
            printf("!!! FACTORY<%s>\n", clazz_name.c_str());
          }
          auto factory_node          = std::make_shared<GedFactoryNode>(_gedContainer, Name.c_str(), iodriver);
          factory_node->_factory_set = factory_set;
          _gedContainer->AddChild(factory_node);
          return factory_node;
        }
      }
    }
  }
  return std::make_shared<GedLabelNode>(_gedContainer, Name.c_str(), iodriver->_par_prop, iodriver->_object);
}

////////////////////////////////////////////////////////////////////////////////////////////

geditemnode_ptr_t ObjModel::createObjPropNode(
    const std::string& Name,             //
    const reflect::ObjectProperty* prop, //
    object_ptr_t pobject) {

  rtti::Class* node_factory_class = nullptr;
  /////////////////////////////////////////////////////////////////////////
  // check editor class anno on property
  /////////////////////////////////////////////////////////////////////////
  ConstString anno_node_factory_class = prop->GetAnnotation("editor.node.factory");
  if (anno_node_factory_class.length()) {
    node_factory_class = rtti::Class::FindClass(anno_node_factory_class.c_str());
  }
  /////////////////////////////////////////////////////////////////////////
  if (node_factory_class) {
    auto clazz         = rtti::safe_downcast<ork::object::ObjectClass*>(node_factory_class);
    auto factory       = clazz->createShared();
    auto typed_factory = std::dynamic_pointer_cast<GedNodeFactory>(factory);
    if (typed_factory) {
      auto iodriver       = std::make_shared<NewIoDriver>();
      iodriver->_par_prop = prop;
      iodriver->_object   = pobject;
      return typed_factory->createItemNode(_gedContainer, Name.c_str(), iodriver);
    }
  }
  /////////////////////////////////////////////////////////////////////////
  auto array_prop = dynamic_cast<const reflect::IArray*>(prop);
  auto map_prop   = dynamic_cast<const reflect::IMap*>(prop);
  /////////////////////////////////////////////////////////////////////////
  ConstString anno_ucdclass  = prop->GetAnnotation("ged.userchoice.delegate");
  bool HasUserChoiceDelegate = (anno_ucdclass.length());
  /////////////////////////////////////////////////////////////////////////
  if (map_prop) {
    return std::make_shared<GedMapNode>(_gedContainer, Name.c_str(), map_prop, pobject);
  } else if (array_prop) {
    return std::make_shared<GedArrayNode>(_gedContainer, Name.c_str(), array_prop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (auto as_bool_prop = dynamic_cast<const reflect::ITyped<bool>*>(prop)) {
    auto iodriver      = std::make_shared<NewIoDriver>();
    bool initial_value = false;
    as_bool_prop->get(initial_value, pobject);
    iodriver->_par_prop       = prop;
    iodriver->_object         = pobject;
    iodriver->_abstract_val   = initial_value;
    iodriver->_onValueChanged = [=]() {
      as_bool_prop->set(iodriver->_abstract_val.get<bool>(), pobject);
    };
    return std::make_shared<GedBoolNode>(_gedContainer, Name.c_str(), iodriver);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (auto as_int_prop = dynamic_cast<const reflect::ITyped<int>*>(prop)) {
    auto iodriver     = std::make_shared<NewIoDriver>();
    int initial_value = 0;
    as_int_prop->get(initial_value, pobject);
    iodriver->_par_prop       = prop;
    iodriver->_object         = pobject;
    iodriver->_abstract_val   = initial_value;
    iodriver->_onValueChanged = [=]() {
      as_int_prop->set(iodriver->_abstract_val.get<int>(), pobject);
    };
    return std::make_shared<GedIntNode>(_gedContainer, Name.c_str(), iodriver);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (auto as_float_prop = dynamic_cast<const reflect::ITyped<float>*>(prop)) {
    auto iodriver       = std::make_shared<NewIoDriver>();
    float initial_value = 0;
    as_float_prop->get(initial_value, pobject);
    iodriver->_par_prop       = prop;
    iodriver->_object         = pobject;
    iodriver->_abstract_val   = initial_value;
    iodriver->_onValueChanged = [=]() {
      as_float_prop->set(iodriver->_abstract_val.get<float>(), pobject);
    };
    return std::make_shared<GedFloatNode>(_gedContainer, Name.c_str(), iodriver);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (auto as_enum_prop = dynamic_cast<const reflect::DirectEnumBase*>(prop)) {
    auto iodriver       = std::make_shared<NewIoDriver>();
    float initial_value = 0;
    //as_enum_prop->get(initial_value, pobject);
    iodriver->_par_prop       = prop;
    iodriver->_object         = pobject;
    iodriver->_abstract_val   = initial_value;
    iodriver->_onValueChanged = [=]() {
      OrkAssert(false);
      //as_enum_prop->set(iodriver->_abstract_val.get<float>(), pobject);
    };
    return std::make_shared<GedEnumNode>(_gedContainer, Name.c_str(), iodriver);

  }
  /////////////////////////////////////////////////////////////////////////
  else if (auto as_dobjprop = dynamic_cast<const reflect::DirectObjectBase*>(prop)) {
    object_ptr_t child_object = as_dobjprop->getObject(pobject);
    auto iodriver             = std::make_shared<NewIoDriver>();
    iodriver->_par_prop       = prop;
    iodriver->_object         = pobject;
    iodriver->_abstract_val.set<object_ptr_t>(child_object);
    
    iodriver->_onValueChanged = [=]() {
      // ary_prop->setElement(obj, key, iodriver->_abstract_val);
      this->enqueueUpdate();
    };
    std::string itemstr;
    if (child_object) {
      auto clazz = child_object->GetClass();
      //auto try_namer = clazz->annotationTyped<reflect::obj_to_string_fn_t>("editor.ged.item.namer");
      //if (try_namer) {
      //  itemstr = try_namer.value()(e.get<object_ptr_t>());
      //}
      itemstr = clazz->Name().c_str();
    }
    else{
      auto anno = prop->GetAnnotation("editor.factorylistbase");
      itemstr = FormatString("FACTORY[ %s ]",  anno.c_str() );
    }
    auto item_node = this->createAbstractNode(itemstr.c_str(), iodriver);
  }
  /////////////////////////////////////////////////////////////////////////
  /*
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
  else if (const reflect::ITyped<int>* intprop = rtti::autocast(prop)) {
    return HasUserChoiceDelegate ? (GedItemNode*)new GedSimpleNode<GedIoDriver<int>, int>(*this, Name.c_str(), intprop, pobject)
                                 : (GedItemNode*)new GedIntNode<GedIoDriver<int>>(*this, Name.c_str(), intprop, pobject);
  }
  */
  else{
    if (auto as_nodefactory_class = prop->typedAnnotation<ConstString>("editor.ged.node.factory")) {
      auto nodefactory_class = as_nodefactory_class.value();
      if (nodefactory_class.length()) {
        rtti::Class* anno_clazz = rtti::Class::FindClass(nodefactory_class.c_str());
        if (0)
          printf("nodefactory_class<%s> anno_clazz<%p>\n", nodefactory_class.c_str(), (void*)anno_clazz);
        if (anno_clazz) {
          auto object_clazz = dynamic_cast<ork::object::ObjectClass*>(anno_clazz);
          OrkAssert(object_clazz != nullptr);
          auto factory       = object_clazz->createShared();
          auto typed_factory = std::dynamic_pointer_cast<GedNodeFactory>(factory);
          OrkAssert(typed_factory != nullptr);
          if (typed_factory) {

            /////////////////////////////////////
            // create node with factory
            /////////////////////////////////////

            auto iodriver     = std::make_shared<NewIoDriver>();
            iodriver->_object = pobject;
            iodriver->_par_prop = prop;

            auto child = typed_factory->createItemNode(_gedContainer, prop->_name.c_str(), iodriver);

            //_gedContainer->AddChild(child);
            /////////////////////////////////////
            //_gedContainer->PopItemNode(groupnode.get());
            //groupnode->updateVisibility();
            //_gedContainer->DoResize();
            return child;
          }
        }
      }
    }

  }  
  return std::make_shared<GedLabelNode>(_gedContainer, Name.c_str(), prop, pobject);
}

////////////////////////////////////////////////////////////////////////////////////////////

bool ObjModel::IsNodeVisible(const reflect::ObjectProperty* prop) {
  auto anno_vis       = prop->typedAnnotation<bool>("editor.visible");
  ConstString anno_ediftageq = prop->GetAnnotation("editor.iftageq");
  if (anno_vis) {
    if ( not anno_vis.value() )
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

  ///////////////////////////////////////////////////////////
  // walk up class heirarchy to Object, enumerating..
  ///////////////////////////////////////////////////////////

  object::ObjectClass* walk_class = the_class;
  orkvector<object::ObjectClass*> ClassVect;
  while (walk_class != ork::Object::GetClassStatic()) {
    ClassVect.push_back(walk_class);
    walk_class = rtti::downcast<ork::object::ObjectClass*>(walk_class->Parent());
  }

  ///////////////////////////////////////////////////////////
  // in heirarchy order, enumerate properties
  ///////////////////////////////////////////////////////////

  int inumclasses = int(ClassVect.size());
  for (int ic = (inumclasses - 1); ic >= 0; ic--) {
    object::ObjectClass* clazz = ClassVect[ic];
    const auto& class_desc     = clazz->Description();
    auto& propmap              = class_desc.properties();
    auto as_group_conststr     = clazz->annotationTyped<ConstString>("editor.prop.groups");
    ///////////////////////////////////////////////////////////
    // overridden order from editor.prop.groups
    ///////////////////////////////////////////////////////////
    if (as_group_conststr) {
      auto eg = as_group_conststr.value().c_str();
      if (1)
        printf("!!!!! enumerate props for class<%s> (overridden order %s)\n", clazz->Name().c_str(), eg);
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
            const std::string& str             = (*itp);
            auto itf                           = propmap.find(str.c_str());
            ork::reflect::ObjectProperty* prop = (itf != propmap.end()) ? itf->second : 0;
            if (prop) {
              pnode->PropVect.push_back(std::make_pair(str.c_str(), prop));
            }
          }
      }
      /////////////////////////////////////////////////
    }
    ///////////////////////////////////////////////////////////
    // default order
    ///////////////////////////////////////////////////////////
    else {
      if (0)
        printf("enumerate props for class<%s> (default order)\n", clazz->Name().c_str());
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
        // printf( "prop name<%s>\n", Name.c_str() );
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
}

} // namespace ork::lev2::ged