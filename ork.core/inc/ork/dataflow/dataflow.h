////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/prop.h>
#include <ork/rtti/downcast.h>
#include <ork/kernel/mutex.h>
#include <ork/util/crc64.h>

#include <ork/config/config.h>

#include <ork/math/multicurve.h>
#include <ork/kernel/orkpool.h>
#include <ork/event/Event.h>
#include <ork/rtti/RTTIX.inl>

namespace ork { namespace dataflow {

///////////////////////////////////////////////////////////////////////////////

class workunit;
class scheduler;
class cluster;
struct dgregister;
struct dyn_external;

struct GraphData;
struct GraphInst;

struct PlugData;
struct PlugInst;
struct InPlugData;
struct OutPlugData;
struct InPlugInst;
struct OutPlugInst;

struct ModuleData;
struct ModuleInst;
struct DgModuleData;
struct DgModuleInst;

struct MorphableData;

using moduledata_ptr_t = std::shared_ptr<ModuleData>;
using moduledata_constptr_t = std::shared_ptr<const ModuleData>;
using moduleinst_ptr_t = std::shared_ptr<ModuleInst>;
using dgmoduledata_ptr_t = std::shared_ptr<DgModuleData>;
using dgmoduleinst_ptr_t = std::shared_ptr<DgModuleInst>;

using graphdata_ptr_t = std::shared_ptr<GraphData>;
using graphinst_ptr_t = std::shared_ptr<GraphInst>;

using plugdata_ptr_t = std::shared_ptr<PlugData>;
using pluginst_ptr_t = std::shared_ptr<PlugInst>;
using inplugdata_ptr_t = std::shared_ptr<InPlugData>;
using outplugdata_ptr_t = std::shared_ptr<OutPlugData>;
using inpluginst_ptr_t = std::shared_ptr<InPlugInst>;
using outpluginst_ptr_t = std::shared_ptr<OutPlugInst>;

using morphable_ptr_t = std::shared_ptr<MorphableData>;

template <typename vartype> class plug;
template <typename vartype> class inplug;
template <typename vartype> class outplug;
typedef int Affinity;

///////////////////////////////////////////////////////////////////////////////

struct nodekey {
  int mSerial;
  int mDepth;
  int mModifier;

  nodekey()
      : mSerial(-1)
      , mDepth(-1)
      , mModifier(-1) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class node_hash {
public:
  node_hash() {
    boost::crc64_init(mValue);
  }

  template <typename T> void Hash(const T& val) {
    crc64_compute(mValue, (const void*)&val, sizeof(val));
    boost::crc64_fin(mValue);
  }

  bool operator==(const node_hash& oth) const {
    return mValue == oth.mValue;
  }

private:
  boost::Crc64 mValue;
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// dgqueue (dependency graph queue)
//  this will topologically sort and queue modules in a graph so that:
//  1. all modules are computed
//	2. no module is computed before its inputs
//  3. modules are computed soon after their parents
//  4. minimal temp registers are used
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// a dgregister is an abstraction of the concept machine register
//  where the dgcontext is a thread and the dggraph is the program
// It knows the dependent clients downstream of it self (for managing the
//  lifetime of given data attached to the register

class dgregisterblock;

struct dgregister {
  int mIndex;
  std::set<dgmoduleinst_ptr_t> _downstream_dependents;
  dgmoduleinst_ptr_t mpOwner;
  dgregisterblock* mpBlock;
  //////////////////////////////////
  void bindModule(dgmoduleinst_ptr_t pmod);
  //////////////////////////////////
  dgregister(dgmoduleinst_ptr_t pmod = 0, int idx = -1);
  //////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

// a dgregisterblock is a pool of registers for a given machine

class dgregisterblock {
public:
  dgregisterblock(const std::string& name, int isize);

  dgregister* Alloc();
  void Free(dgregister* preg);
  const orkset<dgregister*>& Allocated() const {
    return mAllocated;
  }
  void Clear();
  const std::string& GetName() const {
    return mName;
  }

private:
  ork::pool<dgregister> mBlock;
  orkset<dgregister*> mAllocated;
  std::string mName;
};

///////////////////////////////////////////////////////////////////////////////

struct dgcontext {
public:
  void SetRegisters(const std::type_info* pinfo, dgregisterblock*);
  dgregisterblock* GetRegisters(const std::type_info* pinfo);
  void Clear();
  template <typename T> dgregisterblock* GetRegisters() {
    return GetRegisters(&typeid(T));
  }
  template <typename T> void SetRegisters(dgregisterblock* pregs) {
    SetRegisters(&typeid(T), pregs);
  }
  void prune(dgmoduleinst_ptr_t mod);
  void alloc(outpluginst_ptr_t poutplug);
  void setProbeModule(dgmoduleinst_ptr_t pmod) {
    _probemodule = pmod;
  }

  orkmap<const std::type_info*, dgregisterblock*> mRegisterSets;
  dgmoduleinst_ptr_t _probemodule;
};

///////////////////////////////////////////////////////////////////////////////

struct dgqueue {
  std::set<dgmoduleinst_ptr_t> pending;
  int mSerial;
  std::stack<dgmoduleinst_ptr_t> mModStack;
  dgcontext& mCompCtx;
  //////////////////////////////////////////////////////////
  bool IsPending(dgmoduleinst_ptr_t mod);
  size_t NumPending() {
    return pending.size();
  }
  int NumDownstream(dgmoduleinst_ptr_t mod);
  int NumPendingDownstream(dgmoduleinst_ptr_t mod);
  void AddModule(dgmoduleinst_ptr_t mod);
  void pruneRegisters(dgmoduleinst_ptr_t pmod);
  void QueModule(dgmoduleinst_ptr_t pmod, int irecd);
  bool HasPendingInputs(dgmoduleinst_ptr_t mod);
  void DumpInputs(dgmoduleinst_ptr_t mod) const;
  void DumpOutputs(dgmoduleinst_ptr_t mod) const;
  //////////////////////////////////////////////////////////
  dgqueue(const GraphInst* pg, dgcontext& ctx);
  //////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct GraphData : public ork::Object {

  DeclareConcreteX(GraphData, ork::Object);

public:
  GraphData();
  ~GraphData();
  // GraphData(const GraphData& oth);

  virtual bool CanConnect(const inplugdata_ptr_t pin, const outplugdata_ptr_t pout) const;
  bool IsComplete() const;
  bool IsTopologyDirty() const {
    return mbTopologyIsDirty;
  }
  dgmoduledata_ptr_t GetChild(const std::string& named) const;
  dgmoduledata_ptr_t GetChild(size_t indexed) const;
  const orklut<std::string, ork::Object*>& Modules() {
    return mModules;
  }
  size_t GetNumChildren() const {
    return mModules.size();
  }

  void AddChild(const std::string& named, dgmoduledata_ptr_t pchild);
  void AddChild(const char* named, dgmoduledata_ptr_t pchild);
  void RemoveChild(dgmoduledata_ptr_t pchild);
  void SetTopologyDirty(bool bv) {
    mbTopologyIsDirty = bv;
  }
  recursive_mutex& GetMutex() {
    return mMutex;
  }

  const orklut<int, dgmoduledata_ptr_t>& LockTopoSortedChildrenForRead(int lid) const;
  orklut<int, dgmoduledata_ptr_t>& LockTopoSortedChildrenForWrite(int lid);
  void UnLockTopoSortedChildren() const;
  virtual void Clear() {
  }
  void OnGraphChanged();

protected:
  LockedResource<orklut<int, dgmoduledata_ptr_t>> mChildrenTopoSorted;
  orklut<std::string, ork::Object*> mModules;
  bool mbTopologyIsDirty;
  recursive_mutex mMutex;

  bool SerializeConnections(ork::reflect::serdes::ISerializer& ser) const;
  bool DeserializeConnections(ork::reflect::serdes::IDeserializer& deser);
  bool preDeserialize(reflect::serdes::IDeserializer&) override;
  bool postDeserialize(reflect::serdes::IDeserializer&) override;
};

///////////////////////////////////////////////////////////////////////////////

struct GraphInst {

  const std::set<int>& OutputRegisters() const {
    return mOutputRegisters;
  }
  ////////////////////////////////////////////
  GraphInst();
  ~GraphInst();
  GraphInst(const GraphInst& oth);
  ////////////////////////////////////////////
  void BindExternal(dyn_external* pexternal);
  void UnBindExternal();
  dyn_external* GetExternal() const;
  void Clear();
  ////////////////////////////////////////////
  bool IsPending() const;
  bool IsDirty(void) const;
  ////////////////////////////////////////////
  void SetPending(bool bv);
  ////////////////////////////////////////////
  void RefreshTopology(dgcontext& ctx);
  ////////////////////////////////////////////
  void SetScheduler(scheduler* psch);
  scheduler* GetScheduler() const {
    return mScheduler;
  }
  ////////////////////////////////////////////

  dyn_external* mExternal;
  scheduler* mScheduler;
  bool mbInProgress;
  std::priority_queue<dgmoduledata_ptr_t> mModuleQueue;
  std::set<int> mOutputRegisters;

  //void doNotify(const ork::event::Event* event); // virtual
};

///////////////////////////////////////////////////////////////////////////////

#define OutPlugName(name) mPlugOut##name
#define InpPlugName(name) mPlugInp##name
#define OutDataName(name) mOutData##name
#define ConstructOutPlug(name, epr) OutPlugName(name)(this, epr, &mOutData##name, #name)
#define ConstructOutTypPlug(name, epr, typ) OutPlugName(name)(this, epr, &mOutData##name, typ, #name)
#define ConstructInpPlug(name, epr, def) InpPlugName(name)(this, epr, def, #name)

///////////

#define DeclareFloatXfPlug(name)                                                                                                   \
  float mf##name = 0.0f;                                                                                                                  \
  mutable ork::dataflow::floatxfinplug InpPlugName(name);                                                                                  \
  ork::Object* InpAccessor##name() {                                                                                               \
    return &InpPlugName(name);                                                                                                     \
  }

#define DeclareVect3XfPlug(name)                                                                                                   \
  ork::fvec3 mv##name;                                                                                                             \
  mutable ork::dataflow::vect3xfinplug InpPlugName(name);                                                                                  \
  ork::Object* InpAccessor##name() {                                                                                               \
    return &InpPlugName(name);                                                                                                     \
  }

#define DeclareFloatOutPlug(name)                                                                                                  \
  float OutDataName(name) = 0.0f;                                                                                                         \
  mutable ork::dataflow::outplug<float> OutPlugName(name);                                                                                 \
  ork::Object* PlgAccessor##name() {                                                                                               \
    return &OutPlugName(name);                                                                                                     \
  }

#define DeclareVect3OutPlug(name)                                                                                                  \
  ork::fvec3 OutDataName(name);                                                                                                    \
  mutable ork::dataflow::outplug<ork::fvec3> OutPlugName(name);                                                                            \
  ork::Object* PlgAccessor##name() {                                                                                               \
    return &OutPlugName(name);                                                                                                     \
  }

///////////

#define RegisterFloatXfPlug(cls, name, mmin, mmax, deleg)                                                                          \
  ork::reflect::RegisterProperty(#name, &cls::InpAccessor##name);                                                                  \
  ork::reflect::annotatePropertyForEditor<cls>(#name, "editor.class", "ged.factory.plug");                                         \
  ork::reflect::annotatePropertyForEditor<cls>(#name, "ged.plug.delegate", #deleg);                                                \
  ork::reflect::annotatePropertyForEditor<cls>(#name, "editor.range.min", #mmin);                                                  \
  ork::reflect::annotatePropertyForEditor<cls>(#name, "editor.range.max", #mmax);

#define RegisterVect3XfPlug(cls, name, mmin, mmax, deleg)                                                                          \
  ork::reflect::RegisterProperty(#name, &cls::InpAccessor##name);                                                                  \
  ork::reflect::annotatePropertyForEditor<cls>(#name, "editor.class", "ged.factory.plug");                                         \
  ork::reflect::annotatePropertyForEditor<cls>(#name, "ged.plug.delegate", #deleg);                                                \
  ork::reflect::annotatePropertyForEditor<cls>(#name, "editor.range.min", #mmin);                                                  \
  ork::reflect::annotatePropertyForEditor<cls>(#name, "editor.range.max", #mmax);

#define RegisterObjInpPlug(cls, name)                                                                                              \
  ork::reflect::RegisterProperty(#name, &cls::InpAccessor##name);                                                                  \
  ork::reflect::annotatePropertyForEditor<cls>(#name, "editor.class", "ged.factory.plug");                                         \
  ork::reflect::annotatePropertyForEditor<cls>(#name, "ged.plug.delegate", "ged::OutPlugChoiceDelegate");

#define RegisterObjOutPlug(cls, name)                                                                                              \
  ork::reflect::RegisterProperty(#name, &cls::OutAccessor##name);                                                                  \
  ork::reflect::annotatePropertyForEditor<cls>(#name, "editor.visible", "false");

}} // namespace ork::dataflow


#include "enum.h"
#include "plugdata.h"
#include "pluginst.h"
#include "module.h"

///////////////////////////////////////////////////////////////////////////////
