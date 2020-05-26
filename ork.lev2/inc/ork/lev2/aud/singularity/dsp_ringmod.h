#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {
struct RingMod : public DspBlock {
  static void initBlock(dspblkdata_ptr_t blockdata);
  RingMod(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct RingModSumA : public DspBlock {
  static void initBlock(dspblkdata_ptr_t blockdata);
  RingModSumA(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;
};
} //namespace ork::audio::singularity {
