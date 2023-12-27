////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>
#include <ork/lev2/aud/singularity/alg_pan.inl>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

DelayContext::DelayContext() {
  _buffer.resize(_maxdelay);
  _index   = 0;
  _bufdata = _buffer.channel(0);
}

///////////////////////////////////////////////////////////////////////////////

float DelayContext::out(float fi) const {
  int64_t index64       = _index << 16;
  float delaylen        = lerp(_basDelayLen, _tgtDelayLen, fi);
  int64_t outdelayindex = index64 - int64_t(delaylen * 65536.0f);

  while (outdelayindex < 0)
    outdelayindex += _maxx;
  while (outdelayindex >= _maxx)
    outdelayindex -= _maxx;

  float fract    = float(outdelayindex & 0xffff) * kinv64k;
  float invfr    = 1.0f - fract;
  int64_t iiA    = (outdelayindex >> 16) % _maxdelay;
  int64_t iiB    = (iiA + 1) % _maxdelay;
  float sampA    = _bufdata[iiA];
  float sampB    = _bufdata[iiB];
  float delayout = (sampB * fract + sampA * invfr);
  return delayout;
}

///////////////////////////////////////////////////////////////////////////////

void DelayContext::inp(float inp) {
  int64_t inpdelayindex = _index++;
  while (inpdelayindex < 0)
    inpdelayindex += _maxdelay;
  while (inpdelayindex >= _maxdelay)
    inpdelayindex -= _maxdelay;
  _bufdata[inpdelayindex] = inp;
}

///////////////////////////////////////////////////////////////////////////////

void DelayContext::setStaticDelayTime(float dt) {
  float delaylen = dt * getSampleRate();
  _basDelayLen   = delaylen;
  _tgtDelayLen   = delaylen;
}
///////////////////////////////////////////////////////////////////////////////

void DelayContext::setNextDelayTime(float dt) {
  // printf("dt<%g>\n", dt);
  float delaylen = dt * getSampleRate();
  _basDelayLen   = _tgtDelayLen;
  _tgtDelayLen   = delaylen;
}

///////////////////////////////////////////////////////////////////////////////

DelayInput::DelayInput(){
  _buffer.resize(_maxdelay);
  _index   = 0;
  _bufdata = _buffer.channel(0);
}

void DelayInput::inp(float inputSample) {
  int64_t inpdelayindex = _index++;
  while (inpdelayindex < 0)
    inpdelayindex += _maxdelay;
  while (inpdelayindex >= _maxdelay)
    inpdelayindex -= _maxdelay;
  _buffer.channel(0)[inpdelayindex] = inputSample;
}
  void DelayInput::setDelayTime(float delayTime) {
    float delaylen = delayTime * getSampleRate();
    _delayLen = delaylen;
  }
  DelayOutput::DelayOutput(DelayInput& input)
    : _input(input) {}

  float DelayOutput::out(float fi, size_t tapIndex) const {
    if (tapIndex >= _tapDelays.size()) {
      // Handle error or return a default value
      return 0.0f;
    }

    int64_t index64 = _input._index << 16;
    float delaylen = _tapDelays[tapIndex];
    int64_t outdelayindex = index64 - int64_t(delaylen * 65536.0f);

    while (outdelayindex < 0)
      outdelayindex += _maxx;
    while (outdelayindex >= _maxx)
      outdelayindex -= _maxx;

    float fract = float(outdelayindex & 0xffff) * kinv64k;
    float invfr = 1.0f - fract;
    int64_t iiA = (outdelayindex >> 16) % _maxdelay;
    int64_t iiB = (iiA + 1) % _maxdelay;
    float sampA = _input._buffer.channel(0)[iiA];
    float sampB = _input._buffer.channel(0)[iiB];
    return (sampB * fract + sampA * invfr);
  }

  void DelayOutput::addTap(float tapDelay) {
    _tapDelays.push_back(tapDelay);
  }

  void DelayOutput::removeTap(size_t tapIndex) {
    if (tapIndex < _tapDelays.size()) {
      _tapDelays.erase(_tapDelays.begin() + tapIndex);
    }
  }
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
