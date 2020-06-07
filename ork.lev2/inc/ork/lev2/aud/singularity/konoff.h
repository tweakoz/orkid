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
  Layer* _layer                 = nullptr;
  lyrdata_constptr_t _layerdata = nullptr;
  ork::svarp_t _extdata;
};

} // namespace ork::audio::singularity
