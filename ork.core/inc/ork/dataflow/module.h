////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
  std::shared_ptr<inplugdata<plug_traits>> createInputPlug(moduledata_ptr_t m, A&&... args){
    using plug_impl_type = inplugdata<plug_traits>;
    auto plg = std::make_shared<plug_impl_type>(m,std::forward<A>(args)...);
    m->addInput(plg);
    return plg;
  }
  ////////////////////////////////////////////
  template <typename plug_traits,typename... A> static //
  std::shared_ptr<outplugdata<plug_traits>> createOutputPlug(moduledata_ptr_t m, A&&... args){
    using plug_impl_type = outplugdata<plug_traits>;
    auto plg = std::make_shared<plug_impl_type>(m,std::forward<A>(args)...);
    m->addOutput(plg);
    return plg;
  }
//  moduledata_ptr_t childNamed(const std::string& named) const;
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
  std::shared_ptr<inplugdata<plug_traits>> typedInput(int idx) {
    inplugdata_ptr_t plug = input(idx);
    return std::dynamic_pointer_cast<inplugdata<plug_traits>>(plug);
  }
  ////////////////////////////////////////////
  template <typename plug_traits> //
  std::shared_ptr<outplugdata<plug_traits>> typedOutput(int idx) {
    outplugdata_ptr_t plug = output(idx);
    return std::dynamic_pointer_cast<outplugdata<plug_traits>>(plug);
  }
  ////////////////////////////////////////////
  template <typename plug_traits> //
  std::shared_ptr<inplugdata<plug_traits>> typedInputNamed(const std::string& named) const {
    inplugdata_ptr_t plug = inputNamed(named);
    return std::dynamic_pointer_cast<inplugdata<plug_traits>>(plug);
  }
  ////////////////////////////////////////////
  template <typename plug_traits> //
  std::shared_ptr<outplugdata<plug_traits>> typedOutputNamed(const std::string& named) const {
    outplugdata_ptr_t plug = outputNamed(named);
    return std::dynamic_pointer_cast<outplugdata<plug_traits>>(plug);
  }
  ////////////////////////////////////////////

  std::string _name;
  graphdata_ptr_t _graphdata;
  dataflow::node_hash mModuleHash;
  std::vector<inplugdata_ptr_t> _inputs;
  std::vector<outplugdata_ptr_t> _outputs;

};

template <typename T> std::shared_ptr<T> typedModuleData(object_ptr_t m){
  return std::dynamic_pointer_cast<T>(m);
}

///////////////////////////////////////////////////////////////////////////////

struct DgModuleData : public ModuleData {

  DeclareAbstractX(DgModuleData, ModuleData);

public:
  DgModuleData();
  bool isGroup() const;
  ////////////////////////////////////////////
  virtual dgmoduleinst_ptr_t createInstance() const;
  size_t computeMinDepth() const;
  size_t computeMaxDepth() const;
  ////////////////////////////////////////////
  virtual graphdata_ptr_t childGraph() const;

  Affinity mAffinity;
  graphdata_ptr_t _parent;
  fvec2 mgvpos;
  bool _prunable = true;

};

///////////////////////////////////////////////////////////////////////////////

struct ModuleInst {

  ModuleInst(const ModuleData* _this);
  
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
  std::vector<inpluginst_ptr_t> _inputs;
  std::vector<outpluginst_ptr_t> _outputs;
  std::unordered_map<std::string,inpluginst_ptr_t> _inputsByName;
  std::unordered_map<std::string,outpluginst_ptr_t> _outputsByName;

};

template <typename T> std::shared_ptr<T> typedModuleInst(moduleinst_ptr_t m){
  return std::dynamic_pointer_cast<T>(m);
}

///////////////////////////////////////////////////////////////////////////////

struct DgModuleInst : public ModuleInst {

  DgModuleInst(const DgModuleData* _this);
  virtual ~DgModuleInst();

  virtual void onLink(GraphInst* inst) {}
  virtual void onStage(GraphInst* inst) {}
  virtual void onActivate(GraphInst* inst) {}
  virtual void compute(GraphInst* inst,ui::updatedata_ptr_t updata) {}
    
  //void divideWork(scheduler_ptr_t sch, cluster* clus);
  //virtual void _doDivideWork(scheduler_ptr_t sch, cluster* clus);
  //virtual void Compute(workunit* wu) {}
  //virtual void combineWork(const cluster* clus) {}
  //virtual void releaseWorkUnit(workunit* wu);

  ////////////////////////////////////////////
  template <typename plug_traits> //
  std::shared_ptr<inpluginst<plug_traits>> typedInput(int idx) const {
    inpluginst_ptr_t plug = input(idx);
    return std::dynamic_pointer_cast<inpluginst<plug_traits>>(plug);
  }
  ////////////////////////////////////////////
  template <typename plug_traits> //
  std::shared_ptr<outpluginst<plug_traits>> typedOutput(int idx) const {
    outpluginst_ptr_t plug = output(idx);
    return std::dynamic_pointer_cast<outpluginst<plug_traits>>(plug);
  }
  ////////////////////////////////////////////
  template <typename plug_traits> //
  std::shared_ptr<inpluginst<plug_traits>> typedInputNamed(const std::string& named) const {
    inpluginst_ptr_t plug = inputNamed(named);
    return std::dynamic_pointer_cast<inpluginst<plug_traits>>(plug);
  }
  ////////////////////////////////////////////
  template <typename plug_traits> //
  std::shared_ptr<outpluginst<plug_traits>> typedOutputNamed(const std::string& named) const {
    outpluginst_ptr_t plug = outputNamed(named);
    return std::dynamic_pointer_cast<outpluginst<plug_traits>>(plug);
  }

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
