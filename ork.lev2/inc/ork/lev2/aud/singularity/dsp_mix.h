
#pragma once

#include "dspblocks.h"
#include <ork/math/cmatrix4.h>

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
  void doKeyOn(const KeyOnInfo& koi) final;
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
  void doKeyOn(const KeyOnInfo& koi) final;
};
///////////////////////////////////////////////////////////////////////////////
struct StereoDynamicEchoData : public DspBlockData {
  StereoDynamicEchoData();
  dspblk_ptr_t createInstance() const override;
  int64_t _maxdelaylen = 1 << 20;
};
struct StereoDynamicEcho : public DspBlock {
  using dataclass_t = StereoDynamicEchoData;
  StereoDynamicEcho(const StereoDynamicEchoData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  DspBuffer _delaybuffer;
  int64_t _index;
  int64_t _maxdelaylen = 0;
  float _delaylenL     = 0.0f;
  float _delaylenR     = 0.0f;
};
///////////////////////////////////////////////////////////////////////////////
struct Fdn4ReverbData : public DspBlockData {
  Fdn4ReverbData();
  dspblk_ptr_t createInstance() const override;
  int64_t _maxdelaylen = 1 << 20;
};
struct Fdn4Reverb : public DspBlock {
  using dataclass_t = Fdn4ReverbData;
  Fdn4Reverb(const Fdn4ReverbData*);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  DspBuffer _delaybufferA;
  DspBuffer _delaybufferB;
  DspBuffer _delaybufferC;
  DspBuffer _delaybufferD;
  int64_t _indexA;
  int64_t _indexB;
  int64_t _indexC;
  int64_t _indexD;
  fmtx4 _feedbackMatrix;
};

} // namespace ork::audio::singularity
