
#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
struct Sum2Data final : public DspBlockData {
  Sum2Data();
  dspblk_ptr_t createInstance() const override;
};

struct SUM2 : public DspBlock {
  using dataclass_t = Sum2Data;
  SUM2(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
///////////////////////////////////////////////////////////////////////////////
struct MonoInStereoOutData : public DspBlockData {
  MonoInStereoOutData();
  dspblk_ptr_t createInstance() const override;
};
struct MonoInStereoOut : public DspBlock {
  using dataclass_t = MonoInStereoOutData;
  MonoInStereoOut(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
  float _filt;
  float _panbase;
};
///////////////////////////////////////////////////////////////////////////////
struct StereoEnhancerData : public DspBlockData {
  StereoEnhancerData();
  dspblk_ptr_t createInstance() const override;
};
struct StereoEnhancer : public DspBlock {
  using dataclass_t = StereoEnhancerData;
  StereoEnhancer(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const DspKeyOnInfo& koi) final;
};

} // namespace ork::audio::singularity
