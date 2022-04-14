////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/krztypes.h>
#include <ork/lev2/aud/singularity/wavetable.h>
#include <map>

namespace ork::audio::singularity {

static std::map<std::string, Wavetable*> _wavemap;
static const int64_t ksize     = 65536;
static const int64_t ksizem64k = ksize << 16;
static const int64_t ksized2   = ksize / 2;
static const int64_t ksized4   = ksize / 4;
static const int64_t ksized8   = ksize / 8;

///////////////////////////////////////////////////////////////////////////////

Wavetable::Wavetable(int isize) {
  if (isize)
    _wavedata.resize(isize);
}
Wavetable::~Wavetable() {
}

float Wavetable::sample(float fi) const {
  int64_t phase = int64_t(fi * float(ksize)) % ksize;
  return _wavedata[phase];
}
float Wavetable::sampleLerp(float fi) const {
  int64_t phaseA = int64_t(fi * float(ksizem64k)) & (ksizem64k - 1);
  int64_t phaseB = phaseA + 65536;
  float fract    = float(phaseA & 0xffff) * kinv64k;
  float invfr    = 1.0f - fract;
  int64_t iiA    = (phaseA >> 16) & (ksize - 1);
  int64_t iiB    = (iiA + 1) & (ksize - 1);
  float sampA    = float(_wavedata[iiA]);
  float sampB    = float(_wavedata[iiB]);
  float rval     = (sampB * fract + sampA * invfr);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void initWavetablesTX81Z() { /////////////////////
  // wave 2(1)
  /////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      int quadrant = i / ksized4;
      float fph    = float(i) / ksize;
      switch (quadrant) {
        case 0:
        case 1:
        case 2:
        case 3:
          wave->_wavedata[i] = fabs(sin(fph * pi2)) * sin(fph * pi2);
          break;
      }
    }
    _wavemap["tx81z.2"] = wave;
  } // namespace ork::audio::singularity
  /////////////////////
  // wave 3(2)
  /////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      int quadrant    = i / ksized4;
      float fph       = float(i) / ksize;
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
        case 1:
          wave->_wavedata[i] = sin(fph * pi2);
          break;
        case 2:
        case 3:
          out_float = 0.0f;
          break;
      }
    }
    _wavemap["tx81z.3"] = wave;
  }
  /////////////////////
  // wave 4(3)
  /////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      int quadrant    = i / ksized4;
      float fph       = float(i) / ksize;
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
        case 1:
          wave->_wavedata[i] = fabs(sin(fph * pi2)) * sin(fph * pi2);
          break;
        case 2:
        case 3:
          out_float = 0.0f;
          break;
      }
    }
    _wavemap["tx81z.4"] = wave;
  }
  /////////////////////
  // wave 5
  /////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      int quadrant    = i / ksized4;
      float fph       = float(i) / ksize;
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
        case 1:
          wave->_wavedata[i] = sin(fph * pi2 * 2.0);
        case 2:
        case 3:
          wave->_wavedata[i] = 0.0f;
          break;
      }
    }
    _wavemap["tx81z.5"] = wave;
  }
  /////////////////////
  // wave 6
  /////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      int quadrant    = i / ksized4;
      float fph       = float(i) / ksize;
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
        case 1:
          wave->_wavedata[i] = fabs(sin(fph * pi2 * 2.0)) * sin(fph * pi2 * 2.0);
          break;
        case 2:
        case 3:
          out_float = 0.0f;
          break;
      }
    }
    _wavemap["tx81z.6"] = wave;
  }
  /////////////////////
  // wave 7
  /////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      int quadrant    = i / ksized4;
      float fph       = float(i) / ksize;
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
        case 1:
          wave->_wavedata[i] = sin(fph * pi2 * 4.0);
          break;
        case 2:
        case 3:
          out_float = 0.0f;
          break;
      }
    }
    _wavemap["tx81z.7"] = wave;
  }
  /////////////////////
  // wave 8
  /////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      int quadrant    = i / (ksized4);
      int iph         = i;
      float fph       = float(iph) / float(ksize);
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
        case 1: {
          float phase        = fph * pi2 * 2.0f; // * 4.0;
          wave->_wavedata[i] = fabs(sin(phase)) * sin(phase);
          break;
        }
        case 2:
        case 3:
          out_float = 0.0f;
          break;
      }
    }
    _wavemap["tx81z.8"] = wave;
  }
} // namespace ork::audio::singularity

///////////////////////////////////////////////////////////////////////////////

void initWavetables() {
  ////////////////////////
  // sine
  ////////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      float fph          = float(i) / ksize;
      float fsine        = sinf(fph * pi2);
      wave->_wavedata[i] = fsine;
    }
    _wavemap["sine"] = wave;
  }
  ////////////////////////
  // sincwrap
  ////////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      int j       = i - ksized2;
      float fj    = float(j) / ksize;
      float fph   = fj * 8.0f * pi2;
      float fsinc = (fph == 0.0f) ? 1.0f : sinf(fph) / fph;
      // printf( "fsinc<%i:%f>\n", i, fsinc );
      wave->_wavedata[i] = fsinc;
    }
    _wavemap["sincw8pi"] = wave;
  }
  ////////////////////////
  // sincwrap
  ////////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      int j       = i - ksized2;
      float fj    = float(j) / ksize;
      float fph   = (fj)*8.0f * pi2;
      float fsinc = (fph == 0.0f) ? 1.0f : sinf(fph) / fph;
      // printf( "fsinc<%i:%f>\n", i, fsinc );
      wave->_wavedata[i] = (fsinc > 0.0f) ? 1.0f - fsinc : fsinc + 1.0f;
    }
    _wavemap["isincw8pi"] = wave;
  }

  initWavetablesTX81Z();
}

///////////////////////////////////////////////////////////////////////////////

const Wavetable* builtinWaveTable(const std::string& name) {
  static bool ginit = true;
  if (ginit) {
    ginit = false;
    initWavetables();
  }

  ///////////////////////////////////

  auto it = _wavemap.find(name);
  return (it == _wavemap.end()) ? nullptr : it->second;

  ///////////////////////////////////
}

} // namespace ork::audio::singularity
