#pragma once

#include <ork/kernel/svariant.h>

namespace ork::audio::singularity {

struct DspBlock;
struct LayerData;
struct layer;

///////////////////////////////////////////////////////////////////////////////

struct KeyOnInfo
{
    int _key = 0;
    int _vel = 0;
    layer* _layer = nullptr;
    lyrdata_constptr_t _LayerData = nullptr;
    ork::svarp_t _extdata;
};

///////////////////////////////////////////////////////////////////////////////

struct DspKeyOnInfo : public KeyOnInfo
{
    DspBlock* _prv = nullptr;
};

} // namespace ork::audio::singularity {
