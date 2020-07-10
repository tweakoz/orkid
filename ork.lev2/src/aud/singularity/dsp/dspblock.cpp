////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

ImplementReflectionX(ork::audio::singularity::DspBlockData, "SynDspBlock");
ImplementReflectionX(ork::audio::singularity::DspStageData, "SynDspStage");
ImplementReflectionX(ork::audio::singularity::IoMask, "SynIoMask");

namespace ork::audio::singularity {

//////////////////////////////////////////////////////////////////////////////

void IoMask::describeX(class_t* clazz) {
  clazz->directVectorProperty("Inputs", &IoMask::_inputs);
  clazz->directVectorProperty("Outputs", &IoMask::_outputs);
}
//////////////////////////////////////////////////////////////////////////////
IoMask::IoMask() {
}
//////////////////////////////////////////////////////////////////////////////
size_t IoMask::numInputs() const {
  return _inputs.size();
}
size_t IoMask::numOutputs() const {
  return _outputs.size();
}
//////////////////////////////////////////////////////////////////////////////

void DspBlockData::describeX(class_t* clazz) {
  clazz->directProperty("Name", &DspBlockData::_name);
  clazz->directProperty("BlockIndex", &DspBlockData::_blockIndex);
  clazz->directProperty("InputPad", &DspBlockData::_inputPad);
  clazz->directVectorProperty("Params", &DspBlockData::_paramd);
  clazz->directVectorProperty("DspChannels", &DspBlockData::_dspchannels);
}

//////////////////////////////////////////////////////////////////////////////

DspBlockData::DspBlockData(std::string name)
    : _name(name) {
}

int DspBlockData::addDspChannel(int channel) {
  int chanindex = _dspchannels.size();
  _dspchannels.push_back(channel);
  return chanindex;
}

//////////////////////////////////////////////////////////////////////////////

bool DspBlockData::postDeserialize(reflect::serdes::IDeserializer&) { // override
  _numParams = _paramd.size();
  return true;
}

//////////////////////////////////////////////////////////////////////////////

scopesource_ptr_t DspBlockData::createScopeSource() {
  auto src     = std::make_shared<ScopeSource>();
  _scopesource = src;
  return src;
}

///////////////////////////////////////////////////////////////////////////////

dspparam_ptr_t DspBlockData::addParam() {
  OrkAssert(_numParams < kmaxparmperblock - 1);
  auto param = std::make_shared<DspParamData>();
  _paramd.push_back(param);
  _numParams = _paramd.size();
  return param;
}

dspparam_ptr_t DspBlockData::param(int index) {
  return _paramd[index];
}

///////////////////////////////////////////////////////////////////////////////

DspBuffer::DspBuffer()
    : _maxframes(16384)
    , _numframes(0) {
  for (int i = 0; i < kmaxdspblocksperstage; i++) {
    _channels[i].resize(_maxframes);
  }
}

///////////////////////////////////////////////////////////////////////////////

void DspBuffer::resize(int inumframes) {
  if (inumframes > _maxframes) {
    for (int i = 0; i < kmaxdspblocksperstage; i++) {
      _channels[i].resize(inumframes);
    }
    _maxframes = inumframes;
  }
  _numframes = inumframes;
}

///////////////////////////////////////////////////////////////////////////////

float* DspBuffer::channel(int ich) {
  ich = ich % kmaxdspblocksperstage;
  return _channels[ich].data();
}

///////////////////////////////////////////////////////////////////////////////

void DspStageData::describeX(class_t* clazz) {
  clazz->directProperty("Name", &DspStageData::_name);
  clazz->directProperty("StageIndex", &DspStageData::_stageIndex);
  clazz->directObjectProperty("IoMask", &DspStageData::_iomask);
  clazz->directObjectMapProperty("DspBlocks", &DspStageData::_namedblockdatas);
}

///////////////////////////////////////////////////////////////////////////////

bool DspStageData::postDeserialize(reflect::serdes::IDeserializer&) { // override
  for (auto item : _namedblockdatas) {
    auto blockdata     = item.second;
    int index          = blockdata->_blockIndex;
    _blockdatas[index] = blockdata;
  }
  _numblocks = _namedblockdatas.size();
}

///////////////////////////////////////////////////////////////////////////////

DspStageData::DspStageData() {
  _iomask = std::make_shared<IoMask>();
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
    _iomask->_inputs.push_back(i);
  for (int i = 0; i < numout; i++)
    _iomask->_outputs.push_back(i);
}

///////////////////////////////////////////////////////////////////////////////

DspBlock::DspBlock(const DspBlockData* dbd)
    : _dbd(dbd)
    , _numParams(dbd->_numParams) {
  int numchannels = dbd->_dspchannels.size();
  for (int i = 0; i < numchannels; i++)
    _dspchannel[i] = dbd->_dspchannels[i];
  for (int i = numchannels; i < kmaxdspblocksperstage; i++) {
    _dspchannel[i] = i;
  }
}

///////////////////////////////////////////////////////////////////////////////

DspParam DspBlock::initDspParam(dspparam_constptr_t dpd) {
  DspParam rval;
  rval._data      = dpd;
  rval._C1        = _layer->getSRC1(dpd->_mods);
  rval._C2        = _layer->getSRC2(dpd->_mods);
  rval._evaluator = dpd->_mods->_evaluator;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void DspBlock::keyOn(const KeyOnInfo& koi) {
  _layer = koi._layer;
  // HERE
  for (int i = 0; i < _numParams; i++) {
    _param[i] = initDspParam(_dbd->_paramd[i]);
    _param[i].keyOn(koi._key, koi._vel);
  }
  doKeyOn(koi);
}

///////////////////////////////////////////////////////////////////////////////

const float* DspBlock::getInpBuf(DspBuffer& dspbuf, int index) {
  int chan   = _dspchannel[index];
  int inpidx = _iomask->_inputs[chan];
  return dspbuf.channel(inpidx);
}

///////////////////////////////////////////////////////////////////////////////

float* DspBlock::getOutBuf(DspBuffer& dspbuf, int index) {
  int chan   = _dspchannel[index];
  int outidx = _iomask->_outputs[chan];
  return dspbuf.channel(outidx);
}

///////////////////////////////////////////////////////////////////////////////

int DspBlock::numOutputs() const {
  return _iomask->numOutputs();
}

///////////////////////////////////////////////////////////////////////////////

int DspBlock::numInputs() const {
  return _iomask->numInputs();
}

///////////////////////////////////////////////////////////////////////////////
NOPDATA::NOPDATA() {
  _blocktype = "NOP";
}
dspblk_ptr_t NOPDATA::createInstance() const { // override
  return std::make_shared<NOP>(this);
}

NOP::NOP(const DspBlockData* dbd)
    : DspBlock(dbd) {
  _numParams = 0;
}
void NOP::compute(DspBuffer& dspbuf) { // override
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t createDspBlock(const DspBlockData* dbd) {

  dspblk_ptr_t rval = dbd->createInstance();
  if (rval)
    return rval;
  /*
  ////////////////////////
  // amp/mix
  ////////////////////////

  if (dbd->_blocktype == "XFADE")
    rval = std::make_shared<XFADE>(dbd);
  if (dbd->_blocktype == "x GAIN")
    rval = std::make_shared<XGAIN>(dbd);
  if (dbd->_blocktype == "GAIN")
    rval = std::make_shared<GAIN>(dbd);
  if (dbd->_blocktype == "AMP_MONOIO")
    rval = std::make_shared<AMP_MONOIO>(dbd);
  if (dbd->_blocktype == "+ AMP")
    rval = std::make_shared<PLUSAMP>(dbd);
  if (dbd->_blocktype == "x AMP")
    rval = std::make_shared<XAMP>(dbd);
  if (dbd->_blocktype == "PANNER")
    rval = std::make_shared<PANNER>(dbd);
  if (dbd->_blocktype == "AMP U   AMP L")
    rval = std::make_shared<AMPU_AMPL>(dbd);
  if (dbd->_blocktype == "! AMP")
    rval = std::make_shared<BANGAMP>(dbd);

  ////////////////////////
  // osc/gen
  ////////////////////////

  if (dbd->_blocktype == "SAMPLER")
    rval = std::make_shared<SAMPLER>(dbd);
  if (dbd->_blocktype == "SINE")
    rval = std::make_shared<SINE>(dbd);
  if (dbd->_blocktype == "LF SIN")
    rval = std::make_shared<SINE>(dbd);
  if (dbd->_blocktype == "SAW")
    rval = std::make_shared<SAW>(dbd);
  if (dbd->_blocktype == "SQUARE")
    rval = std::make_shared<SQUARE>(dbd);
  if (dbd->_blocktype == "SINE+")
    rval = std::make_shared<SINEPLUS>(dbd);
  if (dbd->_blocktype == "SAW+")
    rval = std::make_shared<SAWPLUS>(dbd);
  if (dbd->_blocktype == "SW+SHP")
    rval = std::make_shared<SWPLUSSHP>(dbd);
  if (dbd->_blocktype == "+ SHAPEMOD OSC")
    rval = std::make_shared<PLUSSHAPEMODOSC>(dbd);
  if (dbd->_blocktype == "SHAPE MOD OSC")
    rval = std::make_shared<SHAPEMODOSC>(dbd);

  if (dbd->_blocktype == "SYNC M")
    rval = std::make_shared<SYNCM>(dbd);
  if (dbd->_blocktype == "SYNC S")
    rval = std::make_shared<SYNCS>(dbd);
  if (dbd->_blocktype == "PWM")
    rval = std::make_shared<PWM>(dbd);

  if (dbd->_blocktype == "NOISE")
    rval = std::make_shared<NOISE>(dbd);

  ////////////////////////
  // EQ
  ////////////////////////

  if (dbd->_blocktype == "PARA BASS")
    rval = std::make_shared<PARABASS>(dbd);
  if (dbd->_blocktype == "PARA MID")
    rval = std::make_shared<PARAMID>(dbd);
  if (dbd->_blocktype == "PARA TREBLE")
    rval = std::make_shared<PARATREBLE>(dbd);

  ////////////////////////
  // filter
  ////////////////////////

  if (dbd->_blocktype == "STEEP RESONANT BASS")
    rval = std::make_shared<STEEP_RESONANT_BASS>(dbd);
  if (dbd->_blocktype == "4POLE HIPASS W/SEP")
    rval = std::make_shared<FOURPOLE_HIPASS_W_SEP>(dbd);
  if (dbd->_blocktype == "NOTCH FILTER")
    rval = std::make_shared<NOTCH_FILT>(dbd);
  if (dbd->_blocktype == "NOTCH2")
    rval = std::make_shared<NOTCH2>(dbd);
  if (dbd->_blocktype == "DOUBLE NOTCH W/SEP")
    rval = std::make_shared<DOUBLE_NOTCH_W_SEP>(dbd);
  if (dbd->_blocktype == "BANDPASS FILT")
    rval = std::make_shared<BANDPASS_FILT>(dbd);
  if (dbd->_blocktype == "BAND2")
    rval = std::make_shared<BAND2>(dbd);

  if (dbd->_blocktype == "LOPAS2")
    rval = std::make_shared<LOPAS2>(dbd);
  if (dbd->_blocktype == "LP2RES")
    rval = std::make_shared<LP2RES>(dbd);
  if (dbd->_blocktype == "LPCLIP")
    rval = std::make_shared<LPCLIP>(dbd);
  if (dbd->_blocktype == "LPGATE")
    rval = std::make_shared<LPGATE>(dbd);

  ////////////////////////
  // nonlin
  ////////////////////////

  if (dbd->_blocktype == "SHAPER")
    rval = std::make_shared<SHAPER>(dbd);
  if (dbd->_blocktype == "2PARAM SHAPER")
    rval = std::make_shared<TWOPARAM_SHAPER>(dbd);
*/
  return rval;
}

} // namespace ork::audio::singularity
