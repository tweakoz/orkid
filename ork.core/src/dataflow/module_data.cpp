////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/registerX.inl>

#include <ork/math/cvector2.hpp>
#include <ork/math/cvector3.hpp>
#include <ork/math/cvector4.hpp>
#include <ork/math/quaternion.hpp>
#include <ork/math/cmatrix3.hpp>
#include <ork/math/cmatrix4.hpp>

#include <ork/dataflow/all.h>
#include <ork/dataflow/module.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ModuleData::describeX(class_t* clazz) {
}
ModuleData::ModuleData(){
}
ModuleData::~ModuleData() {
}

void ModuleData::UpdateHash() {
  mModuleHash = dataflow::node_hash();
}
///////////////////////////////////////////////////////////////////////////////
int ModuleData::numInputs() const {
  return _inputs.size();
}
int ModuleData::numOutputs() const {
  return _outputs.size();
}
///////////////////////////////////////////////////////////////////////////////
inplugdata_ptr_t ModuleData::input(int idx) const {
  return _inputs[idx];
}
outplugdata_ptr_t ModuleData::output(int idx) const {
  return _outputs[idx];
}
///////////////////////////////////////////////////////////////////////////////
inplugdata_ptr_t ModuleData::inputNamed(const std::string& named) const {
  for( auto item : _inputs ){
    if (named == item->_name) {
      return item;
    }
  }
  return nullptr;
}
outplugdata_ptr_t ModuleData::outputNamed(const std::string& named) const {
  for( auto item : _outputs ){
    if (named == item->_name) {
      return item;
    }
  }
  return nullptr;
}
////////////////////////////////////////////
void ModuleData::onTopologyUpdate(void) {
}
void ModuleData::onStart() {
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::addDependency(outplugdata_ptr_t pout, inplugdata_ptr_t pin) {
  //pin->connectInternal(&pout);
  // DepPlugSet::value_type v( & pin, & pout );
  // mDependencies.insert( v );
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::addInput(inplugdata_ptr_t plg) {
  _inputs.push_back(plg);
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::addOutput(outplugdata_ptr_t plg) {
  _outputs.push_back(plg);
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::removeInput(inplugdata_ptr_t plg) {
  auto it = std::find(_inputs.begin(),_inputs.end(),plg);
  if (it != _inputs.end()) {
    _inputs.erase(it);
  }
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::removeOutput(outplugdata_ptr_t plg) {
  auto it = std::find(_outputs.begin(),_outputs.end(),plg);
  if (it != _outputs.end()) {
    _outputs.erase(it);
  }
}

/*bool ModuleData::IsOutputDirty(const ork::dataflow::outplugbase *pplug) const
{
        bool bv = false;
        for( DepPlugSet::const_iterator it=mDependencies.begin();
it!=mDependencies.end(); it++ )
        {
                const DepPlugSet::value_type& v = *it;

                inplugdata_ptr_t pin = v.first;
                const outplugdata_ptr_t pout = v.second;

                if( pout == pplug )
                {
                        bv |= pin->IsDirty();
                }
        }
        return bv;
}*/
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void DgModuleData::describeX(class_t* clazz) {
  // ork::reflect::RegisterProperty("mgvpos", &DgModuleData::mgvpos);
  // ork::reflect::annotatePropertyForEditor<dgmodule>("mgvpos", "editor.visible", "false");
}
///////////////////////////////////////////////////////////////////////////////
DgModuleData::DgModuleData()
    : mAffinity(dataflow::scheduler::CpuAffinity)
    , _parent(nullptr){
}
bool DgModuleData::isGroup() const {
  return childGraph() != nullptr;
}
graphdata_ptr_t DgModuleData::childGraph() const {
  return nullptr;
}
dgmoduledata_ptr_t DgModuleData::createShared(){
    return std::make_shared<DgModuleData>();
}
dgmoduleinst_ptr_t DgModuleData::createInstance() const{
  return std::make_shared<DgModuleInst>(this);
}
size_t DgModuleData::computeMinDepth() const{
    size_t min_depth = InPlugData::NOPATH;
    for( auto upstream_input : _inputs ){
      auto upstream_plug = upstream_input->_connectedOutput;
      if(upstream_plug){
        auto upstream_module = typedModuleData<DgModuleData>(upstream_plug->_parent_module);
        size_t upstream_depth = upstream_module->computeMinDepth()+1;
        if(upstream_depth<min_depth){
          min_depth = upstream_depth;
        }
      }
    }
    if(min_depth==InPlugData::NOPATH)
      return 0;
    return min_depth;
}
size_t DgModuleData::computeMaxDepth() const{
    size_t max_depth = 0;
    for( auto upstream_input : _inputs ){
      auto upstream_plug = upstream_input->_connectedOutput;
      if(upstream_plug){
        auto upstream_module = typedModuleData<DgModuleData>(upstream_plug->_parent_module);
        size_t upstream_depth = upstream_module->computeMaxDepth()+1;
        if(upstream_depth>max_depth){
          max_depth = upstream_depth;
        }
      }
    }
    return max_depth;
}

//////////////////////////////////////////////////////////////////////////

struct LambdaModuleInst : public DgModuleInst {

  LambdaModuleInst(const LambdaModuleData* lmd)
      : DgModuleInst(lmd)
      , _lmd(lmd) {
  }

  ////////////////////////////////////////////////////

  void onLink(GraphInst* inst) final {
    _lmd->_linkLambda(inst->_sharedThis);
  }

  ////////////////////////////////////////////////////

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    _lmd->_computeLambda(inst->_sharedThis,updata);
  }

  const LambdaModuleData* _lmd;
  std::shared_ptr<LambdaModuleInst> _sharedThis;
};

//////////////////////////////////////////////////////////////////////////

void LambdaModuleData::describeX(class_t* clazz) {
}

//////////////////////////////////////////////////////////////////////////

LambdaModuleData::LambdaModuleData() {
  _linkLambda = [](graphinst_ptr_t){};
  _computeLambda = [](graphinst_ptr_t,ui::updatedata_ptr_t){};
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<LambdaModuleData> LambdaModuleData::createShared() {
  auto data = std::make_shared<LambdaModuleData>();
  return data;
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t LambdaModuleData::createInstance() const {
  auto inst = std::make_shared<LambdaModuleInst>(this);
  inst->_sharedThis = inst;
  return inst;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::dataflow::ModuleData, "dflow::ModuleData");
ImplementReflectionX(ork::dataflow::DgModuleData, "dflow::DgModuleData");
ImplementReflectionX(ork::dataflow::LambdaModuleData, "dflow::LambdaModuleData");
