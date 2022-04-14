////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#pragma once

#include <ork/pch.h>
#include <ork/object/AutoConnector.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/orkpool.h>
#include <ork/kernel/any.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/util/choiceman.h>

namespace ork {
struct Object;
}

namespace ork::reflect::editor {
#if 0
struct ObjectModel;
struct ObjectModelNode;
struct ObjectModelObserver;

using objectmodel_ptr_t         = std::shared_ptr<ObjectModel>;
using objectmodelnode_ptr_t     = std::shared_ptr<ObjectModelNode>;
using objectmodelobserver_ptr_t = std::shared_ptr<ObjectModelObserver>;

///////////////////////////////////////////////////////////////////////////////

struct PersistHashContext {
  ork::object_ptr_t mObject;
  const ObjectProperty* mProperty;
  const char* mString;

  PersistHashContext();
  int GenerateHash() const;
};

class PersistantMap : public ork::Object {
  RttiDeclareConcrete(PersistantMap, ork::Object);

  orklut<std::string, std::string> mProperties;

public:
  const std::string& GetValue(const std::string& key);
  void SetValue(const std::string& key, const std::string& val);

  PersistantMap();
  ~PersistantMap();
};

///////////////////////////////////////////////////////////////////////////////

class PersistMapContainer : public ork::Object {
  RttiDeclareConcrete(PersistMapContainer, ork::Object);

  orklut<int, PersistantMap*> mPropPersistMap;

public:
  PersistMapContainer();
  ~PersistMapContainer();

  orklut<int, PersistantMap*>& GetMap() {
    return mPropPersistMap;
  }
  const orklut<int, PersistantMap*>& GetMap() const {
    return mPropPersistMap;
  }

  void CloneFrom(const PersistMapContainer& oth);
};

///////////////////////////////////////////////////////////////////////////////

struct ObjectModel final : public ork::AutoConnector {
  RttiDeclareConcrete(ObjectModel, ork::AutoConnector);

public:
  static orkset<objectmodel_ptr_t> gAllObjectModels;
  static objectmodel_ptr_t create();
  static void release(objectmodel_ptr_t model);
  static void FlushAllQueues();

  void setObserver(objectmodelobserver_ptr_t observer) {
    _observer = observer;
  }
  void Attach(
      ork::object_ptr_t obj, //
      bool bclearstack            = true,
      objectmodelnode_ptr_t rootw = 0);

  objectmodelnode_ptr_t Recurse(
      ork::object_ptr_t obj, //
      const char* pname = 0,
      bool binline      = false);

  void Detach();
  objectmodelobserver_ptr_t observer() const {
    return _observer;
  }
  ObjectModel();
  ~ObjectModel() override;

  void SetChoiceManager(util::choicemanager_ptr_t chcman) {
    mChoiceManager = chcman;
  }
  util::choicemanager_ptr_t GetChoiceManager(void) const {
    return mChoiceManager;
  }

  void Dump(const char* header) const;

  void QueueObject(ork::object_ptr_t obj) {
    mQueueObject = obj;
  }

  void ProcessQueue();

  ork::object_ptr_t CurrentObject() const {
    return mCurrentObject;
  }

  PersistantMap* GetPersistMap(const PersistHashContext& ctx);

  void QueueUpdateAll();
  void QueueUpdate();

  void FlushQueue();

  void PushBrowseStack(ork::object_ptr_t pobj);
  void PopBrowseStack();
  ork::object_ptr_t BrowseStackTop() const;
  int StackSize() const;

  PersistMapContainer& GetPersistMapContainer() {
    return mPersistMapContainer;
  }
  const PersistMapContainer& GetPersistMapContainer() const {
    return mPersistMapContainer;
  }

  void EnablePaint() {
    mbEnablePaint = true;
  }

  objectmodelobserver_ptr_t _observer = nullptr;
  ork::object_ptr_t mCurrentObject    = nullptr;
  ork::object_ptr_t mRootObject       = nullptr;
  ork::object_ptr_t mQueueObject      = nullptr;
  util::choicemanager_ptr_t mChoiceManager;
  orkstack<ork::object_ptr_t> mBrowseStack;
  PersistMapContainer mPersistMapContainer;
  bool mbEnablePaint;

  //////////////////////////////////////////////////////////

  DeclarePublicSignal(Repaint);
  DeclarePublicSignal(PreNewObject);
  DeclarePublicSignal(ModelInvalidated);
  DeclarePublicSignal(PropertyInvalidated);
  DeclarePublicSignal(NewObject);
  DeclarePublicSignal(SpawnNewGed);

  DeclarePublicSignal(PostNewObject);

  DeclarePublicAutoSlot(NewObject);
  DeclarePublicAutoSlot(RelayModelInvalidated);
  DeclarePublicAutoSlot(RelayPropertyInvalidated);
  DeclarePublicAutoSlot(ObjectDeleted);
  DeclarePublicAutoSlot(ObjectSelected);
  DeclarePublicAutoSlot(ObjectDeSelected);
  DeclarePublicAutoSlot(Repaint);

public:
  void SigModelInvalidated();
  void SigPreNewObject();
  void SigPropertyInvalidated(ork::object_ptr_t pobj, const ObjectProperty* prop);
  void SigRepaint();
  void SigSpawnNewGed(ork::object_ptr_t pobj);
  void SigNewObject(ork::object_ptr_t pobj);
  void SigPostNewObject(ork::object_ptr_t pobj);

private:
  //////////////////////////////////////////////////////////

  void SlotNewObject(ork::object_ptr_t pobj);
  void SlotRelayModelInvalidated();
  void SlotRelayPropertyInvalidated(ork::object_ptr_t pobj, const ObjectProperty* prop);
  void SlotObjectDeleted(ork::object_ptr_t pobj);
  void SlotObjectSelected(ork::object_ptr_t pobj);
  void SlotObjectDeSelected(ork::object_ptr_t pobj);
  void SlotRepaint();

  //////////////////////////////////////////////////////////

  struct sortnode {
    std::string Name;
    orkvector<std::pair<std::string, ObjectProperty*>> PropVect;
    orkvector<std::pair<std::string, sortnode*>> GroupVect;
  };

  void EnumerateNodes(
      sortnode& in_node, //
      object::ObjectClass*);

  //////////////////////////////////////////////////////////
  IInvokation* mModelInvalidatedInvoker = nullptr;

  // bool IsNodeVisible(const ObjectProperty* prop);
  // objectmodelnode_ptr_t CreateNode(const std::string& Name, const ObjectProperty* prop, object_ptr_t pobject);
};

///////////////////////////////////////////////////////////////////////////////
struct ObjectModelNode : public ork::Object { //
  RttiDeclareAbstract(ObjectModelNode, ork::Object);

public:
  ObjectModelNode(
      objectmodel_ptr_t mdl, //
      const char* name,
      const reflect::ObjectProperty* prop,
      ork::object_ptr_t obj);

  ~ObjectModelNode() override;
  ///////////////////////////////////
  void SigInvalidateProperty();
  objectmodelnode_ptr_t GetItem(int idx) const;
  int GetNumItems() const;
  ObjectModelNode* parent() const;
  void releaseChildren();
  void SetOrkProp(const reflect::ObjectProperty* prop);
  const reflect::ObjectProperty* GetOrkProp() const;
  void SetOrkObj(ork::object_ptr_t obj);
  ork::object_ptr_t GetOrkObj() const;
  objectmodelobserver_ptr_t root() const;
  void AddItem(objectmodelnode_ptr_t w);
  ///////////////////////////////////
  virtual void Invalidate();
  ///////////////////////////////////
  void SetDepth(int id);
  int GetDepth() const;
  int GetDecoIndex() const;
  void SetDecoIndex(int idx);
  ///////////////////////////////////
  const reflect::ObjectProperty* mOrkProp;
  ork::object_ptr_t mOrkObj;
  ObjectModelNode* _parent;
  objectmodelobserver_ptr_t mRoot;
  orkvector<objectmodelnode_ptr_t> mItems;
  orkmap<std::string, std::string> mTags;
  std::string _propname;
  std::string _content;
  objectmodel_ptr_t _objectmodel;
  bool mbInvalid;
  int _depth       = 0;
  int _linearindex = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct ObjectModelObserver : public ork::AutoConnector { //
  RttiDeclareAbstract(ObjectModelObserver, ork::AutoConnector);

public: //
  ObjectModelObserver();
  void PropertyInvalidated(ork::object_ptr_t pobj, const reflect::ObjectProperty* prop);
  void Attach(ork::object_ptr_t obj);
  ///////////////////////////////////
  void PushItemNode(objectmodelnode_ptr_t qw);
  void PopItemNode(objectmodelnode_ptr_t qw);
  void AddChild(objectmodelnode_ptr_t pw);
  int GetStackDepth() const {
    return int(mItemStack.size());
  }
  U64 GetStackHash() const;
  void ComputeStackHash();
  objectmodelnode_ptr_t topItemNode() const;
  DeclarePublicAutoSlot(ModelInvalidated);
  void SlotModelInvalidated();
  ///////////////////////////////////
  virtual void onModelInvalidated() {
  }
  virtual void onObjectAttachedToModel(ork::object_ptr_t root_obj) {
  }
  virtual objectmodelnode_ptr_t createGroup(
      ObjectModel* mdl,
      const char* name,
      const reflect::ObjectProperty* prop,
      ork::object_ptr_t obj,
      bool is_obj_node = false) {
    return nullptr;
  }
  ///////////////////////////////////
  objectmodelnode_ptr_t _rootnode;
  ork::object_ptr_t mRootObject;
  objectmodel_ptr_t _objectmodel;
  std::deque<objectmodelnode_ptr_t> mItemStack;
  U64 mStackHash;
};
#endif
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::editor
