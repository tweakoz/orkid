////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>
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

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::dataflow::ModuleData, "dflow/ModuleData");
ImplementReflectionX(ork::dataflow::DgModuleData, "dflow/DgModuleData");

///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ModuleData::describeX(class_t* clazz) {
}
ModuleData::ModuleData()
    : mpMorphable(nullptr) {
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
bool ModuleData::isMorphable() const {
  return (mpMorphable != 0);
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
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////
