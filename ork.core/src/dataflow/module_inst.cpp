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
#include <ork/math/cvector2.hpp>
#include <ork/math/cvector3.hpp>
#include <ork/math/cvector4.hpp>
#include <ork/math/quaternion.hpp>
#include <ork/math/cmatrix3.hpp>
#include <ork/math/cmatrix4.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////

ModuleInst::ModuleInst(moduledata_constptr_t absdata)
    : _abstract_module_data(absdata) {
}
int ModuleInst::numOutputs() const {
  return _abstract_module_data->numOutputs();
}
void ModuleInst::_doSetInputDirty(inpluginst_ptr_t plg) {
}
void ModuleInst::_doSetOutputDirty(outpluginst_ptr_t plg) {
}
void ModuleInst::setInputDirty(inpluginst_ptr_t plg) {
  _doSetInputDirty(plg);
}
void ModuleInst::setOutputDirty(outpluginst_ptr_t plg) {
  _doSetOutputDirty(plg);
}
bool ModuleInst::isDirty(void) const {
  bool rval   = false;
  int inumout = this->numOutputs();
  for (int i = 0; i < inumout; i++) {
    rval |= output(i)->isDirty();
  }
  /*if (false == rval) {
    int inumchi = numChildren();
    for (int ic = 0; ic < inumchi; ic++) {
      module* pchild = child(ic);
      rval |= pchild->isDirty();
    }
  }*/
  return rval;
}
inpluginst_ptr_t ModuleInst::input(int idx) const{
  return mStaticInputs[idx];
}
outpluginst_ptr_t ModuleInst::output(int idx) const{
  return mStaticOutputs[idx];
}

///////////////////////////////////////////////////////////////////////////////
DgModuleInst::DgModuleInst(dgmoduledata_constptr_t absdata)
  : ModuleInst(absdata)
  , _dgmodule_data(absdata) {
}
///////////////////////////////////////////////////////////////////////////////
void DgModuleInst::divideWork(scheduler_ptr_t sch, cluster* clus) {
  //clus->AddModule(this);
  _doDivideWork(sch, clus);
}
///////////////////////////////////////////////////////////////////////////////
void DgModuleInst::_doDivideWork(scheduler_ptr_t sch, cluster* clus) {
  //workunit* wu = new workunit(this, clus, 0);
  //wu->SetAffinity(_dgmodule_data->GetAffinity());
  //clus->AddWorkUnit(wu);
}
///////////////////////////////////////////////////////////////////////////////
void DgModuleInst::releaseWorkUnit(workunit* wu) {
  //OrkAssert(wu->GetModule() == this);
  delete wu;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
