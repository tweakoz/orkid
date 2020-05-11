#pragma once

#include <ork/kernel/svariant.h>

namespace ork::audio::singularity {

struct DspBlock;
struct LayerData;
struct layer;

///////////////////////////////////////////////////////////////////////////////

struct KeyOnInfo {
  int _key                      = 0;
  int _vel                      = 0;
  layer* _layer                 = nullptr;
  lyrdata_constptr_t _LayerData = nullptr;
  ork::svarp_t _extdata;
};

///////////////////////////////////////////////////////////////////////////////

struct DspKeyOnInfo : public KeyOnInfo {
  dspblk_ptr_t _prv;
};

} // namespace ork::audio::singularity
