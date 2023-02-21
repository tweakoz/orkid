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

DgSorter::DgSorter(const GraphData* pg, dgcontext_ptr_t ctx)
    : _dgcontext(ctx)
    , _graphdata(pg)
    , _serial(NOSERIAL) {

  /////////////////////////////////////////
  // add all modules
  /////////////////////////////////////////

  size_t inummodules = pg->numModules();
  for (size_t im = 0; im < inummodules; im++) {
    dgmoduledata_ptr_t pmod = pg->module(im);
    addModule(pmod);
  }

  /////////////////////////////////////////
  // compute depths iteratively
  /////////////////////////////////////////

  int inumchg = -1;
  while (inumchg != 0) {
    inumchg = 0;
    for (size_t im = 0; im < inummodules; im++) {
      dgmoduledata_ptr_t pmod = pg->module(im);

      auto& node_info = _nodeinfomap[pmod];

      int inumouts            = pmod->numOutputs();
      for (int op = 0; op < inumouts; op++) {
        auto poutplug = pmod->output(op);
        size_t inumcon             = poutplug->numConnections();
        int ilo                    = 0;
        for (size_t ic = 0; ic < inumcon; ic++) {
          auto pin  = poutplug->connected(ic);
          auto pcon = typedModuleData<DgModuleData>(pin->_parent_module);
          int itd   = node_info.mDepth - 1;
          if (itd < ilo)
            ilo = itd;
        }
        if (node_info.mDepth > ilo && ilo != 0) {
          node_info.mDepth = s8(ilo);
          inumchg++;
        }
      }
      // printf( " mod<%s> comp_depth<%d>\n", pmod->GetName().c_str(), pmod->_key.mDepth );
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

bool DgSorter::isPending(dgmoduledata_ptr_t mod) const {
  return (_pending.find(mod) != _pending.end());
}

//////////////////////////////////////////////////////////

size_t DgSorter::numPending() const {
  return _pending.size();
}

//////////////////////////////////////////////////////////

int DgSorter::numDownstream(dgmoduledata_ptr_t mod) const {
  int inumoutcon = 0;
  int inumouts   = mod->numOutputs();
  for (int io = 0; io < inumouts; io++) {
    outplugdata_ptr_t poutplug = mod->output(io);
    inumoutcon += (int)poutplug->numConnections();
  }
  return inumoutcon;
}

//////////////////////////////////////////////////////////

int DgSorter::numPendingDownstream(dgmoduledata_ptr_t mod) const {
  int inumoutcon = 0;
  int inumouts   = mod->numOutputs();
  for (int io = 0; io < inumouts; io++) {
    outplugdata_ptr_t poutplug = mod->output(io);
    size_t inumcon             = poutplug->numConnections();
    for (size_t ic = 0; ic < inumcon; ic++) {
      auto pinplug               = poutplug->connected(ic);
      dgmoduledata_ptr_t pconmod = std::dynamic_pointer_cast<DgModuleData>(pinplug->_parent_module);
      inumoutcon += int(isPending(pconmod));
    }
  }
  return inumoutcon;
}

//////////////////////////////////////////////////////////

void DgSorter::addModule(dgmoduledata_ptr_t mod) {

  auto& node_info = _nodeinfomap[mod];
  node_info.mDepth    = 0;
  node_info._serial   = NOSERIAL;
  int inumo           = mod->numOutputs();
  node_info.mModifier = s8(-inumo);
  for (int io = 0; io < inumo; io++) {
    auto plug_out = mod->output(io);
    auto& plug_info = _pluginfomap[plug_out];
    plug_info._register = nullptr;
  }
  _pending.insert(mod);
}

//////////////////////////////////////////////////////////

void DgSorter::pruneRegisters(dgmoduledata_ptr_t pmod) {
  _dgcontext->prune(pmod);
}

//////////////////////////////////////////////////////////

bool DgSorter::hasPendingInputs(dgmoduledata_ptr_t mod) const {
  bool bhaspending = false;
  int inumins      = mod->numInputs();
  for (int ip = 0; ip < inumins; ip++) {
    auto pinplug = mod->input(ip);
    if (pinplug->_connectedOutput) {
      auto pout    = pinplug->_connectedOutput;
      auto pconcon = typedModuleData<DgModuleData>(pout->_parent_module);
      auto it      = _pending.find(pconcon);
      if (pconcon == mod &&
          typeid(float) == pinplug->GetDataTypeId()) // connected to self and a float plug, must be an internal loop rate plug
      {                                              // pending.erase(it);
        // it = pending.end();
      } else if (it != _pending.end()) {
        bhaspending = true;
      }
    }
  }
  return bhaspending;
}

//////////////////////////////////////////////////////////
void DgSorter::enqueueModule(dgmoduledata_ptr_t pmod, int irecd) {
  if (_pending.find(pmod) != _pending.end()) { // is pmod pending ?

    if (_modulestack.size()) { // check the top of stack for registers to prune
      dgmoduledata_ptr_t prev = _modulestack.top();
      pruneRegisters(prev);
    }
    _modulestack.push(pmod);

    ///////////////////////////////////
    auto& node_info = _nodeinfomap[pmod];
    node_info._serial = ++_serial;
    _pending.erase(pmod);

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
      auto poutplug = pmod->output(io);
      if (poutplug->isConnected() || (inumincon != 0)) { // if it has input or output connections
        auto& plug_info = _pluginfomap[poutplug];
        plug_info._register = _dgcontext->alloc(poutplug);
      }
    }

    ///////////////////////////////////
    // add dependents to register
    ///////////////////////////////////

    for (int io = 0; io < inumouts; io++) {
      outplugdata_ptr_t poutplug = pmod->output(io);
      auto& plug_info = _pluginfomap[poutplug];
      auto preg       = plug_info._register;
      if (preg) {
        size_t inumcon = poutplug->numConnections();
        for (size_t ic = 0; ic < inumcon; ic++) {
          auto pinp = poutplug->connected(ic);
          if (pinp && pinp->_parent_module != pmod) {
            dgmoduledata_ptr_t dmod = typedModuleData<DgModuleData>(pinp->_parent_module);
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
      outplugdata_ptr_t poutplug = pmod->output(io);
      auto& plug_info = _pluginfomap[poutplug];
      if (plug_info._register) {
        size_t inumcon = poutplug->numConnections();
        for (size_t ic = 0; ic < inumcon; ic++) {
          auto pinp = poutplug->connected(ic);
          if (pinp && pinp->_parent_module != pmod) {
            dgmoduledata_ptr_t dmod = typedModuleData<DgModuleData>(pinp->_parent_module);
            if (false == hasPendingInputs(dmod)) {
              enqueueModule(dmod, irecd + 1);
            }
          }
        }
      }
    }
    if (_pending.size() != 0) {
      pruneRegisters(pmod);
    }
    ///////////////////////////////////
    _modulestack.pop();
  }
}

//////////////////////////////////////////////////////////

void DgSorter::dumpOutputs(dgmoduledata_ptr_t mod) const {
  int inump = mod->numOutputs();
  for (int ip = 0; ip < inump; ip++) {
    outplugdata_ptr_t poutplug = mod->output(ip);
    auto it_plug_info = _pluginfomap.find(poutplug);
    OrkAssert(it_plug_info!=_pluginfomap.end());
    auto& plug_info = it_plug_info->second;
    dgregister* preg           = plug_info._register;
    dgregisterblock* pblk      = (preg != nullptr) ? preg->mpBlock : nullptr;
    std::string regb           = (pblk != nullptr) ? pblk->name() : "";
    int reg_index              = (preg != nullptr) ? preg->mIndex : -1;
    printf("  mod<%s> out<%d> reg<%s:%d>\n", mod->_name.c_str(), ip, regb.c_str(), reg_index);
  }
}

//////////////////////////////////////////////////////////

void DgSorter::dumpInputs(dgmoduledata_ptr_t mod) const {
  int inumins = mod->numInputs();
  for (int ip = 0; ip < inumins; ip++) {
    const auto pinplug = mod->input(ip);
    if (pinplug->_connectedOutput) {
      auto poutplug = pinplug->_connectedOutput;
      auto pconcon  = typedModuleData<DgModuleData>(poutplug->_parent_module);
      auto it_plug_info = _pluginfomap.find(poutplug);
      auto& plug_info = it_plug_info->second;
      OrkAssert(it_plug_info!=_pluginfomap.end());
      dgregister* preg           = plug_info._register;
      if (preg) {
        dgregisterblock* pblk = preg->mpBlock;
        std::string regb      = (pblk != nullptr) ? pblk->name() : "";
        int reg_index         = preg->mIndex;
        printf(
            "  mod<%s> inp<%d> -< module<%s> reg<%s:%d>\n",
            mod->_name.c_str(),
            ip,
            pconcon->_name.c_str(),
            regb.c_str(),
            reg_index);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
topology_ptr_t DgSorter::generateTopology(dgcontext_ptr_t ctx) {

  if( not _graphdata->isComplete() ){
    return nullptr;
  }

  auto new_topo = std::make_shared<Topology>();

  using dgmodlut_t = std::multimap<int, dgmoduledata_ptr_t>;
    
    while (this->numPending()) {
      dgmodlut_t pending_and_ready;

      for (dgmoduledata_ptr_t pmod : this->_pending) {
        if (not hasPendingInputs(pmod)) {

          auto it_node_info = _nodeinfomap.find(pmod);

          const auto& node_info = it_node_info->second;

          int ikey = (node_info.mDepth * 16) + node_info.mModifier;

          pending_and_ready.insert(std::make_pair(ikey, pmod));
        }
      }

      for (const auto& next : pending_and_ready) {
        this->enqueueModule(next.second, 0);
      }
      ///////////////////////////////////////
    }
    ///////////////////////////////////////
    // SORT into flattened
    ///////////////////////////////////////
    size_t num_modules = _graphdata->numModules();
    std::multimap<size_t,dgmoduledata_ptr_t> sorted;
    for (size_t ic = 0; ic < num_modules; ic++) {
      auto module = _graphdata->module(ic);
      auto it_node_info = _nodeinfomap.find(module);
      const auto& node_info = it_node_info->second;
      size_t iserial = node_info._serial;
      sorted.insert(std::make_pair(iserial, module));
    }
    for( auto item : sorted ){
      new_topo->_flattened.push_back(item.second);
    }
    ///////////////////////////////////////
    ///////////////////////////////////////
  
  return new_topo;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
