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
  virtual int numChildren() const;
  virtual moduledata_ptr_t child(int idx) const;
  virtual int numInputs() const;
  virtual int numOutputs() const;
  virtual inplugdata_ptr_t input(int idx) const;
  virtual outplugdata_ptr_t output(int idx) const;
  inplugdata_ptr_t staticInput(int idx) const;
  outplugdata_ptr_t staticOutput(int idx) const;
  inplugdata_ptr_t inputNamed(const std::string& named);
  outplugdata_ptr_t outputNamed(const std::string& named);
  ////////////////////////////////////////////
  moduledata_ptr_t childNamed(const std::string& named) const;
  ////////////////////////////////////////////
  virtual void onTopologyUpdate(void);
  virtual void onStart();
  bool isMorphable() const;
  ////////////////////////////////////////////
  void addInput(inplugdata_ptr_t plg);
  void addOutput(outplugdata_ptr_t plg);
  void removeInput(inplugdata_ptr_t plg);
  void removeOutput(outplugdata_ptr_t plg);
  ////////////////////////////////////////////
  template <typename T> std::shared_ptr<inplugdata<T>> typedInput(int idx) {
    inplugdata_ptr_t plug = input(idx);
    return std::dynamic_pointer_cast<T>(plug);
  }
  ////////////////////////////////////////////
  template <typename T> std::shared_ptr<outplug<T>> typedOutput(int idx) {
    outplugdata_ptr_t plug = output(idx);
    return std::dynamic_pointer_cast<T>(plug);
  }

  std::string mName;
  dataflow::node_hash mModuleHash;
  morphable_ptr_t mpMorphable;
  int _numStaticInputs;
  int _numStaticOutputs;
  std::set<inplugdata_ptr_t> mStaticInputs;
  std::set<outplugdata_ptr_t> mStaticOutputs;

  void AddDependency(OutPlugData& pout, InPlugData& pin);
};

///////////////////////////////////////////////////////////////////////////////

struct ModuleInst {

  ModuleInst(moduledata_constptr_t absdata);
  
  int numOutputs() const;
  int numInputs() const;
  inpluginst_ptr_t input(int idx) const;
  outpluginst_ptr_t output(int idx) const;
  inpluginst_ptr_t staticInput(int idx) const;
  outpluginst_ptr_t staticOutput(int idx) const;
  inpluginst_ptr_t inputNamed(const std::string& named);
  outpluginst_ptr_t outputNamed(const std::string& named);


  void setInputDirty(inpluginst_ptr_t plg);
  void setOutputDirty(outpluginst_ptr_t plg);

  virtual void _doSetInputDirty(inpluginst_ptr_t plg);
  virtual void _doSetOutputDirty(outpluginst_ptr_t plg);
  virtual bool isDirty(void) const;

  moduledata_constptr_t _abstract_module_data;
  std::set<inplugdata_ptr_t> mStaticInputs;
  std::set<outplugdata_ptr_t> mStaticOutputs;

};

///////////////////////////////////////////////////////////////////////////////

struct DgModuleData : public ModuleData {

  DeclareAbstractX(DgModuleData, ModuleData);

public:
  DgModuleData();
  void divideWork(const scheduler& sch, cluster* clus);
  bool isGroup() const;
  ////////////////////////////////////////////
  // bool IsOutputDirty( const outplugdata_ptr_t pplug ) const;
  ////////////////////////////////////////////
  virtual void Compute(workunit* wu) = 0;
  virtual void combineWork(const cluster* clus) = 0;
  virtual void releaseWorkUnit(workunit* wu);
  virtual graphdata_ptr_t childGraph() const;

  Affinity mAffinity;
  graphdata_ptr_t _parent;
  nodekey mKey;
  fvec2 mgvpos;

protected:
  virtual void _doDivideWork(const scheduler& sch, cluster* clus);

};

///////////////////////////////////////////////////////////////////////////////

struct DgModuleInst : public ModuleInst {

  DgModuleInst(const DgModuleData& absdata);

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
