////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#pragma once

#include "reflection.h"
#include "dspblocks.h"
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector4.h>
#include "shelveeq.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
struct Sum2Data final : public DspBlockData {
  Sum2Data(std::string name);
  dspblk_ptr_t createInstance() const override;
};

struct SUM2 : public DspBlock {
  using dataclass_t = Sum2Data;
  SUM2(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
///////////////////////////////////////////////////////////////////////////////
struct MonoInStereoOutData : public DspBlockData {
  DeclareConcreteX(MonoInStereoOutData, DspBlockData);
  MonoInStereoOutData(std::string name = "");
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
  StereoEnhancerData(std::string name);
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
struct DelayInput {
  DelayInput();
  void inp(float inputSample);
  void setDelayTime(float delayTime);
  DspBuffer _buffer;
  int64_t _index = 0;
  float _delayLen = 0.0f;
  float* _bufdata = nullptr;
  static constexpr int64_t _maxdelay = 1 << 20;
};
struct DelayOutput {
  DelayOutput(DelayInput& input);
  float out(float fi, size_t tapIndex) const;
  void addTap(float tapDelay);
  void removeTap(size_t tapIndex);
  DelayInput& _input;
  std::vector<float> _tapDelays;
  static constexpr int64_t _maxdelay = 1 << 20;
  static constexpr int64_t _maxx = _maxdelay << 16;
};

///////////////////////////////////////////////////////////////////////////////
struct PitchShifterData : public DspBlockData {
  PitchShifterData(std::string name);
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
struct RecursivePitchShifterData : public DspBlockData {
  RecursivePitchShifterData(std::string name,float feedback);
  dspblk_ptr_t createInstance() const override;
  float _feedback;
};
struct RecursivePitchShifter : public DspBlock {
  using dataclass_t = RecursivePitchShifterData;
  RecursivePitchShifter(const dataclass_t* dbd);
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

  DelayContext _delayOuter;
  const dataclass_t* _mydata;

};
///////////////////////////////////////////////////////////////////////////////
struct StereoDynamicEchoData : public DspBlockData {
  StereoDynamicEchoData(std::string name);
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
  Fdn4ReverbData(std::string name);
  dspblk_ptr_t createInstance() const override;
  float _time_base  = 0.01 ; // sec
  float _time_scale = 0.031; // sec
  float _input_gain = 0.75;  // linear
  float _output_gain = 0.75; // linear
  float _matrix_gain = 0.35; // linear
  float _allpass_cutoff = 4500.0; // hz
  float _hipass_cutoff = 200.0; // hz
};
struct Fdn4Reverb : public DspBlock {
  using dataclass_t = Fdn4ReverbData;
  Fdn4Reverb(const Fdn4ReverbData*);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  void matrixHadamard(float fblevel);
  void matrixHouseholder(float fbgain=0.45);

  const Fdn4ReverbData* _mydata;
  DelayContext _delayA;
  DelayContext _delayB;
  DelayContext _delayC;
  DelayContext _delayD;
  BiQuad _hipassfilterL;
  BiQuad _hipassfilterR;
  TrapAllpass _allpassA;
  TrapAllpass _allpassB;
  TrapAllpass _allpassC;
  TrapAllpass _allpassD;
  TrapAllpass _allpassE;
  TrapAllpass _allpassF;
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
  Fdn4ReverbXData(std::string name);
  dspblk_ptr_t createInstance() const override;
  float _tscale = 1.0f;

  void update();
  
  fvec4 _inputGainsL;
  fvec4 _inputGainsR;
  fvec4 _outputGainsL;
  fvec4 _outputGainsR;
  fvec3 _axis;
  float _angle;
  float _speed = 0.0f;
  float _time_scale = 1.0; // x
  float _input_gain = 0.75;  // linear
  float _output_gain = 0.75; // linear
  float _matrix_gain = 0.35; // linear
  float _allpass_cutoff = 4500.0; // hz
  float _hipass_cutoff = 200.0; // hz
};
struct Fdn4ReverbX : public DspBlock {
  using dataclass_t = Fdn4ReverbXData;
  Fdn4ReverbX(const Fdn4ReverbXData*);
  void compute(DspBuffer& dspbuf) final;
  void doKeyOn(const KeyOnInfo& koi) final;
  void matrixHadamard(float fblevel);
  void matrixHouseholder(float fbgain);

  const Fdn4ReverbXData* _mydata;
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
  TrapAllpass _allpassA;
  TrapAllpass _allpassB;
  TrapAllpass _allpassC;
  TrapAllpass _allpassD;
  TrapAllpass _allpassE;
  TrapAllpass _allpassF;

  fmtx4 _feedbackMatrix;
  fvec4 _inputGainsL;
  fvec4 _inputGainsR;
  fvec4 _outputGainsL;
  fvec4 _outputGainsR;
  fvec4 _delayTimes;
};
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
