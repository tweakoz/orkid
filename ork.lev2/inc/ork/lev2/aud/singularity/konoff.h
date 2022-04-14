////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/svariant.h>

namespace ork::audio::singularity {

struct DspBlock;
struct LayerData;
struct layer;

using layer_ptr_t = std::shared_ptr<Layer>;

///////////////////////////////////////////////////////////////////////////////

struct KeyOnInfo {
  int _key                      = 0;
  int _vel                      = 0;
  layer_ptr_t _layer                 = nullptr;
  lyrdata_constptr_t _layerdata = nullptr;
  ork::svarp_t _extdata;
};

} // namespace ork::audio::singularity
