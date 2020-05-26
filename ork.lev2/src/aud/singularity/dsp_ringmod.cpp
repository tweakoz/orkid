#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_ringmod.h>
#include <ork/lev2/aud/singularity/modulation.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
void RingMod::initBlock(dspblkdata_ptr_t blockdata) {
  blockdata->_dspBlock = "RingMod";
  auto& param          = blockdata->addParam();
  param.useAmplitudeEvaluator();
}
///////////////////////////////////////////////////////////////////////////////
RingMod::RingMod(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void RingMod::compute(DspBuffer& dspbuf) { // final
  int inumframes       = _numFrames;
  const float* inpbufa = getInpBuf(dspbuf, 0);
  const float* inpbufb = getInpBuf(dspbuf, 1);
  float* outbufa       = getOutBuf(dspbuf, 0);
  float* outbufb       = getOutBuf(dspbuf, 1);
  // printf("running ringmod\n");
  for (int i = 0; i < inumframes; i++) {
    float inA  = inpbufa[i] * _dbd->_inputPad;
    float inB  = inpbufb[i] * _dbd->_inputPad;
    float res  = (inA * inB);
    res        = clip_float(res, -2, 2);
    outbufa[i] = res;
    outbufb[i] = res;
  }
}
///////////////////////////////////////////////////////////////////////////////
void RingModSumA::initBlock(dspblkdata_ptr_t blockdata) {
  blockdata->_dspBlock = "RingModSumA";
  auto& param          = blockdata->addParam();
  param.useAmplitudeEvaluator();
}
///////////////////////////////////////////////////////////////////////////////
RingModSumA::RingModSumA(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void RingModSumA::compute(DspBuffer& dspbuf) { // final
  int inumframes       = _numFrames;
  const float* inpbufa = getInpBuf(dspbuf, 0);
  const float* inpbufb = getInpBuf(dspbuf, 1);
  float* outbufa       = getOutBuf(dspbuf, 0);
  float* outbufb       = getOutBuf(dspbuf, 1);
  // printf("running ringmod\n");
  for (int i = 0; i < inumframes; i++) {
    float inA  = inpbufa[i] * _dbd->_inputPad;
    float inB  = inpbufb[i] * _dbd->_inputPad;
    float res  = inA + (inA * inB);
    res        = clip_float(res, -2, 2);
    outbufa[i] = res;
    outbufb[i] = res;
  }
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
