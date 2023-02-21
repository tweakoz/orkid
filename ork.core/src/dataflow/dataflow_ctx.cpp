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
///////////////////////////////////////////////////////////////////////////////
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
dgregister* dgregisterblock::Alloc() {
  dgregister* reg = mBlock.allocate();
  OrkAssert(reg != 0);
  mAllocated.insert(reg);
  return reg;
}
///////////////////////////////////////////////////////////////////////////////
void dgregisterblock::Free(dgregister* preg) {
  preg->_downstream_dependents.clear();
  mBlock.deallocate(preg);
  mAllocated.erase(preg);
}
///////////////////////////////////////////////////////////////////////////////
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
void dgregister::bindPlug(plugdata_ptr_t plug) {
  if (plug) {
    _plug = plug;
    auto pmod = _plug->_parent_module;
    /////////////////////////////////////////
    // for each output,
    //  find any modules connected to this module
    //  and mark it as a dependency
    /////////////////////////////////////////

    int inumouts = pmod->numOutputs();
    for (int io = 0; io < inumouts; io++) {
      auto poutplug  = pmod->output(io);
      size_t inumcon = poutplug->numConnections();
      for (size_t ic = 0; ic < inumcon; ic++) {
        auto pconnected = poutplug->connected(ic);
        if (pconnected && pconnected->_parent_module != pmod) { // it is dependent on pmod
          auto childmod = std::dynamic_pointer_cast<DgModuleData>(pconnected->_parent_module);
          _downstream_dependents.insert(childmod);
        }
      }
    }
  }
}
//////////////////////////////////
dgregister::dgregister(plugdata_ptr_t plug, int idx)
    : mIndex(idx)
    , _plug(nullptr)
    , mpBlock(nullptr) {
  bindPlug(plug);
}

///////////////////////////////////////////////////////////////////////////////

std::string dgregister::name() const {
  return mpBlock->name() + FormatString("-%d", mIndex);
}

///////////////////////////////////////////////////////////////////////////////
// prune no longer needed registers
///////////////////////////////////////////////////////////////////////////////
orkvector<dgregister*> dgcontext::prune(dgmoduledata_ptr_t pmod) { // we are done with pmod, prune registers associated with it

  orkvector<dgregister*> deallocvec_accum;

  // check all register sets
  for (auto itc : _registerSets) {

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

      auto owner_module = typedModuleData<DgModuleData>(reg->_plug->_parent_module);

      if (b_didfeed_pmod and owner_module->_prunable) {
        reg->_downstream_dependents.erase(itfind);
      }
      if (0 == reg->_downstream_dependents.size()) {
        deallocvec.push_back(reg);
      }
    }
    for (dgregister* free_reg : deallocvec) {
      regs->Free(free_reg);
      deallocvec_accum.push_back(free_reg);
    }
  }
  return deallocvec_accum;
}
//////////////////////////////////////////////////////////
dgregister* dgcontext::alloc(outplugdata_ptr_t poutplug) {
  const std::type_info* tinfo = &poutplug->GetDataTypeId();
  auto itc                    = _registerSets.find(tinfo);
  if (itc != _registerSets.end()) {
    dgregisterblock* regs = itc->second;
    dgregister* preg      = regs->Alloc();
    preg->_plug = poutplug;
    return preg;
  }
  return nullptr;
}
//////////////////////////////////////////////////////////
void dgcontext::setRegisters(const std::type_info* pinfo, dgregisterblock* pregs) {
  _registerSets[pinfo] = pregs;
}
//////////////////////////////////////////////////////////
dgregisterblock* dgcontext::registers(const std::type_info* pinfo) {
  auto it = _registerSets.find(pinfo);
  return (it == _registerSets.end()) ? 0 : it->second;
}
//////////////////////////////////////////////////////////
void dgcontext::Clear() {
  for (auto it : _registerSets) {
    dgregisterblock* pregs = it.second;
    pregs->Clear();
  }
}
///////////////////////////////////////////////////////////////////////////////
void dgcontext::assignSchedulerToGraphInst(graphinst_ptr_t gi, scheduler_ptr_t sched) {
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
