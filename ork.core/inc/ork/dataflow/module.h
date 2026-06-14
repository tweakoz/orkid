////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::dataflow {

///////////////////////////////////////////////////////////////////////////////

struct ModuleData : public ork::Object {
  DeclareAbstractX(ModuleData, ork::Object);
public:
  ////////////////////////////////////////////
  ModuleData();
  ~ModuleData();
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) override;
  ////////////////////////////////////////////
  virtual void UpdateHash();
  ////////////////////////////////////////////
  int numInputs() const;
  int numOutputs() const;
  virtual inplugdata_ptr_t input(int idx) const;
  virtual outplugdata_ptr_t output(int idx) const;
  virtual inplugdata_ptr_t inputNamed(const std::string& named) const;
  virtual outplugdata_ptr_t outputNamed(const std::string& named) const;
  ////////////////////////////////////////////
  template <typename plug_traits,typename... A> static //
  std::shared_ptr<inplugdata<plug_traits>> createInputPlug(moduledata_ptr_t m, A&&... args);
  template <typename plug_traits,typename... A> static //
  std::shared_ptr<outplugdata<plug_traits>> createOutputPlug(moduledata_ptr_t m, A&&... args);
  ////////////////////////////////////////////
  virtual void onTopologyUpdate(void);
  virtual void onStart();
  ////////////////////////////////////////////
  void addInput(inplugdata_ptr_t plg);
  void addOutput(outplugdata_ptr_t plg);
  void removeInput(inplugdata_ptr_t plg);
  void removeOutput(outplugdata_ptr_t plg);
  ////////////////////////////////////////////
  void addDependency(outplugdata_ptr_t pout, inplugdata_ptr_t pin);
  ////////////////////////////////////////////
  template <typename plug_traits> //
  std::shared_ptr<inplugdata<plug_traits>> typedInput(int idx);
  ////////////////////////////////////////////
  template <typename plug_traits> //
  std::shared_ptr<outplugdata<plug_traits>> typedOutput(int idx);
  ////////////////////////////////////////////
  template <typename plug_traits> //
  std::shared_ptr<inplugdata<plug_traits>> typedInputNamed(const std::string& named) const;
  ////////////////////////////////////////////
  template <typename plug_traits> //
  std::shared_ptr<outplugdata<plug_traits>> typedOutputNamed(const std::string& named) const;
  ////////////////////////////////////////////

  std::string _name;
  graphdata_ptr_t _graphdata;
  dataflow::node_hash mModuleHash;
  std::vector<inplugdata_ptr_t> _inputs;
  std::vector<outplugdata_ptr_t> _outputs;


};

///////////////////////////////////////////////////////////////////////////////

struct DgModuleData : public ModuleData {

  DeclareAbstractX(DgModuleData, ModuleData);

public:
  DgModuleData();
  bool isGroup() const;
  ////////////////////////////////////////////
  static dgmoduledata_ptr_t createShared();
  virtual dgmoduleinst_ptr_t createInstance(GraphInst* ginst) const;
  ////////////////////////////////////////////
  size_t computeMinDepth() const;
  size_t computeMaxDepth() const;
  ////////////////////////////////////////////
  virtual graphdata_ptr_t childGraph() const;

  Affinity mAffinity;
  graphdata_ptr_t _parent;
  fvec2 mgvpos;
  bool _prunable = true;

private:
  // Cycle-safe, memoized recursive helpers. `on_path` tracks the current DFS
  // path to terminate on cycles (return 0 — cycle node contributes nothing;
  // DgSorter::generateTopology detects the cycle independently and returns
  // nullptr with the offender list). `memo` caches each fully-resolved module's
  // depth so re-convergent (diamond) DAGs are walked in O(V+E) rather than
  // re-traversing every distinct upstream path (which is exponential in the
  // number of reconvergence points — e.g. deep iterated erosion graphs).
  size_t _computeMinDepth(dgmoduleset_t& on_path, dgmoduledepthmap_t& memo) const;
  size_t _computeMaxDepth(dgmoduleset_t& on_path, dgmoduledepthmap_t& memo) const;
};

///////////////////////////////////////////////////////////////////////////////

struct LambdaModuleData : public DgModuleData {
  DeclareAbstractX(LambdaModuleData, DgModuleData);
public:
  LambdaModuleData();
  static std::shared_ptr<LambdaModuleData> createShared();
  dgmoduleinst_ptr_t createInstance(GraphInst* ginst) const final;

  using compute_lamda_t = std::function<void(graphinst_ptr_t,ui::updatedata_ptr_t)>;
  using link_lamda_t = std::function<void(graphinst_ptr_t)>;

  compute_lamda_t _computeLambda;
  link_lamda_t _linkLambda;

};
///////////////////////////////////////////////////////////////////////////////

struct ModuleInst {

  ModuleInst(const ModuleData* _this, GraphInst* ginst);
  
  int numOutputs() const;
  int numInputs() const;
  inpluginst_ptr_t input(int idx) const;
  outpluginst_ptr_t output(int idx) const;
  inpluginst_ptr_t inputNamed(const std::string& named) const;
  outpluginst_ptr_t outputNamed(const std::string& named) const;
  std::string name() const;

  void setInputDirty(inpluginst_ptr_t plg);
  void setOutputDirty(outpluginst_ptr_t plg);
  virtual void _doSetInputDirty(inpluginst_ptr_t plg);
  virtual void _doSetOutputDirty(outpluginst_ptr_t plg);
  virtual bool isDirty(void) const;

  const ModuleData* _abstract_module_data;
  GraphInst* _graphinst = nullptr;
  std::vector<inpluginst_ptr_t> _inputs;
  std::vector<outpluginst_ptr_t> _outputs;
  std::unordered_map<std::string,inpluginst_ptr_t> _inputsByName;
  std::unordered_map<std::string,outpluginst_ptr_t> _outputsByName;

};

///////////////////////////////////////////////////////////////////////////////

struct DgModuleInst : public ModuleInst {

  DgModuleInst(const DgModuleData* _this, GraphInst* ginst);
  virtual ~DgModuleInst();

  virtual void onLink(GraphInst* inst) {}
  virtual void onStage(GraphInst* inst) {}
  virtual void onActivate(GraphInst* inst) {}
  virtual void compute(GraphInst* inst,ui::updatedata_ptr_t updata) {}

  ////////////////////////////////////////////
  // cook cache (only used when the owning GraphData is _cacheable). The node's
  // content hash is computed by GraphInst::computeCooked; these two hooks are the
  // TYPE-SPECIFIC serialize/load of this node's output. Default = not cacheable
  // (cookStore returns null -> always recompute), so modules opt in by overriding.
  uint64_t _cookHash = 0;
  // this node's CONTENT hash = f( version-salt, own scalar params, context,
  // [upstream node hashes] ). Per-class override (each adds a version salt so a
  // change to the op's logic invalidates its cache). Default 0 = no identity
  // (sinks / uncacheable nodes). Never hashes output buffers — cheap.
  virtual uint64_t cookComputeHash(const std::vector<uint64_t>& input_hashes, uint64_t context) const { return 0; }
  // serialize this node's freshly-computed output to a datablock (e.g. SSBO
  // readback). null = don't cache this node.
  virtual datablock_ptr_t cookStore() const { return nullptr; }
  // restore this node's output FROM a cached datablock (e.g. upload to an SSBO),
  // so downstream nodes can consume it without this node recomputing. Return true
  // if the load succeeded (then compute() is skipped).
  virtual bool cookLoad(datablock_constptr_t db) { return false; }
  ////////////////////////////////////////////
  // Called by GraphInst::reset(). Default is a no-op; modules that hold
  // per-instance state needing a clean re-start (e.g. Globals' first-compute
  // timebase capture, particle pools' live particles, RNG seeds) override
  // this. Used by ECS particle slot recycle (A3).
  virtual void onReset(GraphInst* inst) {}
    
  //void divideWork(scheduler_ptr_t sch, cluster* clus);
  //virtual void _doDivideWork(scheduler_ptr_t sch, cluster* clus);
  //virtual void Compute(workunit* wu) {}
  //virtual void combineWork(const cluster* clus) {}
  //virtual void releaseWorkUnit(workunit* wu);

  ////////////////////////////////////////////
  template <typename plug_type> //
  std::shared_ptr<inpluginst<plug_type>> typedInput(int idx) const;
  template <typename plug_type> //
  std::shared_ptr<outpluginst<plug_type>> typedOutput(int idx) const;
  template <typename plug_type> //
  std::shared_ptr<inpluginst<plug_type>> typedInputNamed(const std::string& named) const;
  template <typename plug_type> //
  std::shared_ptr<outpluginst<plug_type>> typedOutputNamed(const std::string& named) const;
  ////////////////////////////////////////////

  const DgModuleData* _dgmodule_data;

};

///////////////////////////////////////////////////////////////////////////////

struct dyn_external {
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

} //namespace ork { namespace dataflow {
