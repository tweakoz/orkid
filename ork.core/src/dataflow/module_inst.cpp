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

ModuleInst::ModuleInst(const ModuleData* absdata)
    : _abstract_module_data(absdata) {
}
int ModuleInst::numInputs() const {
  return _inputs.size();
}
int ModuleInst::numOutputs() const {
  return _outputs.size();
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
  return _inputs[idx];
}
outpluginst_ptr_t ModuleInst::output(int idx) const{
  return _outputs[idx];
}
///////////////////////////////////////////////////////////////////////////////
DgModuleInst::DgModuleInst(const DgModuleData* absdata)
  : ModuleInst(absdata)
  , _dgmodule_data(absdata) {
}
///////////////////////////////////////////////////////////////////////////////
DgModuleInst::~DgModuleInst(){

}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
