#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
void SUM2::initBlock(dspblkdata_ptr_t blockdata) {
  blockdata->_dspBlock = "SUM2";
  auto& param          = blockdata->addParam();
  param.useAmplitudeEvaluator();
}
///////////////////////////////////////////////////////////////////////////////
SUM2::SUM2(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void SUM2::compute(DspBuffer& dspbuf) { // final
  int inumframes       = _numFrames;
  const float* inpbufa = getInpBuf(dspbuf, 0);
  const float* inpbufb = getInpBuf(dspbuf, 1);
  float* outbufa       = getOutBuf(dspbuf, 0);
  float* outbufb       = getOutBuf(dspbuf, 1);
  for (int i = 0; i < inumframes; i++) {
    float inA  = inpbufa[i] * _dbd->_inputPad;
    float inB  = inpbufb[i] * _dbd->_inputPad;
    float res  = (inA + inB);
    res        = clip_float(res, -2, 2);
    outbufa[i] = res;
    outbufb[i] = res;
  }
}
///////////////////////////////////////////////////////////////////////////////
void MUL2::initBlock(dspblkdata_ptr_t blockdata) {
  blockdata->_dspBlock = "MUL2";
  auto& param          = blockdata->addParam();
  param.useAmplitudeEvaluator();
}
///////////////////////////////////////////////////////////////////////////////
MUL2::MUL2(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void MUL2::compute(DspBuffer& dspbuf) { // final
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
