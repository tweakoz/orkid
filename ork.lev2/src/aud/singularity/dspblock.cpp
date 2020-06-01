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

namespace ork::audio::singularity {

DspBlockData::DspBlockData() {
  for (int i = 0; i < kmaxdspblocksperstage; i++)
    _dspchannel[i] = i;
}

///////////////////////////////////////////////////////////////////////////////

DspParamData::DspParamData() {
  useDefaultEvaluator();
}

///////////////////////////////////////////////////////////////////////////////

DspParamData& DspBlockData::addParam() {
  OrkAssert(_numParams < kmaxparmperblock - 1);
  return _paramd[_numParams++];
}

DspParamData& DspBlockData::param(int index) {
  return _paramd[index];
}

///////////////////////////////////////////////////////////////////////////////

DspBuffer::DspBuffer()
    : _maxframes(0)
    , _numframes(0) {
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

float* DspBuffer::channel(int ich) {
  ich = ich % kmaxdspblocksperstage;
  return _channels[ich].data();
}

///////////////////////////////////////////////////////////////////////////////

DspStageData::DspStageData() {
  _iomask = std::make_shared<IoMask>();
}

dspblkdata_ptr_t DspStageData::appendBlock() {
  OrkAssert(_numblocks < kmaxdspblocksperstage);
  auto blk                  = std::make_shared<DspBlockData>();
  _blockdatas[_numblocks++] = blk;
  return blk;
}

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
  for (int i = 0; i < kmaxdspblocksperstage; i++)
    _dspchannel[i] = dbd->_dspchannel[i];
}

///////////////////////////////////////////////////////////////////////////////

FPARAM DspBlock::initFPARAM(const DspParamData& dpd) {
  FPARAM rval;
  rval._coarse    = dpd._coarse;
  rval._fine      = dpd._fine;
  rval._C1        = _layer->getSRC1(dpd._mods);
  rval._C2        = _layer->getSRC2(dpd._mods);
  rval._evaluator = dpd._mods._evaluator;

  rval._keyTrack      = dpd._keyTrack;
  rval._velTrack      = dpd._velTrack;
  rval._kstartNote    = dpd._keystartNote;
  rval._kstartBipolar = dpd._keystartBipolar;

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void DspBlock::keyOn(const DspKeyOnInfo& koi) {
  _layer = koi._layer;
  for (int i = 0; i < _numParams; i++) {
    _param[i] = initFPARAM(_dbd->_paramd[i]);
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

  if (dbd->_blocktype == "FM4")
    rval = std::make_shared<FM4>(dbd);
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
  if (dbd->_blocktype == "PARAMETRIC EQ")
    rval = std::make_shared<PARAMETRIC_EQ>(dbd);

  ////////////////////////
  // filter
  ////////////////////////

  if (dbd->_blocktype == "2POLE ALLPASS")
    rval = std::make_shared<TWOPOLE_ALLPASS>(dbd);

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
  if (dbd->_blocktype == "LOPASS")
    rval = std::make_shared<LOPASS>(dbd);
  if (dbd->_blocktype == "LPCLIP")
    rval = std::make_shared<LPCLIP>(dbd);
  if (dbd->_blocktype == "LPGATE")
    rval = std::make_shared<LPGATE>(dbd);

  if (dbd->_blocktype == "HIPASS")
    rval = std::make_shared<HIPASS>(dbd);
  if (dbd->_blocktype == "ALPASS")
    rval = std::make_shared<ALPASS>(dbd);

  if (dbd->_blocktype == "HIFREQ STIMULATOR")
    rval = std::make_shared<HIFREQ_STIMULATOR>(dbd);

  ////////////////////////
  // nonlin
  ////////////////////////

  if (dbd->_blocktype == "SHAPER")
    rval = std::make_shared<SHAPER>(dbd);
  if (dbd->_blocktype == "2PARAM SHAPER")
    rval = std::make_shared<TWOPARAM_SHAPER>(dbd);

  return rval;
}

} // namespace ork::audio::singularity
