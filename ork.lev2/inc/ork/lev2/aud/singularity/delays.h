////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "dspbuffer.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
struct DelayContext {
  DelayContext();
  float out(float fi) const;
  void inp(float inp);
  void clear();
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

} //namespace ork::audio::singularity {

