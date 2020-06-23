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
class Object;
}

namespace ork::reflect::editor {
struct ObjectModel;
struct ObjectModelNode;
struct ObjectModelObserver;

using objectmodel_ptr_t         = std::shared_ptr<ObjectModel>;
using objectmodelnode_ptr_t     = std::shared_ptr<ObjectModelNode>;
using objectmodelobserver_ptr_t = std::shared_ptr<ObjectModelObserver>;

///////////////////////////////////////////////////////////////////////////////

struct PersistHashContext {
  ork::Object* mObject;
  const IObjectProperty* mProperty;
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
      ork::Object* obj, //
      bool bclearstack            = true,
      objectmodelnode_ptr_t rootw = 0);

  objectmodelnode_ptr_t Recurse(
      ork::Object* obj, //
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

  void QueueObject(ork::Object* obj) {
    mQueueObject = obj;
  }

  void ProcessQueue();

  ork::Object* CurrentObject() const {
    return mCurrentObject;
  }

  PersistantMap* GetPersistMap(const PersistHashContext& ctx);

  void QueueUpdateAll();
  void QueueUpdate();

  void FlushQueue();

  void PushBrowseStack(ork::Object* pobj);
  void PopBrowseStack();
  ork::Object* BrowseStackTop() const;
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
  ork::Object* mCurrentObject         = nullptr;
  ork::Object* mRootObject            = nullptr;
  ork::Object* mQueueObject           = nullptr;
  util::choicemanager_ptr_t mChoiceManager;
  orkstack<ork::Object*> mBrowseStack;
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
  void SigPropertyInvalidated(ork::Object* pobj, const IObjectProperty* prop);
  void SigRepaint();
  void SigSpawnNewGed(ork::Object* pobj);
  void SigNewObject(ork::Object* pobj);
  void SigPostNewObject(ork::Object* pobj);

private:
  //////////////////////////////////////////////////////////

  void SlotNewObject(ork::Object* pobj);
  void SlotRelayModelInvalidated();
  void SlotRelayPropertyInvalidated(ork::Object* pobj, const IObjectProperty* prop);
  void SlotObjectDeleted(ork::Object* pobj);
  void SlotObjectSelected(ork::Object* pobj);
  void SlotObjectDeSelected(ork::Object* pobj);
  void SlotRepaint();

  //////////////////////////////////////////////////////////

  struct sortnode {
    std::string Name;
    orkvector<std::pair<std::string, IObjectProperty*>> PropVect;
    orkvector<std::pair<std::string, sortnode*>> GroupVect;
  };

  void EnumerateNodes(
      sortnode& in_node, //
      object::ObjectClass*);

  //////////////////////////////////////////////////////////
  IInvokation* mModelInvalidatedInvoker = nullptr;

  // bool IsNodeVisible(const IObjectProperty* prop);
  // objectmodelnode_ptr_t CreateNode(const std::string& Name, const IObjectProperty* prop, Object* pobject);
};

///////////////////////////////////////////////////////////////////////////////
struct ObjectModelNode : public ork::Object { //
  RttiDeclareAbstract(ObjectModelNode, ork::Object);

public:
  ObjectModelNode(
      objectmodel_ptr_t mdl, //
      const char* name,
      const reflect::IObjectProperty* prop,
      ork::Object* obj);

  ~ObjectModelNode() override;
  ///////////////////////////////////
  void SigInvalidateProperty();
  objectmodelnode_ptr_t GetItem(int idx) const;
  int GetNumItems() const;
  ObjectModelNode* parent() const;
  void releaseChildren();
  void SetOrkProp(const reflect::IObjectProperty* prop);
  const reflect::IObjectProperty* GetOrkProp() const;
  void SetOrkObj(ork::Object* obj);
  ork::Object* GetOrkObj() const;
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
  const reflect::IObjectProperty* mOrkProp;
  ork::Object* mOrkObj;
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
class ObjectModelObserver : public ork::AutoConnector { //
  RttiDeclareAbstract(ObjectModelObserver, ork::AutoConnector);

public: //
  ObjectModelObserver();
  void PropertyInvalidated(ork::Object* pobj, const reflect::IObjectProperty* prop);
  void Attach(ork::Object* obj);
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
  virtual void onObjectAttachedToModel(ork::Object* root_obj) {
  }
  virtual objectmodelnode_ptr_t createGroup(
      ObjectModel* mdl,
      const char* name,
      const reflect::IObjectProperty* prop,
      ork::Object* obj,
      bool is_obj_node = false) {
    return nullptr;
  }
  ///////////////////////////////////
  objectmodelnode_ptr_t _rootnode;
  ork::Object* mRootObject;
  objectmodel_ptr_t _objectmodel;
  std::deque<objectmodelnode_ptr_t> mItemStack;
  U64 mStackHash;
};
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::editor
