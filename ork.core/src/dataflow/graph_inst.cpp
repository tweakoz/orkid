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
    auto module_inst = data->createInstance();
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
      auto plug_inst = input->createInstance();
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
      auto plug_inst = output->createInstance();
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
  ////////////////////////////////////////////////
  // final link
  ////////////////////////////////////////////////
  for( int i=0; i<num_modules; i++ ){
    auto inst_module = _ordered_module_insts[i];
    inst_module->onLink(this);
  }
  ////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void GraphInst::compute(){
  for( auto item : _ordered_module_insts ){
    item->compute(this);
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
