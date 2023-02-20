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

///////////////////////////////////////////////////////////////////////////////
void ModuleData::SetInputDirty(inplugbase* plg) {
  DoSetInputDirty(plg);
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::SetOutputDirty(outplugbase* plg) {
  DoSetOutputDirty(plg);
}
///////////////////////////////////////////////////////////////////////////////
void ModuleData::AddDependency(outplugbase& pout, inplugbase& pin) {
  pin.ConnectInternal(&pout);
  // DepPlugSet::value_type v( & pin, & pout );
  // mDependencies.insert( v );
}
void ModuleData::AddInput(inplugbase* plg) {
  auto it = mStaticInputs.find(plg);
  if (it == mStaticInputs.end()) {
    mStaticInputs.insert(plg);
    mNumStaticInputs++;
  }
}
void ModuleData::AddOutput(outplugbase* plg) {
  auto it = mStaticOutputs.find(plg);
  if (it == mStaticOutputs.end()) {
    mStaticOutputs.insert(plg);
    mNumStaticOutputs++;
  }
}
void ModuleData::RemoveInput(inplugbase* plg) {
  auto it = mStaticInputs.find(plg);
  if (it != mStaticInputs.end()) {
    mStaticInputs.erase(it);
    mNumStaticInputs--;
  }
}
void ModuleData::RemoveOutput(outplugbase* plg) {
  auto it = mStaticOutputs.find(plg);
  if (it != mStaticOutputs.end()) {
    mStaticOutputs.erase(it);
    mNumStaticOutputs--;
  }
}
inplugbase* ModuleData::GetStaticInput(int idx) const {
  int size = mStaticInputs.size();
  auto it  = mStaticInputs.begin();
  for (int i = 0; i < idx; i++) {
    it++;
  }
  inplugbase* rval = (it != mStaticInputs.end()) ? *it : nullptr;
  return rval;
}
outplugbase* ModuleData::GetStaticOutput(int idx) const {
  int size = mStaticOutputs.size();
  auto it  = mStaticOutputs.begin();
  for (int i = 0; i < idx; i++) {
    it++;
  }
  outplugbase* rval = (it != mStaticOutputs.end()) ? *it : nullptr;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
bool ModuleData::IsDirty() const {
  bool rval   = false;
  int inumout = this->GetNumOutputs();
  for (int i = 0; i < inumout; i++) {
    outplugbase* poutput = GetOutput(i);
    rval |= poutput->IsDirty();
  }
  if (false == rval) {
    int inumchi = GetNumChildren();
    for (int ic = 0; ic < inumchi; ic++) {
      module* pchild = GetChild(ic);
      rval |= pchild->IsDirty();
    }
  }
  return rval;
}
inplugbase* GetInput(int idx) {
  return 0;
}
outplugbase* GetOutput(int idx) {
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
inplugbase* ModuleData::GetInputNamed(const PoolString& named) {
  int inuminp = GetNumInputs();
  for (int ip = 0; ip < inuminp; ip++) {
    inplugbase* rval = GetInput(ip);
    OrkAssert(rval != nullptr);
    if (named == rval->GetName()) {
      return rval;
    }
  }
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
outplugbase* ModuleData::GetOutputNamed(const PoolString& named) {
  int inumout = GetNumOutputs();
  printf("module<%p> numouts<%d>\n", (void*) this, inumout);
  for (int ip = 0; ip < inumout; ip++) {
    outplugbase* rval = GetOutput(ip);
    OrkAssert(rval != nullptr);
    if (named == rval->GetName()) {
      return rval;
    }
  }
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
module* ModuleData::GetChildNamed(const ork::PoolString& named) const {
  int inumchi = GetNumChildren();
  for (int ic = 0; ic < inumchi; ic++) {
    module* rval = GetChild(ic);
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

                inplugbase* pin = v.first;
                const outplugbase* pout = v.second;

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
void DgModuleData::DivideWork(const scheduler& sch, cluster* clus) {
  clus->AddModule(this);
  DoDivideWork(sch, clus);
}
///////////////////////////////////////////////////////////////////////////////
void DgModuleData::DoDivideWork(const scheduler& sch, cluster* clus) {
  workunit* wu = new workunit(this, clus, 0);
  wu->SetAffinity(GetAffinity());
  clus->AddWorkUnit(wu);
}
///////////////////////////////////////////////////////////////////////////////
void DgModuleData::ReleaseWorkUnit(workunit* wu) {
  OrkAssert(wu->GetModule() == this);
  delete wu;
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////
