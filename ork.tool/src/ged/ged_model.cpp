///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>

#include <ork/kernel/opq.h>
///////////////////////////////////////////////////////////////////////////////

#include <queue>

#include <ork/reflect/DirectObjectPropertyType.h>
#include <ork/reflect/IObjectArrayProperty.h>
#include <ork/reflect/IObjectMapProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <pkg/ent/scene.h>

#include <ork/util/crc.h>
#include <signal.h>
///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::ObjModel, "ObjModel");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedWidget, "GedWidget");

namespace ork::tool::ged {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void ObjModel::Describe() {
  //////////////////////////////////////////////////////////////////
  RegisterAutoSignal(ObjModel, Repaint);
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
  RegisterAutoSlot(ObjModel, RelayPropertyInvalidated);
  //////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotRelayPropertyInvalidated(ork::Object* pobj, const reflect::IObjectProperty* prop) {
  if (mpGedWidget)
    mpGedWidget->PropertyInvalidated(pobj, prop);
  // Attach( mCurrentObject );
  // SigModelInvalidated();
}

void ObjModel::SlotRelayModelInvalidated() {
  Attach(mCurrentObject);
  // SigModelInvalidated();
}

orkset<ObjModel*> ObjModel::gAllObjectModels;

ObjModel::ObjModel()
    : mpGedWidget(0)
    , mChoiceManager(0)
    , mModelInvalidatedInvoker(mSignalModelInvalidated.CreateInvokation())
    , mQueueObject(0)
    , mCurrentObject(0)
    , mRootObject(0)
    , mbEnablePaint(false)
    , ConstructAutoSlot(NewObject)
    , ConstructAutoSlot(RelayModelInvalidated)
    , ConstructAutoSlot(RelayPropertyInvalidated)
    , ConstructAutoSlot(ObjectDeleted)
    , ConstructAutoSlot(ObjectSelected)
    , ConstructAutoSlot(ObjectDeSelected)
    , ConstructAutoSlot(Repaint) {

  SetupSignalsAndSlots();
  ///////////////////////////////////////////
  // A Touch Of Class

  GedFactoryEnum::GetClassStatic();
  GedFactoryGradient::GetClassStatic();
  GedFactoryCurve::GetClassStatic();
  GedFactoryAssetList::GetClassStatic();
  GedFactoryFileList::GetClassStatic();
  GedFactoryTransform::GetClassStatic();

  ///////////////////////////////////////////

  object::Connect(&mSignalNewObject, &mSlotNewObject);

  gAllObjectModels.insert(this);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::FlushAllQueues() {
  for (auto pmodel : gAllObjectModels)
    pmodel->FlushQueue();
  ork::msleep(0);
}

///////////////////////////////////////////////////////////////////////////////

ObjModel::~ObjModel() {
  DisconnectAll();
  gAllObjectModels.erase(this);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::QueueUpdate() {
  auto lamb = [=]() { FlushQueue(); };
  opq::updateSerialQueue()->enqueue(lamb);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::QueueUpdateAll() {
  auto lamb = [=]() { FlushAllQueues(); };
  opq::updateSerialQueue()->enqueue(lamb);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::FlushQueue() {
  // printf("ObjModel::FlushQueue\n");
  Attach(CurrentObject());
  SigModelInvalidated();
  // SigNewObject(pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigModelInvalidated() {
  // printf("ObjModel::SigModelInvalidated\n");
  mSignalModelInvalidated(&ObjModel::SigModelInvalidated);
}
void ObjModel::SigPreNewObject() {
  auto lamb = [=]() { this->mSignalPreNewObject(&ObjModel::SigPreNewObject); };
  opq::updateSerialQueue()->enqueue(lamb);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigPropertyInvalidated(ork::Object* pobj, const reflect::IObjectProperty* prop) {
  auto lamb = [=]() { this->mSignalPropertyInvalidated(&ObjModel::SigPropertyInvalidated, pobj, prop); };
  opq::updateSerialQueue()->enqueue(lamb);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigRepaint() {
  mSignalRepaint(&ObjModel::SigRepaint);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigNewObject(ork::Object* pobj) {
  SigPreNewObject();
  mSignalNewObject(&ObjModel::SigNewObject, pobj);
  SigRepaint();
  SigPostNewObject(pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigPostNewObject(ork::Object* pobj) {
  mSignalPostNewObject(&ObjModel::SigPostNewObject, pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigSpawnNewGed(ork::Object* pOBJ) {
  mSignalSpawnNewGed(&ObjModel::SigSpawnNewGed, pOBJ);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotNewObject(ork::Object* pobj) {
  Attach(mCurrentObject);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotObjectDeleted(ork::Object* pobj) {
  Attach(0);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::ProcessQueue() {
  if (mQueueObject)
    Attach(mQueueObject);
  mQueueObject = 0;
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotObjectSelected(ork::Object* pobj) {
  // printf("ObjModel<%p> Object<%p> selected\n", this, pobj);
  Attach(pobj);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotObjectDeSelected(ork::Object* pobj) {
  Attach(0);
}

//////////////////////////////////////////////////////////////////////////////

GedItemNode* ObjModel::Recurse(ork::Object* root_object, const char* pname, bool binline) {

  GedItemNode* rval    = 0;
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
        const reflect::IObjectProperty* prop = item.second;
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
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

bool ObjModel::IsNodeVisible(const reflect::IObjectProperty* prop) {
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
    GedItemNode* parentnode                                = GetGedWidget()->ParentItemNode();
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
//////////////////////////////////////////////////////////////////////////////

void ObjModel::EnumerateNodes(sortnode& in_node, object::ObjectClass* the_class) {
  object::ObjectClass* walk_class = the_class;
  orkvector<object::ObjectClass*> ClassVect;
  while (walk_class != ork::Object::GetClassStatic()) {
    ClassVect.push_back(walk_class);
    walk_class = rtti::downcast<ork::object::ObjectClass*>(walk_class->Parent());
  }
  int inumclasses = int(ClassVect.size());
  for (int ic = (inumclasses - 1); ic >= 0; ic--) {
    object::ObjectClass* pclass                         = ClassVect[ic];
    ork::reflect::Description::PropertyMapType& propmap = pclass->Description().Properties();
    auto eg_anno                                        = pclass->Description().classAnnotation("editor.prop.groups");
    auto as_conststr                                    = eg_anno.TryAs<const char*>();
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
            ork::reflect::IObjectProperty* prop                      = (itf != propmap.end()) ? itf->second : 0;
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
        if (auto as_str = obj_props_anno.TryAs<std::string>()) {
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
          ork::reflect::IObjectProperty* prop = it.second;
          if (prop) {
            in_node.PropVect.push_back(std::make_pair(Name.c_str(), prop));
          }
        }
      }
  }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

GedItemNode* ObjModel::CreateNode(const std::string& Name, const reflect::IObjectProperty* prop, Object* pobject) {
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
  const reflect::IObjectArrayProperty* ArrayProp = rtti::autocast(prop);
  /////////////////////////////////////////////////////////////////////////
  ConstString anno_ucdclass  = prop->GetAnnotation("ged.userchoice.delegate");
  bool HasUserChoiceDelegate = (anno_ucdclass.length());
  /////////////////////////////////////////////////////////////////////////
  if (const reflect::IObjectPropertyType<Char8>* c8prop = rtti::autocast(prop))
    return new GedLabelNode(*this, Name.c_str(), prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectPropertyType<PoolString>* psprop = rtti::autocast(prop))
    return new GedSimpleNode<GedIoDriver<PoolString>, PoolString>(*this, Name.c_str(), psprop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectPropertyType<bool>* boolprop = rtti::autocast(prop))
    return new GedBoolNode<PropSetterObj>(*this, Name.c_str(), boolprop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectPropertyType<float>* floatprop = rtti::autocast(prop))
    return new GedFloatNode<GedIoDriver<float>>(*this, Name.c_str(), floatprop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectPropertyType<fvec4>* vec4prop = rtti::autocast(prop))
    return new GedSimpleNode<GedIoDriver<fvec4>, fvec4>(*this, Name.c_str(), vec4prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectPropertyType<fvec3>* vec3prop = rtti::autocast(prop))
    return new GedSimpleNode<GedIoDriver<fvec3>, fvec3>(*this, Name.c_str(), vec3prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectPropertyType<fvec2>* vec2prop = rtti::autocast(prop))
    return new GedSimpleNode<GedIoDriver<fvec2>, fvec2>(*this, Name.c_str(), vec2prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectPropertyType<fmtx4>* mtx44prop = rtti::autocast(prop))
    return new GedSimpleNode<GedIoDriver<fmtx4>, fmtx4>(*this, Name.c_str(), mtx44prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectPropertyType<TransformNode>* xfprop = rtti::autocast(prop)) {
    return new GedSimpleNode<GedIoDriver<TransformNode>, TransformNode>(*this, Name.c_str(), xfprop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectPropertyType<ork::rtti::ICastable*>* castprop = rtti::autocast(prop)) {
    return new GedObjNode<PropSetterObj>(*this, Name.c_str(), prop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectPropertyType<int>* intprop = rtti::autocast(prop)) {
    return HasUserChoiceDelegate ? (GedItemNode*)new GedSimpleNode<GedIoDriver<int>, int>(*this, Name.c_str(), intprop, pobject)
                                 : (GedItemNode*)new GedIntNode<GedIoDriver<int>>(*this, Name.c_str(), intprop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectPropertyObject* objprop = rtti::autocast(prop)) {
    ork::Object* psubobj = objprop->Access(pobject);
    if (psubobj)
      Recurse(psubobj, Name.c_str());
    else
      return new GedObjNode<PropSetterObj>(*this, Name.c_str(), prop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::DirectObjectMapPropertyType<ent::SceneData::SystemDataLut>* MapProp = rtti::autocast(prop)) {
    auto mapprop = rtti::downcast<const reflect::DirectObjectMapPropertyType<ent::SceneData::SystemDataLut>*>(prop);
    if (mapprop)
      return new GedMapNode(*this, Name.c_str(), mapprop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::DirectObjectPropertyType<ork::Object*>* dobjprop = rtti::autocast(prop)) {
    ork::Object* psubobj = 0;
    dobjprop->Get(psubobj, pobject);
    if (psubobj)
      Recurse(psubobj);
    return new GedObjNode<PropSetterObj>(*this, Name.c_str(), prop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else if (const reflect::IObjectMapProperty* MapProp = rtti::autocast(prop)) {
    auto mapprop = rtti::downcast<const reflect::IObjectMapProperty*>(prop);
    if (mapprop)
      return new GedMapNode(*this, Name.c_str(), mapprop, pobject);
  }
  /////////////////////////////////////////////////////////////////////////
  else
    return new GedLabelNode(*this, Name.c_str(), prop, pobject);
  /////////////////////////////////////////////////////////////////////////
  return nullptr;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void ObjModel::Attach(ork::Object* root_object, bool bclearstack, GedItemNode* top_root_item) {
  this->mRootObject = root_object;
  bool bnewobj      = (mCurrentObject != mRootObject);

  auto ged_widget = GetGedWidget();

  if (bclearstack) {
    while (false == mBrowseStack.empty())
      mBrowseStack.pop();
  }
  if (mRootObject) {
    mCurrentObject = mRootObject;
    if (bclearstack)
      PushBrowseStack(mCurrentObject);
  }

  if (ged_widget) {
    if (top_root_item) { // partial tree (starting at top_root_item) ?
      ged_widget->PushItemNode(top_root_item);
      Recurse(root_object);
      ged_widget->PopItemNode(top_root_item);
    } else { // full tree
      ged_widget->GetRootItem()->DestroyChildren();
      if (root_object) {
        ged_widget->PushItemNode(ged_widget->GetRootItem());
        Recurse(root_object);
        ged_widget->PopItemNode(ged_widget->GetRootItem());
      }
      Detach();
    }
    if (ged_widget->GetViewport()) {
      if (bnewobj)
        ged_widget->GetViewport()->ResetScroll();
    }
    ged_widget->DoResize();
  }
  // dont spam refresh, please
  if (mbEnablePaint)
    SigRepaint();
}

//////////////////////////////////////////////////////////////////////////////

void ObjModel::Detach() {
  mRootObject = 0;
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::PushBrowseStack(ork::Object* pobj) {
  mBrowseStack.push(pobj);
}

void ObjModel::PopBrowseStack() {
  mBrowseStack.pop();
}

ork::Object* ObjModel::BrowseStackTop() const {
  ork::Object* rval = 0;
  if (mBrowseStack.size())
    rval = mBrowseStack.top();
  return rval;
}

int ObjModel::StackSize() const {
  return mBrowseStack.size();
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::Dump(const char* header) const {
  orkprintf("OBJMODELDUMP<%s>\n", header);
  GedWidget* qw = this->GetGedWidget();
  std::queue<GedItemNode*> ItemQueue;
  if (qw) {
    ItemQueue.push(qw->GetRootItem());
  }
  while (!ItemQueue.empty()) {
    GedItemNode* node = ItemQueue.front();
    ItemQueue.pop();
    int inumc = node->GetNumItems();
    for (int ic = 0; ic < inumc; ic++) {
      GedItemNode* pchild = node->GetItem(ic);
      ItemQueue.push(pchild);
    }
    orkprintf("NODE<%08x> Name<%s>\n", node, node->_propname.c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::tool::ged
