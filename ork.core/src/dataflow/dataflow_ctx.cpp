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
dgregister::dgregister(dgmoduleinst_ptr_t pmod, int idx)
    : mIndex(idx)
    , mpOwner(0)
    , mpBlock(nullptr) {
  bindModule(pmod);
}
///////////////////////////////////////////////////////////////////////////////
// prune no longer needed registers
///////////////////////////////////////////////////////////////////////////////
void dgcontext::prune(dgmoduleinst_ptr_t pmod) // we are done with pmod, prune registers associated with it
{
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
      bool b_is_probed    = (reg->mpOwner == _probemodule);

      if (b_didfeed_pmod && (false == b_is_probed)) {
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
void dgcontext::SetRegisters(const std::type_info* pinfo, dgregisterblock* pregs) {
  mRegisterSets[pinfo] = pregs;
}
//////////////////////////////////////////////////////////
dgregisterblock* dgcontext::GetRegisters(const std::type_info* pinfo) {
  auto it = mRegisterSets.find(pinfo);
  return (it == mRegisterSets.end()) ? 0 : it->second;
}
//////////////////////////////////////////////////////////
void dgcontext::Clear() {
  for (auto it : mRegisterSets) {
    dgregisterblock* pregs = it.second;
    pregs->Clear();
  }
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool dgqueue::IsPending(dgmoduleinst_ptr_t mod) {
  return (pending.find(mod) != pending.end());
}
//////////////////////////////////////////////////////////
int dgqueue::NumDownstream(dgmoduleinst_ptr_t mod) {
  int inumoutcon = 0;
  int inumouts   = mod->numOutputs();
  for (int io = 0; io < inumouts; io++) {
    outpluginst_ptr_t poutplug = mod->output(io);
    inumoutcon += (int)poutplug->numConnections();
  }
  return inumoutcon;
}
//////////////////////////////////////////////////////////
int dgqueue::NumPendingDownstream(dgmoduleinst_ptr_t mod) {
  int inumoutcon = 0;
  int inumouts   = mod->numOutputs();
  for (int io = 0; io < inumouts; io++) {
    outpluginst_ptr_t poutplug = mod->output(io);
    size_t inumcon             = poutplug->numConnections();
    for (size_t ic = 0; ic < inumcon; ic++) {
      auto pinplug        = poutplug->connected(ic);
      dgmoduleinst_ptr_t pconmod = std::dynamic_pointer_cast<DgModuleInst>(pinplug->_owning_module);
      inumoutcon += int(IsPending(pconmod));
    }
  }
  return inumoutcon;
}
//////////////////////////////////////////////////////////
void dgqueue::addModule(dgmoduleinst_ptr_t mod) {
  mod->_key.mDepth    = 0;
  mod->_key.mSerial   = -1;
  int inumo            = mod->numOutputs();
  mod->_key.mModifier = s8(-inumo);
  for (int io = 0; io < inumo; io++) {
    mod->output(io)->_register = nullptr;
  }
  pending.insert(mod);
}
//////////////////////////////////////////////////////////
void dgqueue::pruneRegisters(dgmoduleinst_ptr_t pmod) {
  mCompCtx.prune(pmod);
}
//////////////////////////////////////////////////////////
void dgqueue::QueModule(dgmoduleinst_ptr_t pmod, int irecd) {
  if (pending.find(pmod) != pending.end()) // is pmod pending ?
  {
    if (mModStack.size()) { // check the top of stack for registers to prune
      dgmoduleinst_ptr_t prev = mModStack.top();
      pruneRegisters(prev);
    }
    mModStack.push(pmod);
    ///////////////////////////////////
    pmod->_key.mSerial = s8(mSerial++);
    pending.erase(pmod);
    ///////////////////////////////////
    int inuminps = pmod->numInputs();
    int inumouts = pmod->numOutputs();
    ///////////////////////////////////
    // assign new registers
    ///////////////////////////////////
    int inumincon = 0;
    for (int ii = 0; ii < inuminps; ii++) {
      auto pinpplug = pmod->input(ii);
      inumincon += int(pinpplug->isConnected());
    }
    for (int io = 0; io < inumouts; io++) {
      outpluginst_ptr_t poutplug = pmod->output(io);
      if (poutplug->isConnected() || (inumincon != 0)) // if it has input or output connections
      {
        mCompCtx.alloc(poutplug);
      }
    }
    ///////////////////////////////////
    // add dependants to register
    ///////////////////////////////////
    for (int io = 0; io < inumouts; io++) {
      outpluginst_ptr_t poutplug = pmod->output(io);
      dgregister* preg      = poutplug->_register;
      if (preg) {
        size_t inumcon = poutplug->numConnections();
        for (size_t ic = 0; ic < inumcon; ic++) {
          auto pinp = poutplug->connected(ic);
          if (pinp && pinp->_owning_module != pmod) {
            dgmoduleinst_ptr_t dmod = std::dynamic_pointer_cast<DgModuleInst>(pinp->_owning_module);
            preg->_downstream_dependents.insert(dmod);
          }
        }
      }
    }
    ///////////////////////////////////
    // completed "pmod"
    //  add connected with no other pending deps
    ///////////////////////////////////
    for (int io = 0; io < inumouts; io++) {
      outpluginst_ptr_t poutplug = pmod->output(io);
      if (poutplug->_register) {
        size_t inumcon = poutplug->numConnections();
        for (size_t ic = 0; ic < inumcon; ic++) {
          auto pinp = poutplug->connected(ic);
          if (pinp && pinp->_owning_module != pmod) {
            dgmoduleinst_ptr_t dmod = std::dynamic_pointer_cast<DgModuleInst>(pinp->_owning_module);
            if (false == HasPendingInputs(dmod)) {
              QueModule(dmod, irecd + 1);
            }
          }
        }
      }
    }
    if (pending.size() != 0) {
      pruneRegisters(pmod);
    }
    ///////////////////////////////////
    mModStack.pop();
  }
}
//////////////////////////////////////////////////////////
void dgqueue::DumpOutputs(dgmoduleinst_ptr_t mod) const {
  int inump = mod->numOutputs();
  for (int ip = 0; ip < inump; ip++) {
    outpluginst_ptr_t poutplug = mod->output(ip);
    dgregister* preg           = poutplug->_register;
    dgregisterblock* pblk      = (preg != nullptr) ? preg->mpBlock : nullptr;
    std::string regb           = (pblk != nullptr) ? pblk->name() : "";
    int reg_index              = (preg != nullptr) ? preg->mIndex : -1;
    printf("  mod<%s> out<%d> reg<%s:%d>\n", mod->name().c_str(), ip, regb.c_str(), reg_index);
  }
}
void dgqueue::DumpInputs(dgmoduleinst_ptr_t mod) const {
  int inumins = mod->numInputs();
  for (int ip = 0; ip < inumins; ip++) {
    const auto pinplug = mod->input(ip);
    if (pinplug->connected()) {
      outpluginst_ptr_t poutplug = pinplug->connected();
      dgmoduleinst_ptr_t pconcon = std::dynamic_pointer_cast<DgModuleInst>(poutplug->_owning_module);
      dgregister* preg           = poutplug->_register;
      if (preg) {
        dgregisterblock* pblk = preg->mpBlock;
        std::string regb      = (pblk != nullptr) ? pblk->name() : "";
        int reg_index         = preg->mIndex;
        printf(
            "  mod<%s> inp<%d> -< module<%s> reg<%s:%d>\n",
            mod->name().c_str(),
            ip,
            pconcon->name().c_str(),
            regb.c_str(),
            reg_index);
      }
    }
  }
}
bool dgqueue::HasPendingInputs(dgmoduleinst_ptr_t mod) {
  bool bhaspending = false;
  int inumins      = mod->numInputs();
  for (int ip = 0; ip < inumins; ip++) {
    const auto pinplug = mod->input(ip);
    if (pinplug->connected()) {
      outpluginst_ptr_t pout                    = pinplug->connected();
      dgmoduleinst_ptr_t pconcon                = std::dynamic_pointer_cast<DgModuleInst>(pout->_owning_module);
      std::set<dgmoduleinst_ptr_t>::iterator it = pending.find(pconcon);
      if (pconcon == mod &&
          typeid(float) == pinplug->GetDataTypeId()) // connected to self and a float plug, must be an internal loop rate plug
      {                                              // pending.erase(it);
        // it = pending.end();
      } else if (it != pending.end()) {
        bhaspending = true;
      }
    }
  }
  return bhaspending;
}
//////////////////////////////////////////////////////////
dgqueue::dgqueue(graphinst_ptr_t pg, dgcontext& ctx)
    : mSerial(0)
    , mCompCtx(ctx) {
  /////////////////////////////////////////
  // add all modules
  /////////////////////////////////////////
  size_t inummodules = pg->numModules();
  for (size_t im = 0; im < inummodules; im++) {
    dgmoduleinst_ptr_t pmod = pg->module(im);
    addModule(pmod);
  }
  /////////////////////////////////////////
  // compute depths iteratively
  /////////////////////////////////////////
  int inumchg = -1;
  while (inumchg != 0) {
    inumchg = 0;
    for (size_t im = 0; im < inummodules; im++) {
      dgmoduleinst_ptr_t pmod = pg->module(im);
      int inumouts            = pmod->numOutputs();
      for (int op = 0; op < inumouts; op++) {
        outpluginst_ptr_t poutplug = pmod->output(op);
        size_t inumcon             = poutplug->numConnections();
        int ilo                    = 0;
        for (size_t ic = 0; ic < inumcon; ic++) {
          auto pin         = poutplug->connected(ic);
          dgmoduleinst_ptr_t pcon = std::dynamic_pointer_cast<DgModuleInst>(pin->_owning_module);
          int itd                 = pcon->_key.mDepth - 1;
          if (itd < ilo)
            ilo = itd;
        }
        if (pmod->_key.mDepth > ilo && ilo != 0) {
          pmod->_key.mDepth = s8(ilo);
          inumchg++;
        }
      }
      // printf( " mod<%s> comp_depth<%d>\n", pmod->GetName().c_str(), pmod->_key.mDepth );
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
