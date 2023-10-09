////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/math/cvector2.hpp>
#include <ork/math/cvector3.hpp>
#include <ork/math/cvector4.hpp>
#include <ork/math/quaternion.hpp>
#include <ork/math/cmatrix3.hpp>
#include <ork/math/cmatrix4.hpp>

#include <ork/dataflow/all.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////

ModuleInst::ModuleInst(const ModuleData* absdata, GraphInst* ginst)
    : _abstract_module_data(absdata)
    , _graphinst(ginst) {
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
inpluginst_ptr_t ModuleInst::inputNamed(const std::string& named) const {
  auto it = _inputsByName.find(named);
  if( it==_inputsByName.end() ){
    printf( "inputNamed<%s> not found!\n", named.c_str() );
    OrkAssert(false);
  }
  return it->second;
}
outpluginst_ptr_t ModuleInst::outputNamed(const std::string& named) const {
  auto it = _outputsByName.find(named);
  OrkAssert(it!=_outputsByName.end());
  return it->second;
}
///////////////////////////////////////////////////////////////////////////////
DgModuleInst::DgModuleInst(const DgModuleData* absdata, GraphInst* ginst)
  : ModuleInst(absdata,ginst)
  , _dgmodule_data(absdata) {
}
///////////////////////////////////////////////////////////////////////////////
DgModuleInst::~DgModuleInst(){

}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
