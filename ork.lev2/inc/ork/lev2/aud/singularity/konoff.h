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

struct KeyOnModifiers{
  using fvec4_genfn_t = std::function<fvec4()>;
  using fvec4_subfn_t = std::function<void(std::string name, svar64_t)>;
  using strvect_t = std::vector<std::string>;
  struct DATA{
    std::string _name;
    fvec4_genfn_t _generator;
    fvec4_subfn_t _subscriber;
    fvec4 _currentValue;
    varmap::VarMap _vars;
    LockedResource<strvect_t> _evstrings;
  };
  using data_ptr_t = std::shared_ptr<DATA>;
  using map_t = std::unordered_map<std::string,data_ptr_t>;
  map_t _mods;
  uint32_t _layermask = 0xffffffff;
  bool _dangling = false;
  outbus_ptr_t _outbus_override = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct KeyOnInfo {
  int _key                      = 0;
  int _vel                      = 0;
  layer_ptr_t _layer                 = nullptr;
  lyrdata_constptr_t _layerdata = nullptr;
  ork::svarp_t _extdata;
};

} // namespace ork::audio::singularity
