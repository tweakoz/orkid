#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// nonlinear blocks
///////////////////////////////////////////////////////////////////////////////

struct SHAPER : public DspBlock {
  SHAPER(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct SHAPE2 : public DspBlock {
  SHAPE2(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct TWOPARAM_SHAPER : public DspBlock {
  TWOPARAM_SHAPER(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  float ph1, ph2;
  void doKeyOn(const KeyOnInfo& koi) final;
};
struct WrapData : public DspBlockData {
  WrapData();
  dspblk_ptr_t createInstance() const override;
};
struct Wrap : public DspBlock {
  using dataclass_t = WrapData;
  Wrap(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct DistortionData : public DspBlockData {
  DistortionData();
  dspblk_ptr_t createInstance() const override;
};
struct Distortion : public DspBlock {
  using dataclass_t = DistortionData;
  Distortion(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};

} // namespace ork::audio::singularity
