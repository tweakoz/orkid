#pragma once

#include <mutex>
#include "envelope.h"
#include "konoff.h"

namespace ork::audio::singularity {
static const int koscopelength     = 1 << 12;
static const int koscopelengthmask = koscopelength - 1;

struct lfoframe {
  int _index           = 0;
  float _value         = 0.0f;
  float _currate       = 1.0f;
  const LfoData* _data = nullptr;
};
struct funframe {
  int _index           = 0;
  float _value         = 0.0f;
  float _a             = 0.0f;
  float _b             = 0.0f;
  const FunData* _data = nullptr;
};
struct op4frame {
  float _envout = 0.0f;
  int _envph    = 0;
  float _mi     = 0.0f;
  float _r      = 0.0f;
  float _f      = 0.0f;
  int _ar       = 0;
  int _d1r      = 0;
  int _d1l      = 0;
  int _d2r      = 0;
  int _rr       = 0;
  int _egshift  = 0;
  int _wav      = 0;
  int _olev     = 0;
};
struct HudFrameAudio {
  HudFrameAudio()
      : _oscopebuffer(koscopelength) {
  }
  std::vector<float> _oscopebuffer;
  std::vector<bool> _oscopesync;

  std::vector<ork::svar256_t> _items;

  op4frame _op4frame[4];
  size_t _trackoffset = 0;
  size_t _baseserial  = 0;
  size_t _owcount     = 0;
};
struct HudFrameControl {
  lyrdata_constptr_t _layerdata;
  alg_ptr_t _alg;
  const kmregion* _kmregion = nullptr;
  int _note                 = 0;
  int _vel                  = 0;
  int _layerIndex           = -1;
  bool _useFm4              = false;

  std::string _miscText;
};
} // namespace ork::audio::singularity
