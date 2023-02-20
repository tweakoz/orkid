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

struct GraphData;
struct GraphInst;
struct InPlugData;
struct OutPlugData;

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

using inplugdata_ptr_t = std::shared_ptr<InPlugData>;
using outplugdata_ptr_t = std::shared_ptr<OutPlugData>;

using morphable_ptr_t = std::shared_ptr<MorphableData>;

template <typename vartype> class plug;
template <typename vartype> class inplug;
template <typename vartype> class outplug;
typedef int Affinity;

///////////////////////////////////////////////////////////////////////////////

typedef std::string MorphKey;
typedef std::string MorphGroup;

enum EMorphEventType { EMET_WRITE = 0, EMET_MORPH, EMET_END };

class morph_event : public event::Event {
public:
  EMorphEventType meType;
  float mfMorphValue;
  MorphGroup mMorphGroup;

  morph_event()
      : meType(EMET_END)
      , mfMorphValue(0.0f) {
  }
};

struct imorphtarget {
  virtual void Apply() = 0;
};
struct float_morphtarget : public imorphtarget {
  float mfValue;
};

struct morphitem {
  MorphKey mKey;
  float mWeight;

  morphitem()
      : mWeight(0.0f) {
  }
};

struct morphable {
  void HandleMorphEvent(const morph_event* me);
  virtual void WriteMorphTarget(MorphKey name, float flerpval) = 0;
  virtual void RecallMorphTarget(MorphKey name)                = 0;
  virtual void Morph1D(const morph_event* pevent)              = 0;

  static const int kmaxweights = 2;
  morphitem mMorphItems[kmaxweights];
};

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

enum EPlugDir {
  EPD_INPUT = 0,
  EPD_OUTPUT,
  EPD_BOTH,
  EPD_NONE,
};

enum EPlugRate {
  EPR_EVENT = 0, // plug will not change during the entire duration of an event
  EPR_UNIFORM,   // plug will not change during the entire duration of a single compute call
  EPR_VARYING1,  // plug may change during the entire duration of a single compute call (once per item)
  EPR_VARYING2,  // plug may change more frequently than EPR_VARYING1 (multiple times per item)
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> int MaxFanout(void);

///////////////////////////////////////////////////////////////////////////////

class PlugData : public ork::Object {
  DeclareAbstractX(PlugData, ork::Object);

public:

  PlugData(moduledata_ptr_t pmod, EPlugDir edir, EPlugRate epr, const std::type_info& tid, const char* pname);

  const std::type_info& GetDataTypeId() const {
    return mTypeId;
  }

  EPlugDir mePlugDir;
  EPlugRate mePlugRate;
  moduledata_ptr_t _module;
  bool mbDirty;
  const std::type_info& mTypeId;
  std::string mPlugName;

  virtual void DoSetDirty(bool bv) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct InPlugData : public PlugData {

public:

  DeclareAbstractX(InPlugData, PlugData);

public:

  InPlugData(moduledata_ptr_t pmod, EPlugRate epr, const std::type_info& tid, const char* pname);
  ~InPlugData();

  bool isConnected() const {
    return (mExternalOutput != 0);
  }
  bool isMorphable() const {
    return (mpMorphable != 0);
  }

  outplugdata_ptr_t GetExternalOutput() const {
    return mExternalOutput;
  }

  void SafeConnect(graphdata_ptr_t gr, outplugdata_ptr_t vt);
  void Disconnect();
  void SetMorphable(morphable_ptr_t pmorph) {
    mpMorphable = pmorph;
  }
  morphable_ptr_t GetMorphable() const {
    return mpMorphable;
  }

  outplugdata_ptr_t mExternalOutput;                       // which EXTERNAL output plug are we connected to
  orkvector<outplugdata_ptr_t> mInternalOutputConnections; // which output plugs IN THE SAME MODULE are connected to me ?
  morphable_ptr_t mpMorphable;

  void ConnectInternal(outplugdata_ptr_t vt);
  void ConnectExternal(outplugdata_ptr_t vt);

  void DoSetDirty(bool bv) override; // virtual
};

///////////////////////////////////////////////////////////////////////////////

struct OutPlugData : public PlugData {

  DeclareAbstractX(OutPlugData, PlugData);

public:
  dataflow::node_hash& RefHash() {
    return mOutputHash;
  }

  OutPlugData(moduledata_ptr_t pmod, EPlugRate epr, const std::type_info& tid, const char* pname);
  ~OutPlugData();

  virtual int MaxFanOut() const {
    return 0;
  }

  bool IsConnected() const {
    return (GetNumExternalOutputConnections() != 0);
  }

  size_t GetNumExternalOutputConnections() const {
    return mExternalInputConnections.size();
  }
  inplugdata_ptr_t GetExternalOutputConnection(size_t idx) const {
    return mExternalInputConnections[idx];
  }

  dgregister* GetRegister() const {
    return mpRegister;
  }
  void SetRegister(dgregister* preg) {
    mpRegister = preg;
  }
  void Disconnect(inplugdata_ptr_t pinplug);

  mutable orkvector<inplugdata_ptr_t> mExternalInputConnections;

  void DoSetDirty(bool bv) override; // virtual

  dataflow::node_hash mOutputHash;
  dgregister* mpRegister;
};

///////////////////////////////////////////////////////////////////////////////

template <typename vartype> //
class outplug : public OutPlugData {
  DeclareTemplateConcreteX(outplug<vartype>, OutPlugData);

public:
  void operator=(const outplug<vartype>& oth) {
    new (this) outplug<vartype>(oth);
  }
  outplug(const outplug<vartype>& oth)
      : OutPlugData(oth.GetModule(), oth.GetPlugRate(), oth.GetDataTypeId(), oth.GetName().c_str())
      , mOutputData(oth.mOutputData) {
  }

  outplug()
      : OutPlugData(0, EPR_EVENT, typeid(vartype), 0)
      , mOutputData(0) {
  }

  outplug(moduledata_ptr_t pmod, EPlugRate epr, const vartype* def, const char* pname)
      : OutPlugData(pmod, epr, typeid(vartype), pname)
      , mOutputData(def) {
  }
  outplug(moduledata_ptr_t pmod, EPlugRate epr, const vartype* def, const std::type_info& tinfo, const char* pname)
      : OutPlugData(pmod, epr, tinfo, pname)
      , mOutputData(def) {
  }
  int MaxFanOut() const override {
    return MaxFanout<vartype>();
  }
  ///////////////////////////////////////////////////////////////
  void ConnectData(const vartype* pd) {
    mOutputData = pd;
  }
  ///////////////////////////////////////////////////////////////
  // Internal value access (will not ever give effective value)
  const vartype& GetInternalData() const;
  ///////////////////////////////////////////////////////////////
  const vartype& GetValue() const; // virtual
                                   ///////////////////////////////////////////////////////////////

private:
  const vartype* mOutputData;
};

///////////////////////////////////////////////////////////////////////////////

template <typename vartype> class inplug : public InPlugData {
  DeclareTemplateAbstractX(inplug<vartype>, InPlugData);

public:
  explicit inplug(moduledata_ptr_t pmod, EPlugRate epr, vartype& def, const char* pname)
      : InPlugData(pmod, epr, typeid(vartype), pname)
      , mDefault(def) {
  }

  void SetDefault(const vartype& val) {
    mDefault = val;
  }
  const vartype& GetDefault() const {
    return mDefault;
  }

  ////////////////////////////////////////////

  void Connect(outplug<vartype>* vt) {
    ConnectExternal(vt);
  }
  void Connect(outplug<vartype>& vt) {
    ConnectExternal(&vt);
  }

  ///////////////////////////////////////////////////////////////

  template <typename T> void GetTypedInput(outplug<T>*& oval) {
    outplugdata_ptr_t oplug = mExternalOutput;
    oval               = rtti::downcast<outplug<T>*>(oplug);
  }

  ///////////////////////////////////////////////////////////////

  inline const vartype& GetValue() // virtual
  {
    outplug<vartype>* connected = 0;
    GetTypedInput(connected);
    return (connected != 0) ? (connected->GetValue()) : mDefault;
  }

  ///////////////////////////////////////////////////////////////

protected:
  vartype& mDefault; // its a reference to prevent large plugs taking up memory
                     // in the unconnected state it can connect to a global dummy
};

///////////////////////////////////////////////////////////////////////////////

class floatinplug : public inplug<float> {
  DeclareAbstractX(floatinplug, inplug<float>);

public:
  floatinplug(moduledata_ptr_t pmod, EPlugRate epr, float& def, const char* pname)
      : inplug<float>(pmod, epr, def, pname) {
  }

private:
  void SetValAccessor(float const& val) {
    mDefault = val;
  }
  void GetValAccessor(float& val) const {
    val = mDefault;
  }
};

///////////////////////////////////////////////////////////////////////////////

class vect3inplug : public inplug<fvec3> {
  DeclareAbstractX(vect3inplug, inplug<fvec3>);

public:
  vect3inplug(moduledata_ptr_t pmod, EPlugRate epr, fvec3& def, const char* pname)
      : inplug<fvec3>(pmod, epr, def, pname) {
  }

private:
  void SetValAccessor(fvec3 const& val) {
    mDefault = val;
  }
  void GetValAccessor(fvec3& val) const {
    val = mDefault;
  }
};

///////////////////////////////////////////////////////////////////////////////

template <typename xf> class floatinplugxf : public floatinplug {
  DeclareTemplateAbstractX(floatinplugxf<xf>, floatinplug);

public:
  explicit floatinplugxf(moduledata_ptr_t pmod, EPlugRate epr, float& def, const char* pname)
      : floatinplug(pmod, epr, def, pname)
      , mtransform() {
  }

  ///////////////////////////////////////////////////////////////

  xf& GetTransform() {
    return mtransform;
  }

  ///////////////////////////////////////////////////////////////

  inline const float& GetValue() // virtual
  {
    outplug<float>* connected = 0;
    GetTypedInput(connected);
    mtransformed = mtransform.transform((connected != 0) ? (connected->GetValue()) : mDefault);
    return mtransformed;
  }

private:
  xf mtransform;
  mutable float mtransformed;
  ork::Object* XfAccessor() {
    return &mtransform;
  }
};

///////////////////////////////////////////////////////////////////////////////

template <typename xf> class vect3inplugxf : public vect3inplug {
  DeclareTemplateAbstractX(vect3inplugxf<xf>, vect3inplug);

public:
  explicit vect3inplugxf(moduledata_ptr_t pmod, EPlugRate epr, fvec3& def, const char* pname)
      : vect3inplug(pmod, epr, def, pname)
      , mtransform() {
  }

  ///////////////////////////////////////////////////////////////

  xf& GetTransform() {
    return mtransform;
  }

  ///////////////////////////////////////////////////////////////

  inline const fvec3& GetValue() // virtual
  {
    outplug<fvec3>* connected = 0;
    GetTypedInput(connected);
    mtransformed = mtransform.transform((connected != 0) ? (connected->GetValue()) : mDefault);
    return mtransformed;
  }

private:
  xf mtransform;
  mutable fvec3 mtransformed;
  ork::Object* XfAccessor() {
    return &mtransform;
  }
};

///////////////////////////////////////////////////////////////////////////////
class modscabias : public ork::Object {
  DeclareConcreteX(modscabias, ork::Object);

public:
  float GetMod() const {
    return mfMod;
  }
  float GetScale() const {
    return mfScale;
  }
  float GetBias() const {
    return mfBias;
  }
  void SetMod(float val) {
    mfMod = val;
  }
  void SetScale(float val) {
    mfScale = val;
  }
  void SetBias(float val) {
    mfBias = val;
  }

  modscabias()
      : mfMod(1.0f)
      , mfScale(1.0f)
      , mfBias(0.0f) {
  }

private:
  float mfMod;
  float mfScale;
  float mfBias;
};

///////////////////////////////////////////////////////////////////////////////

class floatxfitembase : public ork::Object {
  DeclareAbstractX(floatxfitembase, ork::Object);

public:
  virtual float transform(float inp) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

class floatxfmsbcurve : public floatxfitembase {
  DeclareConcreteX(floatxfmsbcurve, floatxfitembase);

public:
  float GetMod() const {
    return mModScaleBias.GetMod();
  }
  float GetScale() const {
    return mModScaleBias.GetScale();
  }
  float GetBias() const {
    return mModScaleBias.GetBias();
  }

  void SetMod(float val) {
    mModScaleBias.SetMod(val);
  }
  void SetScale(float val) {
    mModScaleBias.SetScale(val);
  }
  void SetBias(float val) {
    mModScaleBias.SetBias(val);
  }

  ork::Object* CurveAccessor() {
    return &mMultiCurve1d;
  }
  ork::Object* ModScaleBiasAccessor() {
    return &mModScaleBias;
  }

  floatxfmsbcurve()
      : mbDoModScaBia(false)
      , mbDoCurve(false) {
  }

  float transform(float input) const override; // virtual

private:
  ork::MultiCurve1D mMultiCurve1d;
  modscabias mModScaleBias;
  bool mbDoModScaBia;
  bool mbDoCurve;
};

///////////////////////////////////////////////////////////////////////////////

class floatxfmodstep : public floatxfitembase {
  DeclareConcreteX(floatxfmodstep, floatxfitembase);

public:
  floatxfmodstep()
      : mMod(1.0f)
      , miSteps(4)
      , mOutputScale(1.0f)
      , mOutputBias(1.0f) {
  }

  float transform(float input) const override; // virtual

private:
  float mMod;
  int miSteps;
  float mOutputScale;
  float mOutputBias;
};

///////////////////////////////////////////////////////////////////////////////

class floatxf : public ork::Object {
  DeclareAbstractX(floatxf, ork::Object);

public:
  float transform(float inp) const;
  floatxf();
  ~floatxf();

private:
  orklut<std::string, ork::Object*> mTransforms;
  int miTest;
};

///////////////////////////////////////////////////////////////////////////////

class vect3xf : public ork::Object {
  DeclareConcreteX(vect3xf, ork::Object);

public:
  const floatxf& GetTransformX() const {
    return mTransformX;
  }
  const floatxf& GetTransformY() const {
    return mTransformY;
  }
  const floatxf& GetTransformZ() const {
    return mTransformZ;
  }

  fvec3 transform(const fvec3& input) const;

private:
  ork::Object* TransformXAccessor() {
    return &mTransformX;
  }
  ork::Object* TransformYAccessor() {
    return &mTransformY;
  }
  ork::Object* TransformZAccessor() {
    return &mTransformZ;
  }

  floatxf mTransformX;
  floatxf mTransformY;
  floatxf mTransformZ;
};

typedef floatinplugxf<floatxf> floatxfinplug;
typedef vect3inplugxf<vect3xf> vect3xfinplug;

///////////////////////////////////////////////////////////////////////////////

struct ModuleData : public ork::Object {

public:

  DeclareAbstractX(ModuleData, ork::Object);

public:

  ModuleData();
  ~ModuleData();

  ////////////////////////////////////////////
  const dataflow::node_hash& GetModuleHash() {
    return mModuleHash;
  }
  virtual void UpdateHash() {
    mModuleHash = dataflow::node_hash();
  }
  ////////////////////////////////////////////
  const std::string& GetName() const {
    return mName;
  }
  void SetName(const std::string& val) {
    mName = val;
  }
  ////////////////////////////////////////////
  virtual int numInputs() const {
    return _numStaticInputs;
  }
  virtual int numOutputs() const {
    return _numStaticOutputs;
  }
  virtual inplugdata_ptr_t input(int idx) const {
    return staticInput(idx);
  }
  virtual outplugdata_ptr_t GetOutput(int idx) const {
    return staticOutput(idx);
  }
  inplugdata_ptr_t staticInput(int idx) const;
  outplugdata_ptr_t staticOutput(int idx) const;
  inplugdata_ptr_t inputNamed(const std::string& named);
  outplugdata_ptr_t outputNamed(const std::string& named);
  ////////////////////////////////////////////
  virtual int GetNumChildren() const {
    return 0;
  }
  virtual moduledata_ptr_t GetChild(int idx) const {
    return 0;
  }
  moduledata_ptr_t GetChildNamed(const std::string& named) const;
  ////////////////////////////////////////////
  template <typename T> std::shared_ptr<inplug<T>> typedInput(int idx) {
    inplugdata_ptr_t plug = input(idx);
    return std::dynamic_pointer_cast<T>(plug);
  }
  ////////////////////////////////////////////
  virtual void OnTopologyUpdate(void) {
  }
  virtual void OnStart() {
  }
  ////////////////////////////////////////////
  template <typename T> void GetTypedOutput(int idx, outplug<T>*& oval) {
    outplugdata_ptr_t oplug = GetOutput(idx);
    oval               = rtti::downcast<outplug<T>*>(oplug);
  }
  ////////////////////////////////////////////
  void SetMorphable(morphable_ptr_t pmorph) {
    mpMorphable = pmorph;
  }
  morphable_ptr_t GetMorphable() const {
    return mpMorphable;
  }
  bool IsMorphable() const {
    return (mpMorphable != 0);
  }
  ////////////////////////////////////////////
  void AddInput(inplugdata_ptr_t plg);
  void AddOutput(outplugdata_ptr_t plg);
  void RemoveInput(inplugdata_ptr_t plg);
  void RemoveOutput(outplugdata_ptr_t plg);

protected:
  std::string mName;
  dataflow::node_hash mModuleHash;
  morphable_ptr_t mpMorphable;
  int _numStaticInputs;
  int _numStaticOutputs;
  std::set<inplugdata_ptr_t> mStaticInputs;
  std::set<outplugdata_ptr_t> mStaticOutputs;

  void AddDependency(OutPlugData& pout, InPlugData& pin);
};

struct ModuleInst  {

  ModuleInst(moduledata_constptr_t absdata) : _abstract_module_data(absdata) {}
  moduledata_constptr_t _abstract_module_data;

  virtual void DoSetInputDirty(inplugdata_ptr_t plg) {
  }
  virtual void DoSetOutputDirty(outplugdata_ptr_t plg) {
  }
  int numOutputs() const {
    return _abstract_module_data->numOutputs();
  }
  void SetInputDirty(inplugdata_ptr_t plg);
  void SetOutputDirty(outplugdata_ptr_t plg);
  virtual bool IsDirty(void) const;

};

///////////////////////////////////////////////////////////////////////////////

struct DgModuleData : public ModuleData {

  DeclareAbstractX(DgModuleData, ModuleData);

public:
  DgModuleData();
  ////////////////////////////////////////////
  void SetParent(graphdata_ptr_t par) {
    _parent = par;
  }
  nodekey& Key() {
    return mKey;
  }
  const nodekey& Key() const {
    return mKey;
  }
  graphdata_ptr_t GetParent() const {
    return _parent;
  }
  ////////////////////////////////////////////
  // bool IsOutputDirty( const outplugdata_ptr_t pplug ) const;
  ////////////////////////////////////////////
  virtual void Compute(workunit* wu) = 0;
  ////////////////////////////////////////////
  void SetAffinity(Affinity ia) {
    mAffinity = ia;
  }
  const Affinity& GetAffinity() const {
    return mAffinity;
  }
  ////////////////////////////////////////////
  void DivideWork(const scheduler& sch, cluster* clus);
  virtual void CombineWork(const cluster* clus) = 0;
  ////////////////////////////////////////////
  virtual void ReleaseWorkUnit(workunit* wu);
  ////////////////////////////////////////////
  virtual graphdata_ptr_t GetChildGraph() const {
    return 0;
  }
  bool IsGroup() const {
    return GetChildGraph() != 0;
  }
  ////////////////////////////////////////////
  const fvec2& GetGVPos() const {
    return mgvpos;
  }
  void SetGVPos(const fvec2& p) {
    mgvpos = p;
  }

protected:
  virtual void DoDivideWork(const scheduler& sch, cluster* clus);

private:
  Affinity mAffinity;
  graphdata_ptr_t _parent;
  nodekey mKey;
  fvec2 mgvpos;
};


struct DgModuleInst : public ModuleInst {

  DgModuleInst(const DgModuleData& absdata);

};


class dyn_external {
public:
  struct FloatBinding {
    const float* mpSource;
    orklut<std::string, float>::iterator mIterator;
  };
  struct Vect3Binding {
    const fvec3* mpSource;
    orklut<std::string, fvec3>::iterator mIterator;
  };

  const orklut<std::string, FloatBinding>& GetFloatBindings() const {
    return mFloatBindings;
  }
  orklut<std::string, FloatBinding>& GetFloatBindings() {
    return mFloatBindings;
  }

  const orklut<std::string, Vect3Binding>& GetVect3Bindings() const {
    return mVect3Bindings;
  }
  orklut<std::string, Vect3Binding>& GetVect3Bindings() {
    return mVect3Bindings;
  }

private:
  orklut<std::string, FloatBinding> mFloatBindings;
  orklut<std::string, Vect3Binding> mVect3Bindings;
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
  std::set<dgmoduleinst_ptr_t> mChildren;
  dgmoduleinst_ptr_t mpOwner;
  dgregisterblock* mpBlock;
  //////////////////////////////////
  void SetModule(dgmoduleinst_ptr_t pmod);
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

class dgcontext {
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
  void Prune(dgmoduledata_ptr_t mod);
  void Alloc(outplugdata_ptr_t poutplug);
  void SetProbeModule(dgmoduledata_ptr_t pmod) {
    mpProbeModule = pmod;
  }

private:
  orkmap<const std::type_info*, dgregisterblock*> mRegisterSets;
  dgmoduledata_ptr_t mpProbeModule;
};

///////////////////////////////////////////////////////////////////////////////

struct dgqueue {
  std::set<dgmoduledata_ptr_t> pending;
  int mSerial;
  std::stack<dgmoduledata_ptr_t> mModStack;
  dgcontext& mCompCtx;
  //////////////////////////////////////////////////////////
  bool IsPending(dgmoduledata_ptr_t mod);
  size_t NumPending() {
    return pending.size();
  }
  int NumDownstream(dgmoduledata_ptr_t mod);
  int NumPendingDownstream(dgmoduledata_ptr_t mod);
  void AddModule(dgmoduledata_ptr_t mod);
  void PruneRegisters(dgmoduledata_ptr_t pmod);
  void QueModule(dgmoduledata_ptr_t pmod, int irecd);
  bool HasPendingInputs(dgmoduledata_ptr_t mod);
  void DumpInputs(dgmoduledata_ptr_t mod) const;
  void DumpOutputs(dgmoduledata_ptr_t mod) const;
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

///////////////////////////////////////////////////////////////////////////////
