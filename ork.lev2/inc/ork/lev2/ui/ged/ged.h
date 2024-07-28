////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/choiceman.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/orkpool.h>
#include <ork/kernel/any.h>
#include <ork/util/choiceman.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/ui/viewport.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/kernel/sigslot2.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2::ged {

static constexpr int koff = 1;
util::choicemanager_ptr_t choicemanager();

struct ObjModel;
struct PersistHashContext;
struct PersistantMap;
struct PersistMapContainer;
//
struct GedContainer;
struct GedItemNode;
struct GedSkin;
struct GedObject;
struct GedGroupNode;
struct GedSurface;

using objectmodel_ptr_t = std::shared_ptr<ObjModel>;
using persisthashcontext_ptr_t = std::shared_ptr<PersistHashContext>;
using persistantmap_ptr_t = std::shared_ptr<PersistantMap>;
using persistmapcontainer_ptr_t = std::shared_ptr<PersistMapContainer>;

using gedcontainer_ptr_t = std::shared_ptr<GedContainer>;
using gedobject_ptr_t = std::shared_ptr<GedObject>;
using geditemnode_ptr_t = std::shared_ptr<GedItemNode>;
using gedskin_ptr_t = std::shared_ptr<GedSkin>;
using gedgroupnode_ptr_t = std::shared_ptr<GedGroupNode>;
using gedsurface_ptr_t = std::shared_ptr<GedSurface>;

///////////////////////////////////////////////////////////////////////////////

struct PersistHashContext {

  PersistHashContext();
  uint32_t hash() const;

  object_ptr_t _object;
  const reflect::ObjectProperty* _property = nullptr;
  std::string _string;

};

///////////////////////////////////////////////////////////////////////////////

struct PersistantMap : public ork::Object {
  DeclareConcreteX(PersistantMap, ork::Object);
public:

  PersistantMap();
  ~PersistantMap();

  const std::string& value(const std::string& key);
  void setValue(const std::string& key, const std::string& val);

  template <typename T> attempt_cast_const<T> typedValue(const std::string& key) const {
    auto it = _typedProperties.find(key);
    if (it != _typedProperties.end()) {
      return it->second.tryAs<T>();
    }
    return attempt_cast_const<T>(nullptr);
  }
  template <typename T> T typedValueWithDefault(const std::string& key, const T& the_default) {
    auto it = _typedProperties.find(key);
    if (it != _typedProperties.end()) {
      return it->second.get<T>();
    }else{
      _typedProperties[key].set<T>(the_default);
    }
    return the_default;
  }
  template <typename T> void setTypedValue(const std::string& key, const T& val) {
    _typedProperties[key].set<T>(val);
  }

  orklut<std::string, std::string> _properties;
  std::map<std::string, svar64_t> _typedProperties;
};

struct NewIoDriver;
using newiodriver_ptr_t = std::shared_ptr<NewIoDriver>;

struct NewIoDriver {

  void_lambda_t _onValueChanged = []{};
  svar64_t _impl;  
  const reflect::ObjectProperty* _par_prop = nullptr;
  object_ptr_t _object = nullptr;
  varmap::var_t _abstract_val;
  newiodriver_ptr_t _parent;
};


///////////////////////////////////////////////////////////////////////////////

struct PersistMapContainer : public ork::Object {
  DeclareConcreteX(PersistMapContainer, ork::Object);
public:

  PersistMapContainer();
  ~PersistMapContainer();

  void cloneFrom(const PersistMapContainer& oth);

  std::unordered_map<uint32_t, persistantmap_ptr_t> _prop_persist_map;

};

///////////////////////////////////////////////////////////////////////////////

struct ObjModel : public sigslot2::AutoConnector {

public:

  static orkset<objectmodel_ptr_t> gAllObjectModels;
  static void queueFlushAll();
  static objectmodel_ptr_t createShared(opq::opq_ptr_t updateopq=nullptr);

  ObjModel(opq::opq_ptr_t updateopq=nullptr);
  ~ObjModel() override; 

  void attach(object_ptr_t obj, bool bclearstack = true, geditemnode_ptr_t rootw = 0);
  geditemnode_ptr_t recurse( newiodriver_ptr_t iodriver, //
                             const char* pname = 0,
                             bool binline = false); //
  void detach();

  void dump(const char* header) const;

  void enqueueObject(object_ptr_t obj);

  void processQueue();

  persistantmap_ptr_t persistMapForHash(const PersistHashContext& ctx);

  void enqueueUpdateAll();
  void enqueueUpdate();
  void queueFlush();

  void browseStackPush(object_ptr_t pobj);
  void browseStackPop();
  object_ptr_t browseStackTop() const;
  int browseStackSize() const;

  //////////////////////////////////////////////////////////
  //DeclareAutoSlotX(repaint);
  /*
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
  */
  reflect::IInvokation* mModelInvalidatedInvoker;

public:
  void SigModelInvalidated();
  void SigPreNewObject();
  void SigPropertyInvalidated(object_ptr_t pobj, const reflect::ObjectProperty* prop);
  void emitRepaint();
  void SigSpawnNewGed(object_ptr_t pobj);
  void SigNewObject(object_ptr_t pobj);
  void SigPostNewObject(object_ptr_t pobj);

private:
  //////////////////////////////////////////////////////////

  void SlotNewObject(object_ptr_t pobj);
  void SlotRelayModelInvalidated();
  void SlotRelayPropertyInvalidated(object_ptr_t pobj, const reflect::ObjectProperty* prop);
  void SlotObjectDeleted(object_ptr_t pobj);
  void SlotObjectSelected(object_ptr_t pobj);
  void SlotObjectDeSelected(object_ptr_t pobj);
  void SlotRepaint();

  //////////////////////////////////////////////////////////

  struct sortnode {
    std::string Name;
    orkvector<std::pair<std::string, reflect::ObjectProperty*>> PropVect;
    orkvector<std::pair<std::string, sortnode*>> GroupVect;
  };

public:

  void EnumerateNodes(sortnode& in_node, object::ObjectClass*);

  //////////////////////////////////////////////////////////

  bool IsNodeVisible(const reflect::ObjectProperty* prop);
  geditemnode_ptr_t createObjPropNode(const std::string& Name, const reflect::ObjectProperty* prop, object_ptr_t pobject);
  geditemnode_ptr_t createAbstractNode(const std::string& Name, newiodriver_ptr_t iodriver );

  bool _enablePaint = true;
  GedContainer* _gedContainer = nullptr;
  object_ptr_t _currentObject;
  object_ptr_t _rootObject;
  object_ptr_t _enqueuedObject;
  util::choicemanager_ptr_t _choicemanager;
  orkstack<object_ptr_t> _browseStack;
  persistmapcontainer_ptr_t _persistMapContainer;
  opq::opq_ptr_t _updateOPQ;


  sigslot2::signal_void_t _sigRepaint;
  sigslot2::signal_void_t _sigModelInvalidated;
};

///////////////////////////////////////////////////////////////////////////////

using factory_class_set_t = std::set<object::ObjectClass*>;

void enumerateFactories(
    const ork::Object* pdestobj,
    const reflect::ObjectProperty* prop,
    factory_class_set_t& fset);

void enumerateFactories( object::ObjectClass* baseclazz, 
                         factory_class_set_t& fset );

}