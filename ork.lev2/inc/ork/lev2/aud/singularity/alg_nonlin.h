#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// nonlinear blocks
///////////////////////////////////////////////////////////////////////////////

struct SHAPER : public DspBlock {
  SHAPER(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct SHAPE2 : public DspBlock {
  SHAPE2(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct TWOPARAM_SHAPER : public DspBlock {
  TWOPARAM_SHAPER(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
  float ph1, ph2;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct WRAP : public DspBlock {
  WRAP(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct DIST : public DspBlock {
  DIST(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
};

} // namespace ork::audio::singularity
