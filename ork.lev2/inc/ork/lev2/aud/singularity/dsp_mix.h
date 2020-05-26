
#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {

struct SUM2 : public DspBlock {
  static void initBlock(dspblkdata_ptr_t blockdata);
  SUM2(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
};

} // namespace ork::audio::singularity
