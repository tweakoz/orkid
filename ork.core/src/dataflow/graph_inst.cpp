////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/dataflow/all.h>
#include <ork/kernel/datacache.h> // DataBlockCache (cook cache)

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace dataflow {
///////////////////////////////////////////////////////////////////////////////
GraphInst::GraphInst(graphdata_ptr_t gdata)
    : _graphdata(gdata)
    , _scheduler(nullptr)
    , _inProgress(false) {
}
///////////////////////////////////////////////////////////////////////////////
GraphInst::~GraphInst() {
}
///////////////////////////////////////////////////////////////////////////////
void GraphInst::clear() {
  _ordered_module_datas.clear();
  _ordered_module_insts.clear();
  _scheduler        = nullptr;
  _inProgress      = false;
}
///////////////////////////////////////////////////////////////////////////////
void GraphInst::updateTopology(topology_ptr_t topo){
  _ordered_module_datas = topo->_flattened;
  ////////////////////////////////////////////////
  // create module insts
  ////////////////////////////////////////////////
  for( auto data : _ordered_module_datas ){
    auto module_inst = data->createInstance(this);
    _ordered_module_insts.push_back(module_inst);
  }
  /////////////////////////////////////////////
  int num_modules = _ordered_module_datas.size();
  /////////////////////////////////////////////
  // instantiate input plugs
  /////////////////////////////////////////////
  std::map<PlugData*,int> input_indices;
  std::map<inpluginst_ptr_t,inplugdata_ptr_t> input2dataLUT;
  int input_index = 0;
  for( int i=0; i<num_modules; i++ ){
    auto data_module = _ordered_module_datas[i];
    auto inst_module = _ordered_module_insts[i];
    for( auto input : data_module->_inputs ){
      input_indices[input.get()] = input_index++;
      auto plug_inst = input->createInstance(inst_module.get());
      OrkAssert(plug_inst);
      inst_module->_inputs.push_back(plug_inst);
      inst_module->_inputsByName[input->_name] = plug_inst;
      input2dataLUT[plug_inst]=input;
    }
  }
  /////////////////////////////////////////////
  // instantiate output plugs
  /////////////////////////////////////////////
  std::map<PlugData*,int> output_indices;
  std::map<outpluginst_ptr_t,outplugdata_ptr_t> output2dataLUT;
  std::map<outplugdata_ptr_t,outpluginst_ptr_t> data2outputLUT;
  int output_index = 0;
  for( int i=0; i<num_modules; i++ ){
    auto data_module = _ordered_module_datas[i];
    auto inst_module = _ordered_module_insts[i];
    for( auto output : data_module->_outputs ){
      output_indices[output.get()] = output_index++;
      auto plug_inst = output->createInstance(inst_module.get());
      OrkAssert(plug_inst);
      inst_module->_outputs.push_back(plug_inst);
      inst_module->_outputsByName[output->_name] = plug_inst;
      output2dataLUT[plug_inst]=output;
      data2outputLUT[output]=plug_inst;
    }
  }
  /////////////////////////////////////////////
  // link plug instances
  /////////////////////////////////////////////
  for( int i=0; i<num_modules; i++ ){
    auto data_module = _ordered_module_datas[i];
    auto inst_module = _ordered_module_insts[i];
    auto datamodule_name = data_module->_name;
    _module_inst_map[datamodule_name] = inst_module;
    for( auto input_inst : inst_module->_inputs ){
      auto it_lut = input2dataLUT.find(input_inst);
      OrkAssert(it_lut!=input2dataLUT.end());
      auto input_data = it_lut->second;
      auto connected = input_data->_connectedOutput;
      if(connected){

        auto it_con = data2outputLUT.find(connected);
        OrkAssert(it_con!=data2outputLUT.end());

        auto output_inst = it_con->second;
        input_inst->_connectedOutput = output_inst;
      }
    }
  }
  link();
  stage();
  activate();
}
///////////////////////////////////////////////////////////////////////////////
void GraphInst::link(){
  for( auto item : _ordered_module_insts ){
    item->onLink(this);
  }
}
///////////////////////////////////////////////////////////////////////////////
void GraphInst::stage(){
  for( auto item : _ordered_module_insts ){
    item->onStage(this);
  }
}
///////////////////////////////////////////////////////////////////////////////
void GraphInst::activate(){
  for( auto item : _ordered_module_insts ){
    item->onActivate(this);
  }
}
///////////////////////////////////////////////////////////////////////////////
void GraphInst::compute(ui::updatedata_ptr_t updata){
  if (_graphdata and _graphdata->_cacheable) {
    cachedCompute(updata);
    return;
  }
  if (_moduleTimingSink) { // per-module host-wall timing (per-CLASS perf counters; see the sink decl)
    for (auto item : _ordered_module_insts) {
      auto t0 = std::chrono::steady_clock::now();
      item->compute(this, updata);
      double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      _moduleTimingSink(item->_dgmodule_data->GetClass()->Name().c_str(), dt);
    }
    return;
  }
  for( auto item : _ordered_module_insts ){
    item->compute(this,updata);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Merkle-hash every node into its _cookHash. The per-class virtual
// cookComputeHash(input_hashes, context) builds the hash from a version salt +
// the node's own scalar params + the upstream node hashes — cheap, deterministic
// (no serialized UUIDs), and never touches an output buffer.
///////////////////////////////////////////////////////////////////////////////
void GraphInst::computeNodeHashes() {
  std::unordered_map<const void*, uint64_t> outhash; // outpluginst* -> producer node hash
  size_t n = _ordered_module_insts.size();
  for (size_t i = 0; i < n; i++) {
    auto inst = _ordered_module_insts[i];

    std::vector<uint64_t> input_hashes;
    input_hashes.reserve(inst->_inputs.size());
    for (auto inp : inst->_inputs) {
      uint64_t uh = 0;
      if (inp->_connectedOutput) {
        auto it = outhash.find(inp->_connectedOutput.get());
        if (it != outhash.end())
          uh = it->second;
      }
      input_hashes.push_back(uh);
    }
    inst->_cookHash = inst->cookComputeHash(input_hashes, _cookContextHash);
    for (auto outp : inst->_outputs)
      outhash[outp.get()] = inst->_cookHash;
  }
}

///////////////////////////////////////////////////////////////////////////////
// synchronous cook-cache compute: hash all nodes, then per node load-from-cache
// (skip compute) or compute+store inline. Valid only when compute() produces its
// output synchronously — GPU graphs sync per op in their own driver instead.
///////////////////////////////////////////////////////////////////////////////
void GraphInst::cachedCompute(ui::updatedata_ptr_t updata) {
  computeNodeHashes();
  for (auto inst : _ordered_module_insts) {
    // dataflow cook datablocks go in their own evictable namespace (see
    // DataBlockCache::evictToSize / <staging>/dflowcache), kept separate from
    // the shared dblockcache so they can be size-capped independently.
    auto db = DataBlockCache::findDataBlock("dflowcache", inst->_cookHash);
    if (db and inst->cookLoad(db)) {
      // cache hit — output restored; skip compute()
    } else {
      inst->compute(this, updata);
      if (auto store = inst->cookStore())
        DataBlockCache::setDataBlock("dflowcache", inst->_cookHash, store);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void GraphInst::reset(){
  for( auto item : _ordered_module_insts ){
    item->onReset(this);
  }
}
///////////////////////////////////////////////////////////////////////////////
bool GraphInst::isDirty(void) const {
  return false;
}
///////////////////////////////////////////////////////////////////////////////
bool GraphInst::isPending() const {
  return _inProgress;
}
///////////////////////////////////////////////////////////////////////////////
void GraphInst::setPending(bool bv) {
  _inProgress = bv;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
