
#pragma once

#include "dspblocks.h"
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector4.h>
#include "shelveeq.h"

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
  void setNextDelayTime(float dt);
  static constexpr int64_t _maxdelay = 1 << 20;
  static constexpr int64_t _maxx     = _maxdelay << 16;

  int64_t _index     = 0;
  float _basDelayLen = 0.0f;
  float _tgtDelayLen = 0.0f;
  DspBuffer _buffer;
  float* _bufdata = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
struct PitchShifterData : public DspBlockData {
  PitchShifterData();
  dspblk_ptr_t createInstance() const override;
};
struct PitchShifter : public DspBlock {
  using dataclass_t = PitchShifterData;
  PitchShifter(const PitchShifterData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;

  int64_t _phaseA;
  int64_t _phaseB;
  int64_t _phaseC;
  int64_t _phaseD;
  BiQuad _hipassfilter;
  BiQuad _lopassAfilter;
  BiQuad _lopassBfilter;
  BiQuad _lopassCfilter;
  BiQuad _lopassDfilter;
  BiQuad _lopassEfilter;
  BiQuad _lopassFfilter;
  BiQuad _lopassGfilter;
  BiQuad _lopassHfilter;

  DelayContext _delayA;
  DelayContext _delayB;
  DelayContext _delayC;
  DelayContext _delayD;
};
///////////////////////////////////////////////////////////////////////////////
struct StereoDynamicEchoData : public DspBlockData {
  StereoDynamicEchoData();
  dspblk_ptr_t createInstance() const override;
};
struct StereoDynamicEcho : public DspBlock {
  using dataclass_t = StereoDynamicEchoData;
  StereoDynamicEcho(const StereoDynamicEchoData* dbd);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  DelayContext _delayL;
  DelayContext _delayR;
};
///////////////////////////////////////////////////////////////////////////////
// Feedback Delay Network Reverb (4 nodes)
///////////////////////////////////////////////////////////////////////////////
struct Fdn4ReverbData : public DspBlockData {
  Fdn4ReverbData(float tscale);
  dspblk_ptr_t createInstance() const override;
  float _tscale;
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
  BiQuad _hipassfilterL;
  BiQuad _hipassfilterR;
  fmtx4 _feedbackMatrix;
  fvec4 _inputGainsL;
  fvec4 _inputGainsR;
  fvec4 _outputGainsL;
  fvec4 _outputGainsR;
};
///////////////////////////////////////////////////////////////////////////////
// Feedback Delay Network Reverb (4 nodes) with rotation matrix
///////////////////////////////////////////////////////////////////////////////
struct Fdn4ReverbXData : public DspBlockData {
  Fdn4ReverbXData(float tscale);
  dspblk_ptr_t createInstance() const override;
  float _tscale = 1.0f;

  fvec4 _inputGainsL;
  fvec4 _inputGainsR;
  fvec4 _outputGainsL;
  fvec4 _outputGainsR;
  fvec3 _axis;
  float _angle;
  float _speed = 0.0f;
};
struct Fdn4ReverbX : public DspBlock {
  using dataclass_t = Fdn4ReverbXData;
  Fdn4ReverbX(const Fdn4ReverbXData*);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  void matrixHadamard(float fblevel);
  void matrixHouseholder();

  fvec3 _axis;
  float _angle;
  float _speed = 0.0f;

  DelayContext _delayA;
  DelayContext _delayB;
  DelayContext _delayC;
  DelayContext _delayD;
  BiQuad _filterA;
  BiQuad _filterB;
  BiQuad _filterC;
  BiQuad _filterD;

  fmtx4 _feedbackMatrix;
  fvec4 _inputGainsL;
  fvec4 _inputGainsR;
  fvec4 _outputGainsL;
  fvec4 _outputGainsR;
  fvec4 _delayTimes;
};
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
