////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_eq.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_nonlin.h>
#include <ork/lev2/aud/singularity/alg_filters.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/dsp_ringmod.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::DspStageData, "SynDspStage");
ImplementReflectionX(ork::audio::singularity::IoConfig, "SynIoConfig");

namespace ork::audio::singularity {

//////////////////////////////////////////////////////////////////////////////

void IoConfig::describeX(class_t* clazz) {
  clazz->directVectorProperty("Inputs", &IoConfig::_inputs);
  clazz->directVectorProperty("Outputs", &IoConfig::_outputs);
}
//////////////////////////////////////////////////////////////////////////////
IoConfig::IoConfig() {
}
//////////////////////////////////////////////////////////////////////////////
ioconfig_ptr_t IoConfig::clone() const{
  auto rval = std::make_shared<IoConfig>();
  rval->_inputs = _inputs;
  rval->_outputs = _outputs;
  return rval;
}
//////////////////////////////////////////////////////////////////////////////
size_t IoConfig::numInputs() const {
  return _inputs.size();
}
size_t IoConfig::numOutputs() const {
  return _outputs.size();
}
///////////////////////////////////////////////////////////////////////////////

void DspStageData::describeX(class_t* clazz) {
  clazz->directProperty("Name", &DspStageData::_name);
  clazz->directProperty("StageIndex", &DspStageData::_stageIndex);
  clazz->directObjectProperty("IoConfig", &DspStageData::_ioconfig);
  clazz->directObjectMapProperty("DspBlocks", &DspStageData::_namedblockdatas);
}

///////////////////////////////////////////////////////////////////////////////

dspstagedata_ptr_t DspStageData::clone() const{
  auto rval = std::make_shared<DspStageData>();
  rval->_name = _name;
  rval->_stageIndex = _stageIndex;
  rval->_numblocks = _numblocks;
  rval->_ioconfig = _ioconfig->clone();
  for (size_t i=0; i<_numblocks; i++) {
    auto block = _blockdatas[i];
    auto clone = block->clone();
    rval->_blockdatas[i] = block; //clone;
    rval->_namedblockdatas[clone->_name] = block; //clone;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void DspStageData::dump() const {
  printf("DSPSTAGE<%s>\n", _name.c_str());
  printf(" IOCONFIG <%p>:\n", (void*)_ioconfig.get());
  printf("  inputs[ ");
  for (int i = 0; i < _ioconfig->_inputs.size(); i++) {
    printf("%d ", _ioconfig->_inputs[i]);
  }
  printf("]\n");
  printf("  outputs[ ");
  for (int i = 0; i < _ioconfig->_outputs.size(); i++) {
    printf("%d ", _ioconfig->_outputs[i]);
  }
  printf("]\n");
  printf(" BLOCKS[\n");
  int index = 0;
  for (int ib = 0; ib < _numblocks; ib++) {
    auto blockdata = _blockdatas[ib];
    if (blockdata) {
      printf("   %d: %s (bypass: %d)\n", index, blockdata->_name.c_str(), int(blockdata->_bypass) );
      for (int i = 0; i < blockdata->_numParams; i++) {
        auto param = blockdata->_paramd[i];
        if (param) {
          printf("    param %d: name<%s> : units<%s> evaluator<%s>\n", i, param->_name.c_str(), param->_units.c_str(), param->_evaluatorid.c_str() );
        } else {
          printf("    param %d: NULL!\n", i);
        }
      }
    } else {
      printf("   %d: NULL!\n", index);
    }
    index++;
  }
  printf("]\n");
}

///////////////////////////////////////////////////////////////////////////////

bool DspStageData::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) { // override
  for (auto item : _namedblockdatas) {
    auto blockdata     = item.second;
    int index          = blockdata->_blockIndex;
    _blockdatas[index] = blockdata;
  }
  _numblocks = _namedblockdatas.size();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

DspStageData::DspStageData() {
  _ioconfig = std::make_shared<IoConfig>();
}

///////////////////////////////////////////////////////////////////////////////

dspblkdata_ptr_t DspStageData::appendBlock() {
  OrkAssert(_numblocks < kmaxdspblocksperstage);
  auto blk                  = std::make_shared<DspBlockData>();
  _blockdatas[_numblocks++] = blk;
  return blk;
}

///////////////////////////////////////////////////////////////////////////////

void DspStageData::setNumIos(int numinp, int numout) {
  for (int i = 0; i < numinp; i++)
    _ioconfig->_inputs.push_back(i);
  for (int i = 0; i < numout; i++)
    _ioconfig->_outputs.push_back(i);
}

} // namespace ork::audio::singularity
