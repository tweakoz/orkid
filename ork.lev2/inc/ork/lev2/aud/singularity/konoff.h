////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
