#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {

struct RingModData final : public DspBlockData {
  RingModData();
  dspblk_ptr_t createInstance() const override;
};
struct RingModSumAData final : public DspBlockData {
  RingModSumAData();
  dspblk_ptr_t createInstance() const override;
};

struct RingMod : public DspBlock {
  using dataclass_t = RingModData;
  RingMod(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct RingModSumA : public DspBlock {
  using dataclass_t = RingModSumAData;
  RingModSumA(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
} // namespace ork::audio::singularity
