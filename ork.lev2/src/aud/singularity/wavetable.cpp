#include <ork/lev2/aud/singularity/krztypes.h>
#include <ork/lev2/aud/singularity/wavetable.h>
#include <map>

namespace ork::audio::singularity {

static std::map<std::string, Wavetable*> _wavemap;
static const int ksize     = 4096;
static const int ksizem64k = ksize << 16;
static const int ksized2   = ksize / 2;
static const int ksized4   = ksize / 4;
static const int ksized8   = ksize / 8;
static const float kinv64k = 1.0f / 65536.0f;

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

void initWavetablesTX81Z() {
  /////////////////////
  // wave 2(1)
  /////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      int quadrant    = i / ksized4;
      float fph       = float(i & (ksized4 - 1)) / float(ksized4);
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
          out_float = sqrtf(fph);
          break;
        case 1:
          out_float = sqrtf(1.0f - fph);
          break;
        case 2:
          out_float = -sqrtf(fph);
          break;
        case 3:
          out_float = -sqrtf(1.0f - fph);
          break;
      }
    }
    _wavemap["tx81z.2"] = wave;
  }
  /////////////////////
  // wave 3(2)
  /////////////////////
  {
    auto wave = new Wavetable(ksize);
    for (int i = 0; i < ksize; i++) {
      int quadrant    = i / ksized4;
      float fph       = float(i) / ksize;
      float fsine     = sinf(fph * pi2);
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
        case 1:
          out_float = fsine;
          break;
        case 2:
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
      float fph       = float(i & (ksized4 - 1)) / float(ksized4);
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
          out_float = sqrtf(fph);
          break;
        case 1:
          out_float = sqrtf(1.0f - fph);
          break;
        case 2:
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
      float fph       = float(i & (ksized4 - 1)) / float(ksized4);
      float fsine     = sinf(fph * pi2 * 2.0f);
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
        case 1:
          out_float = fsine;
          break;
        case 2:
          out_float = 0.0f;
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
      int quadrant    = i / (ksized4 / 2);
      float fph       = fmod(2.0f * float(i) / ksize, 1.0f);
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
          out_float = sqrtf(fph);
          break;
        case 1:
          out_float = sqrtf(1.0f - fph);
          break;
        case 2:
          out_float = -sqrtf(fph);
          break;
        case 3:
          out_float = -sqrtf(1.0f - fph);
          break;
        default:
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
      int quadrant    = i / (ksized4 / 2);
      float fph       = fmod(4.0f * float(i) / ksize, 1.0f);
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
          out_float = sinf(fph);
          break;
        case 1:
          out_float = sinf(fph);
          break;
        case 2:
          out_float = sinf(fph);
          break;
        case 3:
          out_float = sinf(fph);
          break;
        default:
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
      int quadrant    = i / (ksized4 / 2);
      float fph       = fmod(2.0f * float(i) / ksize, 1.0f);
      auto& out_float = wave->_wavedata[i];
      switch (quadrant) {
        case 0:
          out_float = sqrtf(fph);
          break;
        case 1:
          out_float = sqrtf(1.0f - fph);
          break;
        case 2:
          out_float = sqrtf(fph);
          break;
        case 3:
          out_float = sqrtf(1.0f - fph);
          break;
        default:
          out_float = 0.0f;
          break;
      }
    }
    _wavemap["tx81z.8"] = wave;
  }
}

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
