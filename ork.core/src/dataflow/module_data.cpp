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
    : mpMorphable(nullptr)
    , _numStaticInputs(0)
    , _numStaticOutputs(0) {
}
ModuleData::~ModuleData() {
}

void ModuleData::UpdateHash() {
  mModuleHash = dataflow::node_hash();
}
int ModuleData::numInputs() const {
  return _numStaticInputs;
}
int ModuleData::numOutputs() const {
  return _numStaticOutputs;
}
inplugdata_ptr_t ModuleData::input(int idx) const {
    return staticInput(idx);
  }
outplugdata_ptr_t ModuleData:: output(int idx) const {
    return staticOutput(idx);
  }

/*int ModuleData::numChildren() const {
  return 0;
}
moduledata_ptr_t ModuleData::child(int idx) const {
  return nullptr;
}*/
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
  auto it = mStaticInputs.find(plg);
  if (it == mStaticInputs.end()) {
    mStaticInputs.insert(plg);
    _numStaticInputs++;
  }
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::addOutput(outplugdata_ptr_t plg) {
  auto it = mStaticOutputs.find(plg);
  if (it == mStaticOutputs.end()) {
    mStaticOutputs.insert(plg);
    _numStaticOutputs++;
  }
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::removeInput(inplugdata_ptr_t plg) {
  auto it = mStaticInputs.find(plg);
  if (it != mStaticInputs.end()) {
    mStaticInputs.erase(it);
    _numStaticInputs--;
  }
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::removeOutput(outplugdata_ptr_t plg) {
  auto it = mStaticOutputs.find(plg);
  if (it != mStaticOutputs.end()) {
    mStaticOutputs.erase(it);
    _numStaticOutputs--;
  }
}
///////////////////////////////////////////////////////////////////////////////
inplugdata_ptr_t ModuleData::staticInput(int idx) const {
  int size = mStaticInputs.size();
  auto it  = mStaticInputs.begin();
  for (int i = 0; i < idx; i++) {
    it++;
  }
  inplugdata_ptr_t rval = (it != mStaticInputs.end()) ? *it : nullptr;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
outplugdata_ptr_t ModuleData::staticOutput(int idx) const {
  int size = mStaticOutputs.size();
  auto it  = mStaticOutputs.begin();
  for (int i = 0; i < idx; i++) {
    it++;
  }
  outplugdata_ptr_t rval = (it != mStaticOutputs.end()) ? *it : nullptr;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
inplugdata_ptr_t ModuleData::inputNamed(const std::string& named) const {
  int inuminp = numInputs();
  for (int ip = 0; ip < inuminp; ip++) {
    inplugdata_ptr_t rval = input(ip);
    OrkAssert(rval != nullptr);
    if (named == rval->_name) {
      return rval;
    }
  }
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
outplugdata_ptr_t ModuleData::outputNamed(const std::string& named) const {
  int inumout = numOutputs();
  printf("module<%p> numouts<%d>\n", (void*) this, inumout);
  for (int ip = 0; ip < inumout; ip++) {
    outplugdata_ptr_t rval = output(ip);
    OrkAssert(rval != nullptr);
    if (named == rval->_name) {
      return rval;
    }
  }
  return 0;
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
