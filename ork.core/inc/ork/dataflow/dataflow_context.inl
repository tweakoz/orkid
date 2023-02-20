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
#include <ork/reflect/properties/register.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////
template <typename T>
dgregisterblock::dgregisterblock(const std::string& name, int isize)
    : mBlock(isize)
    , mName(name) {
  for (int io = 0; io < isize; io++) {
    mBlock.direct_access(io).mIndex = (isize - 1) - io;
    mBlock.direct_access(io)._downstream_dependents.clear();
    mBlock.direct_access(io).mpBlock = this;
  }
}
///////////////////////////////////////////////////////////////////////////////
template <typename T>
dgregister* dgregisterblock::Alloc() {
  dgregister* reg = mBlock.allocate();
  OrkAssert(reg != 0);
  mAllocated.insert(reg);
  return reg;
}
///////////////////////////////////////////////////////////////////////////////
template <typename T>
void dgregisterblock::Free(dgregister* preg) {
  preg->_downstream_dependents.clear();
  mBlock.deallocate(preg);
  mAllocated.erase(preg);
}
///////////////////////////////////////////////////////////////////////////////
template <typename T>
void dgregisterblock::Clear() {
  orkvector<dgregister*> deallocvec;

  for (dgregister* reg : mAllocated)
    deallocvec.push_back(reg);

  for (dgregister* reg : deallocvec)
    Free(reg);
}
///////////////////////////////////////////////////////////////////////////////
// bind a module to a register
///////////////////////////////////////////////////////////////////////////////
template <typename T>
void dgregister::bindModule(dgmoduleinst_ptr_t pmod) {
  if (pmod) {

    mpOwner = pmod;

    /////////////////////////////////////////
    // for each output,
    //  find any modules connected to this module
    //  and mark it as a dependency
    /////////////////////////////////////////

    int inumouts = pmod->numOutputs();
    for (int io = 0; io < inumouts; io++) {
      outpluginst_ptr_t poutplug = pmod->output(io);
      size_t inumcon             = poutplug->numConnections();
      for (size_t ic = 0; ic < inumcon; ic++) {
        auto pconnected = poutplug->connected(ic);
        if (pconnected && pconnected->_owning_module != pmod) { // it is dependent on pmod
          dgmoduleinst_ptr_t childmod = std::dynamic_pointer_cast<DgModuleInst>(pconnected->_owning_module);
          _downstream_dependents.insert(childmod);
        }
      }
    }
  }
}
//////////////////////////////////
template <typename T>
dgregister::dgregister(dgmoduleinst_ptr_t pmod, int idx)
    : mIndex(idx)
    , mpOwner(0)
    , mpBlock(nullptr) {
  bindModule(pmod);
}
///////////////////////////////////////////////////////////////////////////////
// prune no longer needed registers
///////////////////////////////////////////////////////////////////////////////
template <typename T>
void dgcontext::prune(dgmoduleinst_ptr_t pmod) { // we are done with pmod, prune registers associated with it

  // check all register sets
  for (auto itc : mRegisterSets) {

    dgregisterblock* regs                = itc.second;
    const orkset<dgregister*>& allocated = regs->Allocated();
    orkvector<dgregister*> deallocvec;

    // check all allocated registers

    for (auto reg : allocated) {

      auto itfind = reg->_downstream_dependents.find(pmod);

      // were any allocated registers feeding this module?
      // is it also not a probed module ?
      //  if so, they can be pruned!!!

      bool b_didfeed_pmod = (itfind != reg->_downstream_dependents.end());

      if (b_didfeed_pmod and reg->mpOwner->_prunable) {
        reg->_downstream_dependents.erase(itfind);
      }
      if (0 == reg->_downstream_dependents.size()) {
        deallocvec.push_back(reg);
      }
    }
    for (dgregister* free_reg : deallocvec) {
      regs->Free(free_reg);
    }
  }
}
//////////////////////////////////////////////////////////
template <typename T>
void dgcontext::alloc(outpluginst_ptr_t poutplug) {
  const std::type_info* tinfo = &poutplug->GetDataTypeId();
  auto itc                    = mRegisterSets.find(tinfo);
  if (itc != mRegisterSets.end()) {
    dgregisterblock* regs = itc->second;
    dgregister* preg      = regs->Alloc();
    preg->mpOwner         = std::dynamic_pointer_cast<DgModuleInst>(poutplug->_owning_module);
    poutplug->_register = preg;
  }
}
//////////////////////////////////////////////////////////
template <typename T>
void dgcontext::SetRegisters(const std::type_info* pinfo, dgregisterblock* pregs) {
  mRegisterSets[pinfo] = pregs;
}
//////////////////////////////////////////////////////////
dgregisterblock* dgcontext::GetRegisters(const std::type_info* pinfo) {
  auto it = mRegisterSets.find(pinfo);
  return (it == mRegisterSets.end()) ? 0 : it->second;
}
//////////////////////////////////////////////////////////
template <typename T>
void dgcontext::Clear() {
  for (auto it : mRegisterSets) {
    dgregisterblock* pregs = it.second;
    pregs->Clear();
  }
}
///////////////////////////////////////////////////////////////////////////////
template <typename T>
void dgcontext::assignSchedulerToGraphInst(graphinst_ptr_t gi, scheduler_ptr_t sched){
  if (gi->_scheduler) {
    sched->RemoveGraph(gi);
  }
  if (sched) {
    sched->AddGraph(gi);
  } 
  gi->_scheduler = sched;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
