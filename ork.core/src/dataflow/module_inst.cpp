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

ModuleInst::ModuleInst(moduledata_constptr_t absdata) : _abstract_module_data(absdata) {}

void ModuleInst::DoSetInputDirty(inplugdata_ptr_t plg) {
  }
void ModuleInst::DoSetOutputDirty(outplugdata_ptr_t plg) {
  }
  int ModuleInst::numOutputs() const {
    return _abstract_module_data->numOutputs();
  }
  void ModuleInst::SetInputDirty(inplugdata_ptr_t plg){

  }
  void ModuleInst::SetOutputDirty(outplugdata_ptr_t plg){

  }
bool ModuleInst::isDirty(void) const{

  }

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////
