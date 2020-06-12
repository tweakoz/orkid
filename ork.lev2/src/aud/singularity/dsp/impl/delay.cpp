////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
} // namespace ork::audio::singularity
