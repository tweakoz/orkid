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

struct Topology;
struct DgSorter;
struct dgcontext;

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

using dgsorter_ptr_t = std::shared_ptr<DgSorter>;
using dgcontext_ptr_t = std::shared_ptr<dgcontext>;

using moduledata_ptr_t = std::shared_ptr<ModuleData>;
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

using moduledata_constptr_t = std::shared_ptr<const ModuleData>;
using dgmoduledata_constptr_t = std::shared_ptr<const DgModuleData>;
using inplugdata_constptr_t = std::shared_ptr<const InPlugData>;
using outplugdata_constptr_t = std::shared_ptr<const OutPlugData>;

using scheduler_ptr_t = std::shared_ptr<scheduler>;

using topology_ptr_t = std::shared_ptr<Topology>;

template <typename vartype> class plug;
template <typename vartype> class inplug;
template <typename vartype> class outplug;
typedef int Affinity;

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
  std::set<dgmoduledata_ptr_t> _downstream_dependents;
  dgmoduledata_ptr_t mpOwner;
  dgregisterblock* mpBlock;
  //////////////////////////////////
  void bindModule(dgmoduledata_ptr_t pmod);
  //////////////////////////////////
  dgregister(dgmoduledata_ptr_t pmod = 0, int idx = -1);
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
  const std::string& name() const {
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

  void assignSchedulerToGraphInst(graphinst_ptr_t gi, scheduler_ptr_t sched);


  void SetRegisters(const std::type_info* pinfo, dgregisterblock*);
  dgregisterblock* GetRegisters(const std::type_info* pinfo);
  void Clear();
  template <typename T> dgregisterblock* GetRegisters() {
    return GetRegisters(&typeid(T));
  }
  template <typename T> void SetRegisters(dgregisterblock* pregs) {
    SetRegisters(&typeid(T), pregs);
  }
  void prune(dgmoduledata_ptr_t mod);
  dgregister* alloc(outplugdata_ptr_t poutplug);

  orkmap<const std::type_info*, dgregisterblock*> mRegisterSets;
};

///////////////////////////////////////////////////////////////////////////////
// DgSorter : state tracking to help topo-sort dgmodules into execution order
///////////////////////////////////////////////////////////////////////////////

struct DgSorter {

  static constexpr size_t NOSERIAL = 0xffffffffffffffff;

  struct NodeInfo {
    
    size_t _serial;
    int mDepth;
    int mModifier;

    NodeInfo()
        : _serial(NOSERIAL)
        , mDepth(-1)
        , mModifier(-1) {
    }
  };

  struct PlugInfo {
    PlugInfo(){}

    dgregister* _register = nullptr;
  };

  //////////////////////////////////////////////////////////
  DgSorter(const GraphData* pg, dgcontext_ptr_t ctx);
  //////////////////////////////////////////////////////////
  bool isPending(dgmoduledata_ptr_t mod) const;
  size_t numPending() const;
  int numDownstream(dgmoduledata_ptr_t mod) const;
  int numPendingDownstream(dgmoduledata_ptr_t mod) const;
  void addModule(dgmoduledata_ptr_t mod);
  void pruneRegisters(dgmoduledata_ptr_t pmod);
  void enqueueModule(dgmoduledata_ptr_t pmod, int irecd);
  bool hasPendingInputs(dgmoduledata_ptr_t mod) const;
  void dumpInputs(dgmoduledata_ptr_t mod) const;
  void dumpOutputs(dgmoduledata_ptr_t mod) const;
  topology_ptr_t generateTopology(dgcontext_ptr_t ctx);
  //////////////////////////////////////////////////////////
  dgcontext_ptr_t _dgcontext;
  const GraphData* _graphdata;
  size_t _serial;
  std::set<dgmoduledata_ptr_t> _pending;
  std::stack<dgmoduledata_ptr_t> _modulestack;

  std::map<dgmoduledata_ptr_t,NodeInfo> _nodeinfomap;
  std::map<plugdata_ptr_t,PlugInfo> _pluginfomap;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct Topology {
  std::vector<dgmoduledata_ptr_t> _flattened;
  uint64_t _hash = 0;
};

struct GraphData : public ork::Object {

  DeclareConcreteX(GraphData, ork::Object);

public:

  static void addModule(graphdata_ptr_t gd, const std::string& named, dgmoduledata_ptr_t pchild);
  static void removeModule(graphdata_ptr_t gd, dgmoduledata_ptr_t pchild);


  GraphData();
  ~GraphData();

  virtual bool canConnect(inplugdata_constptr_t pin, outplugdata_constptr_t pout) const;
  bool isComplete() const;
  bool isTopologyDirty() const;
  dgmoduledata_ptr_t module(const std::string& named) const;
  dgmoduledata_ptr_t module(size_t indexed) const;
  size_t numModules() const;

  void markTopologyDirty(bool bv);

  virtual void clear();
  void OnGraphChanged();

  void safeConnect(inplugdata_ptr_t inp, outplugdata_ptr_t outp);
  void disconnect(inplugdata_ptr_t inp);
  void disconnect(outplugdata_ptr_t inp);

  //void connectInternal(outplugdata_ptr_t vt);
  //void connectExternal(outplugdata_ptr_t vt);


  bool SerializeConnections(ork::reflect::serdes::ISerializer& ser) const;
  bool DeserializeConnections(ork::reflect::serdes::IDeserializer& deser);
  bool preDeserialize(reflect::serdes::IDeserializer&) override;
  bool postDeserialize(reflect::serdes::IDeserializer&) override;


  orklut<std::string, object_ptr_t> _modules;
  bool _topologyDirty;
};

///////////////////////////////////////////////////////////////////////////////

struct GraphInst {

  const std::set<int>& OutputRegisters() const {
    return mOutputRegisters;
  }
  ////////////////////////////////////////////
  GraphInst();
  ~GraphInst();
  ////////////////////////////////////////////
  size_t numModules() const;
  dgmoduleinst_ptr_t module(size_t indexed) const;
  ////////////////////////////////////////////
  void Clear();
  ////////////////////////////////////////////
  bool isPending() const;
  bool isDirty(void) const;
  ////////////////////////////////////////////
  void setPending(bool bv);
  ////////////////////////////////////////////

  graphdata_ptr_t _graphdata;
  topology_ptr_t _topology;
  scheduler_ptr_t _scheduler;

  std::vector<dgmoduleinst_ptr_t> _module_insts;

  bool mbInProgress;
  std::priority_queue<dgmoduledata_ptr_t> mModuleQueue;
  std::set<int> mOutputRegisters;

  //void doNotify(const ork::event::Event* event); // virtual
};

///////////

#if 0 // TODO
template <typename class_t, typename data_t, typename deleg_t> //
inline void implementFloatXfPlugReflectionX(object::ObjectClass* clazz, //
                                            typename class_t::&data_t member, //
                                            std::string name, //
                                            typename data_t mmin, //
                                            typename data_t mmax, //
                                            typename deleg_t deleg) { //
  clazz->directProperty(name, &member) //
       ->annotate<ConstString>("editor.class", "ged.factory.plug") //
       ->annotate<deleg_t>("ged.plug.delegate", deleg) //
       ->annotate<data_t>("editor.range.min", mmin) //
       ->annotate<data_t>("editor.range.max", mmax);
}

#endif
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
