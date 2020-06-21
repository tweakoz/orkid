#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_ringmod.h>
#include <ork/lev2/aud/singularity/modulation.h>

namespace ork::audio::singularity {
RingModData::RingModData(std::string name)
    : DspBlockData(name) {
  _blocktype  = "RingMod";
  auto& param = addParam();
  param.useAmplitudeEvaluator();
}
dspblk_ptr_t RingModData::createInstance() const { // override
  return std::make_shared<RingMod>(this);
}
///////////////////////////////////////////////////////////////////////////////
RingModSumAData::RingModSumAData(std::string name)
    : DspBlockData(name) {
  _blocktype  = "RingModSumA";
  auto& param = addParam();
  param.useAmplitudeEvaluator();
}
dspblk_ptr_t RingModSumAData::createInstance() const { // override
  return std::make_shared<RingModSumA>(this);
}
///////////////////////////////////////////////////////////////////////////////
RingMod::RingMod(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void RingMod::compute(DspBuffer& dspbuf) { // final
  int inumframes       = _layer->_dspwritecount;
  const float* inpbufa = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  const float* inpbufb = getInpBuf(dspbuf, 1) + _layer->_dspwritebase;
  float* outbufa       = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  // printf("running ringmod\n");
  for (int i = 0; i < inumframes; i++) {
    float inA = inpbufa[i] * _dbd->_inputPad;
    float inB = inpbufb[i] * _dbd->_inputPad;
    float res = (inA * inB);
    res       = clip_float(res, -2, 2);
    // printf("ringmod res<%g>\n", res);
    outbufa[i] = res;
  }
}
///////////////////////////////////////////////////////////////////////////////
RingModSumA::RingModSumA(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void RingModSumA::compute(DspBuffer& dspbuf) { // final
  int inumframes       = _layer->_dspwritecount;
  const float* inpbufa = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  const float* inpbufb = getInpBuf(dspbuf, 1) + _layer->_dspwritebase;
  float* outbufa       = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  // printf("running ringmod\n");
  for (int i = 0; i < inumframes; i++) {
    float inA  = inpbufa[i] * _dbd->_inputPad;
    float inB  = inpbufb[i] * _dbd->_inputPad;
    float res  = inA + (inA * inB);
    res        = clip_float(res, -2, 2);
    outbufa[i] = res;
  }
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
