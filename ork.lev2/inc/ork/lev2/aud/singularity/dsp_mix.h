
#pragma once

#include "dspblocks.h"
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector4.h>

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
struct DelayContext {
  DelayContext();
  float out(float fi) const;
  void inp(float inp);
  void setStaticDelayTime(float dt);
  static constexpr float kinv64k     = 1.0f / 65536.0f;
  static constexpr int64_t _maxdelay = 1 << 20;
  static constexpr int64_t _maxx     = _maxdelay << 16;

  int64_t _index     = 0;
  float _basDelayLen = 0.5f;
  float _tgtDelayLen = 0.5f;
  DspBuffer _buffer;
  float* _bufdata = nullptr;
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
// Feedback Delay Network Reverb (4 nodes)
///////////////////////////////////////////////////////////////////////////////
struct Fdn4ReverbData : public DspBlockData {
  Fdn4ReverbData();
  dspblk_ptr_t createInstance() const override;
};
struct Fdn4Reverb : public DspBlock {
  using dataclass_t = Fdn4ReverbData;
  Fdn4Reverb(const Fdn4ReverbData*);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  void matrixHadamard(float fblevel);
  void matrixHouseholder();

  DelayContext _delayA;
  DelayContext _delayB;
  DelayContext _delayC;
  DelayContext _delayD;
  fmtx4 _feedbackMatrix;
  fvec4 _inputGainsL;
  fvec4 _inputGainsR;
  fvec4 _outputGainsL;
  fvec4 _outputGainsR;
};

///////////////////////////////////////////////////////////////////////////////
// fx presets
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_stereochorus();
lyrdata_ptr_t fxpreset_fdn4reverb();
lyrdata_ptr_t fxpreset_multitest();
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
