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

///////////////////////////////////////////////////////////////////////////////

DspParamData::DspParamData() {
  useDefaultEvaluator();
}

///////////////////////////////////////////////////////////////////////////////

DspParamData& DspBlockData::addParam() {
  OrkAssert(_numParams < kmaxparmperblock - 1);
  return _paramd[_numParams++];
}

DspParamData& DspBlockData::getParam(int index) {
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

///////////////////////////////////////////////////////////////////////////////

DspBlock::DspBlock(dspblkdata_constptr_t dbd)
    : _dbd(dbd)
    , _numParams(dbd->_numParams) {
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

const float* DspBlock::getInpBuf(DspBuffer& obuf, int index) {
  int inpidx = _iomask->_inputs[index];
  return obuf.channel(inpidx);
}

///////////////////////////////////////////////////////////////////////////////

float* DspBlock::getOutBuf(DspBuffer& obuf, int index) {
  int outidx = _iomask->_outputs[index];
  return obuf.channel(outidx);
}

///////////////////////////////////////////////////////////////////////////////

// void DspBlock::output(DspBuffer& obuf, int chanidx, int sampleindex, float val) {
// int outidx     = _iomask->_outputs[chanidx];
// float* A       = obuf.channel(outidx);
// A[sampleindex] = val;
//}

///////////////////////////////////////////////////////////////////////////////

int DspBlock::numOutputs() const {
  return _iomask->numOutputs();
}

///////////////////////////////////////////////////////////////////////////////

int DspBlock::numInputs() const {
  return _iomask->numInputs();
}

///////////////////////////////////////////////////////////////////////////////
struct NONE : public DspBlock {
  NONE(dspblkdata_constptr_t dbd)
      : DspBlock(dbd) {
    _numParams = 0;
  }
  void compute(DspBuffer& dspbuf) final {
  }
};
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t createDspBlock(dspblkdata_constptr_t dbd) {
  dspblk_ptr_t rval;

  if (dbd->_dspBlock == "NONE")
    rval = std::make_shared<NONE>(dbd);

  ////////////////////////
  // amp/mix
  ////////////////////////

  if (dbd->_dspBlock == "XFADE")
    rval = std::make_shared<XFADE>(dbd);
  if (dbd->_dspBlock == "x GAIN")
    rval = std::make_shared<XGAIN>(dbd);
  if (dbd->_dspBlock == "GAIN")
    rval = std::make_shared<GAIN>(dbd);
  if (dbd->_dspBlock == "AMP_MONOIO")
    rval = std::make_shared<AMP_MONOIO>(dbd);
  if (dbd->_dspBlock == "AMP")
    rval = std::make_shared<AMP>(dbd);
  if (dbd->_dspBlock == "+ AMP")
    rval = std::make_shared<PLUSAMP>(dbd);
  if (dbd->_dspBlock == "x AMP")
    rval = std::make_shared<XAMP>(dbd);
  if (dbd->_dspBlock == "PANNER")
    rval = std::make_shared<PANNER>(dbd);
  if (dbd->_dspBlock == "AMP U   AMP L")
    rval = std::make_shared<AMPU_AMPL>(dbd);
  if (dbd->_dspBlock == "! AMP")
    rval = std::make_shared<BANGAMP>(dbd);

  if (dbd->_dspBlock == "SUM2")
    rval = std::make_shared<SUM2>(dbd);
  if (dbd->_dspBlock == "RingMod")
    rval = std::make_shared<RingMod>(dbd);
  if (dbd->_dspBlock == "RingModSumA")
    rval = std::make_shared<RingModSumA>(dbd);

  ////////////////////////
  // osc/gen
  ////////////////////////

  if (dbd->_dspBlock == "SAMPLER")
    rval = std::make_shared<SAMPLER>(dbd);
  if (dbd->_dspBlock == "SINE")
    rval = std::make_shared<SINE>(dbd);
  if (dbd->_dspBlock == "LF SIN")
    rval = std::make_shared<SINE>(dbd);
  if (dbd->_dspBlock == "SAW")
    rval = std::make_shared<SAW>(dbd);
  if (dbd->_dspBlock == "SQUARE")
    rval = std::make_shared<SQUARE>(dbd);
  if (dbd->_dspBlock == "SINE+")
    rval = std::make_shared<SINEPLUS>(dbd);
  if (dbd->_dspBlock == "SAW+")
    rval = std::make_shared<SAWPLUS>(dbd);
  if (dbd->_dspBlock == "SW+SHP")
    rval = std::make_shared<SWPLUSSHP>(dbd);
  if (dbd->_dspBlock == "+ SHAPEMOD OSC")
    rval = std::make_shared<PLUSSHAPEMODOSC>(dbd);
  if (dbd->_dspBlock == "SHAPE MOD OSC")
    rval = std::make_shared<SHAPEMODOSC>(dbd);

  if (dbd->_dspBlock == "SYNC M")
    rval = std::make_shared<SYNCM>(dbd);
  if (dbd->_dspBlock == "SYNC S")
    rval = std::make_shared<SYNCS>(dbd);
  if (dbd->_dspBlock == "PWM")
    rval = std::make_shared<PWM>(dbd);

  if (dbd->_dspBlock == "FM4")
    rval = std::make_shared<FM4>(dbd);
  if (dbd->_dspBlock == "CZX")
    rval = std::make_shared<CZX>(dbd);

  if (dbd->_dspBlock == "NOISE")
    rval = std::make_shared<NOISE>(dbd);

  ////////////////////////
  // EQ
  ////////////////////////

  if (dbd->_dspBlock == "PARA BASS")
    rval = std::make_shared<PARABASS>(dbd);
  if (dbd->_dspBlock == "PARA MID")
    rval = std::make_shared<PARAMID>(dbd);
  if (dbd->_dspBlock == "PARA TREBLE")
    rval = std::make_shared<PARATREBLE>(dbd);
  if (dbd->_dspBlock == "PARAMETRIC EQ")
    rval = std::make_shared<PARAMETRIC_EQ>(dbd);

  ////////////////////////
  // filter
  ////////////////////////

  if (dbd->_dspBlock == "2POLE ALLPASS")
    rval = std::make_shared<TWOPOLE_ALLPASS>(dbd);
  if (dbd->_dspBlock == "2POLE LOWPASS")
    rval = std::make_shared<TWOPOLE_LOWPASS>(dbd);

  if (dbd->_dspBlock == "STEEP RESONANT BASS")
    rval = std::make_shared<STEEP_RESONANT_BASS>(dbd);
  if (dbd->_dspBlock == "4POLE LOPASS W/SEP")
    rval = std::make_shared<FOURPOLE_LOPASS_W_SEP>(dbd);
  if (dbd->_dspBlock == "4POLE HIPASS W/SEP")
    rval = std::make_shared<FOURPOLE_HIPASS_W_SEP>(dbd);
  if (dbd->_dspBlock == "NOTCH FILTER")
    rval = std::make_shared<NOTCH_FILT>(dbd);
  if (dbd->_dspBlock == "NOTCH2")
    rval = std::make_shared<NOTCH2>(dbd);
  if (dbd->_dspBlock == "DOUBLE NOTCH W/SEP")
    rval = std::make_shared<DOUBLE_NOTCH_W_SEP>(dbd);
  if (dbd->_dspBlock == "BANDPASS FILT")
    rval = std::make_shared<BANDPASS_FILT>(dbd);
  if (dbd->_dspBlock == "BAND2")
    rval = std::make_shared<BAND2>(dbd);

  if (dbd->_dspBlock == "LOPAS2")
    rval = std::make_shared<LOPAS2>(dbd);
  if (dbd->_dspBlock == "LP2RES")
    rval = std::make_shared<LP2RES>(dbd);
  if (dbd->_dspBlock == "LOPASS")
    rval = std::make_shared<LOPASS>(dbd);
  if (dbd->_dspBlock == "LPCLIP")
    rval = std::make_shared<LPCLIP>(dbd);
  if (dbd->_dspBlock == "LPGATE")
    rval = std::make_shared<LPGATE>(dbd);

  if (dbd->_dspBlock == "HIPASS")
    rval = std::make_shared<HIPASS>(dbd);
  if (dbd->_dspBlock == "ALPASS")
    rval = std::make_shared<ALPASS>(dbd);

  if (dbd->_dspBlock == "HIFREQ STIMULATOR")
    rval = std::make_shared<HIFREQ_STIMULATOR>(dbd);

  ////////////////////////
  // nonlin
  ////////////////////////

  if (dbd->_dspBlock == "SHAPER")
    rval = std::make_shared<SHAPER>(dbd);
  if (dbd->_dspBlock == "2PARAM SHAPER")
    rval = std::make_shared<TWOPARAM_SHAPER>(dbd);
  if (dbd->_dspBlock == "WRAP")
    rval = std::make_shared<WRAP>(dbd);
  if (dbd->_dspBlock == "DIST")
    rval = std::make_shared<DIST>(dbd);

  return rval;
}

} // namespace ork::audio::singularity
