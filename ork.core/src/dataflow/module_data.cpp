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
template class ork::orklut<ork::PoolString, ork::dataflow::ModuleData*>;

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
ModuleData::module()
    : mpMorphable(nullptr)
    , mNumStaticInputs(0)
    , mNumStaticOutputs(0) {
}
ModuleData::~module() {
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

int ModuleData::numChildren() const {
  return 0;
}
moduledata_ptr_t ModuleData::child(int idx) const {
  return nullptr;
}
////////////////////////////////////////////
void ModuleData::OnTopologyUpdate(void) {
}
void ModuleData::OnStart() {
}
bool ModuleData::IsMorphable() const {
  return (mpMorphable != 0);
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::SetInputDirty(inplugdata_ptr_t plg) {
  DoSetInputDirty(plg);
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::SetOutputDirty(outplugdata_ptr_t plg) {
  DoSetOutputDirty(plg);
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::AddDependency(outplugbase& pout, inplugbase& pin) {
  pin.ConnectInternal(&pout);
  // DepPlugSet::value_type v( & pin, & pout );
  // mDependencies.insert( v );
}
void ModuleData::addInput(inplugdata_ptr_t plg) {
  auto it = mStaticInputs.find(plg);
  if (it == mStaticInputs.end()) {
    mStaticInputs.insert(plg);
    mNumStaticInputs++;
  }
}
void ModuleData::addOutput(outplugdata_ptr_t plg) {
  auto it = mStaticOutputs.find(plg);
  if (it == mStaticOutputs.end()) {
    mStaticOutputs.insert(plg);
    mNumStaticOutputs++;
  }
}
void ModuleData::removeInput(inplugdata_ptr_t plg) {
  auto it = mStaticInputs.find(plg);
  if (it != mStaticInputs.end()) {
    mStaticInputs.erase(it);
    mNumStaticInputs--;
  }
}
void ModuleData::removeOutput(outplugdata_ptr_t plg) {
  auto it = mStaticOutputs.find(plg);
  if (it != mStaticOutputs.end()) {
    mStaticOutputs.erase(it);
    mNumStaticOutputs--;
  }
}
inplugdata_ptr_t ModuleData::staticInput(int idx) const {
  int size = mStaticInputs.size();
  auto it  = mStaticInputs.begin();
  for (int i = 0; i < idx; i++) {
    it++;
  }
  inplugdata_ptr_t rval = (it != mStaticInputs.end()) ? *it : nullptr;
  return rval;
}
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
bool ModuleData::isDirty() const {
  bool rval   = false;
  int inumout = this->numOutputs();
  for (int i = 0; i < inumout; i++) {
    outplugdata_ptr_t poutput = output(i);
    rval |= poutput->IsDirty();
  }
  if (false == rval) {
    int inumchi = numChildren();
    for (int ic = 0; ic < inumchi; ic++) {
      module* pchild = child(ic);
      rval |= pchild->isDirty();
    }
  }
  return rval;
}
inplugdata_ptr_t input(int idx) {
  return 0;
}
outplugdata_ptr_t output(int idx) {
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
inplugdata_ptr_t ModuleData::inputNamed(const PoolString& named) {
  int inuminp = GetNumInputs();
  for (int ip = 0; ip < inuminp; ip++) {
    inplugdata_ptr_t rval = input(ip);
    OrkAssert(rval != nullptr);
    if (named == rval->GetName()) {
      return rval;
    }
  }
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
outplugdata_ptr_t ModuleData::outputNamed(const PoolString& named) {
  int inumout = GetNumOutputs();
  printf("module<%p> numouts<%d>\n", (void*) this, inumout);
  for (int ip = 0; ip < inumout; ip++) {
    outplugdata_ptr_t rval = output(ip);
    OrkAssert(rval != nullptr);
    if (named == rval->GetName()) {
      return rval;
    }
  }
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
module* ModuleData::childNamed(const ork::PoolString& named) const {
  int inumchi = numChildren();
  for (int ic = 0; ic < inumchi; ic++) {
    module* rval = child(ic);
    if (named == rval->GetName()) {
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
DgModuleData::dgmodule()
    : mAffinity(dataflow::scheduler::CpuAffinity)
    , _parent(0)
    , mKey() {
}
///////////////////////////////////////////////////////////////////////////////
void DgModuleData::divideWork(const scheduler& sch, cluster* clus) {
  clus->AddModule(this);
  _doDivideWork(sch, clus);
}
///////////////////////////////////////////////////////////////////////////////
void DgModuleData::_doDivideWork(const scheduler& sch, cluster* clus) {
  workunit* wu = new workunit(this, clus, 0);
  wu->SetAffinity(GetAffinity());
  clus->AddWorkUnit(wu);
}
///////////////////////////////////////////////////////////////////////////////
void DgModuleData::releaseWorkUnit(workunit* wu) {
  OrkAssert(wu->GetModule() == this);
  delete wu;
}
bool DgModuleData::isGroup() const {
  return GetChildGraph() != 0;
}
graphdata_ptr_t DgModuleData::childGraph() const {
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////
