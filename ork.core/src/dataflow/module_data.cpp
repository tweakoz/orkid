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
  // DATA-DRIVEN modules (e.g. psys Parameters) build plugs from reflected state that
  // deserializes BEFORE these arrays (derived-class properties precede base-class ones in
  // the stream). A freshly-constructed instance has no such plugs yet, so the deserializer's
  // pre-instantiated-slot fetch comes up null — this fallback runs the class's reshapeIOs
  // (idempotent per the reshape-runs-twice contract) to build them, then the fetch retries.
  reflect::array_instantiation_fallback_t reshape_now = [](object_ptr_t obj) {
    if (auto as_mod = std::dynamic_pointer_cast<ModuleData>(obj)) {
      // IArray::deserialize RESIZES the plug vector to the serialized count before parsing
      // elements, padding a data-driven module's missing plugs with NULL slots. Strip those
      // first — reshape rebuilds the real plugs, and a null slot is meaningless to dedup
      // against (and would crash name comparisons).
      auto strip_nulls = [](auto& vec) {
        vec.erase(std::remove(vec.begin(), vec.end(), nullptr), vec.end());
      };
      strip_nulls(as_mod->_inputs);
      strip_nulls(as_mod->_outputs);
      auto clazz = as_mod->GetClass();
      if (auto try_reshape = clazz->annotationTyped<moduleIOreshape_fn_t>("reshapeIOs")) {
        try_reshape.value()(as_mod);
      }
    }
  };
  clazz->directObjectVectorProperty("inputs", &ModuleData::_inputs)
      ->annotate<bool>("reflect.no_instantiate", true)
      ->annotate<reflect::array_instantiation_fallback_t>("reflect.no_instantiate.fallback", reshape_now);
  clazz->directObjectVectorProperty("outputs", &ModuleData::_outputs)
      ->annotate<bool>("reflect.no_instantiate", true)
      ->annotate<reflect::array_instantiation_fallback_t>("reflect.no_instantiate.fallback", reshape_now);
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
bool ModuleData::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) {

  auto as_mdataptr = std::dynamic_pointer_cast<ModuleData>(shared);
  auto clazz = as_mdataptr->GetClass();
  if( auto try_reshape = clazz->annotationTyped<moduleIOreshape_fn_t>("reshapeIOs") ){
    try_reshape.value()(as_mdataptr);
  }
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::addDependency(outplugdata_ptr_t pout, inplugdata_ptr_t pin) {
  //pin->connectInternal(&pout);
  // DepPlugSet::value_type v( & pin, & pout );
  // mDependencies.insert( v );
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::addInput(inplugdata_ptr_t plg) {
  auto name = plg->_name;
  bool should_add = true;
  for( auto item : _inputs ){
    if (name == item->_name) {
      should_add = false;
    }
  }
  if(should_add){
    _inputs.push_back(plg);
  }
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::addOutput(outplugdata_ptr_t plg) {
  // dedup by name (symmetric with addInput). reshapeIOs runs TWICE on deserialize
  // (createShared factory + postDeserialize hook); without this, the second pass
  // appends a duplicate same-named output plug. The connected edge points at the
  // first, but the module writes the second (_outputsByName is last-wins), so a
  // reader sees an unallocated buffer. Same-named outputs are unaddressable anyway.
  auto name       = plg->_name;
  bool should_add = true;
  for (auto item : _outputs) {
    if (name == item->_name) {
      should_add = false;
    }
  }
  if (should_add) {
    _outputs.push_back(plg);
  }
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
dgmoduleinst_ptr_t DgModuleData::createInstance(GraphInst* ginst) const{
  return std::make_shared<DgModuleInst>(this,ginst);
}
size_t DgModuleData::computeMinDepth() const{
    dgmoduleset_t on_path;
    dgmoduledepthmap_t memo;
    return _computeMinDepth(on_path, memo);
}
size_t DgModuleData::computeMaxDepth() const{
    dgmoduleset_t on_path;
    dgmoduledepthmap_t memo;
    return _computeMaxDepth(on_path, memo);
}
size_t DgModuleData::_computeMinDepth(dgmoduleset_t& on_path, dgmoduledepthmap_t& memo) const{
    // already fully resolved on a prior path — reuse (kills exponential re-walk
    // of re-convergent / diamond DAGs).
    auto itmemo = memo.find(this);
    if(itmemo != memo.end())
      return itmemo->second;
    if(!on_path.insert(this).second){
      // cycle — this node is already on the DFS path; stop here WITHOUT
      // memoizing (its true depth is path-dependent only in the cyclic case,
      // which DgSorter::generateTopology detects and fails independently).
      return 0;
    }
    size_t min_depth = InPlugData::NOPATH;
    for( auto upstream_input : _inputs ){
      auto upstream_plug = upstream_input->_connectedOutput;
      if(upstream_plug){
        auto upstream_module = typedModuleData<DgModuleData>(upstream_plug->_parent_module);
        size_t upstream_depth = upstream_module->_computeMinDepth(on_path, memo)+1;
        if(upstream_depth<min_depth){
          min_depth = upstream_depth;
        }
      }
    }
    on_path.erase(this);
    size_t result = (min_depth==InPlugData::NOPATH) ? 0 : min_depth;
    memo[this] = result; // fully resolved (not a cycle-cut) — safe to cache
    return result;
}
size_t DgModuleData::_computeMaxDepth(dgmoduleset_t& on_path, dgmoduledepthmap_t& memo) const{
    auto itmemo = memo.find(this);
    if(itmemo != memo.end())
      return itmemo->second;
    if(!on_path.insert(this).second){
      return 0;
    }
    size_t max_depth = 0;
    for( auto upstream_input : _inputs ){
      auto upstream_plug = upstream_input->_connectedOutput;
      if(upstream_plug){
        auto upstream_module = typedModuleData<DgModuleData>(upstream_plug->_parent_module);
        size_t upstream_depth = upstream_module->_computeMaxDepth(on_path, memo)+1;
        if(upstream_depth>max_depth){
          max_depth = upstream_depth;
        }
      }
    }
    on_path.erase(this);
    memo[this] = max_depth; // fully resolved (not a cycle-cut) — safe to cache
    return max_depth;
}

//////////////////////////////////////////////////////////////////////////

struct LambdaModuleInst : public DgModuleInst {

  LambdaModuleInst(const LambdaModuleData* lmd, GraphInst* ginst)
      : DgModuleInst(lmd,ginst)
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

dgmoduleinst_ptr_t LambdaModuleData::createInstance(GraphInst* ginst) const {
  auto inst = std::make_shared<LambdaModuleInst>(this,ginst);
  inst->_sharedThis = inst;
  return inst;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::dataflow::ModuleData, "dflow::ModuleData");
ImplementReflectionX(ork::dataflow::DgModuleData, "dflow::DgModuleData");
ImplementReflectionX(ork::dataflow::LambdaModuleData, "dflow::LambdaModuleData");
