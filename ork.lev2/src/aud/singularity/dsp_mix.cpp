////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
Sum2Data::Sum2Data() {
  _blocktype = "SUM2";
}
dspblk_ptr_t Sum2Data::createInstance() const { // override
  return std::make_shared<SUM2>(this);
}
///////////////////////////////////////////////////////////////////////////////
SUM2::SUM2(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void SUM2::compute(DspBuffer& dspbuf) { // final
  int inumframes       = _layer->_dspwritecount;
  int ibase            = _layer->_dspwritebase;
  const float* inpbufa = getInpBuf(dspbuf, 0) + ibase;
  const float* inpbufb = getInpBuf(dspbuf, 1) + ibase;
  float* outbufa       = getOutBuf(dspbuf, 0) + ibase;
  float* outbufb       = getOutBuf(dspbuf, 1) + ibase;
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
} // namespace ork::audio::singularity
