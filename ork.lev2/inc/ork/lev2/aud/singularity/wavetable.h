////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

namespace ork::audio::singularity {

struct Wavetable
{
    Wavetable(int isize=0);
    ~Wavetable();

    float sample(float fi) const;
    float sampleLerp(float fi) const;

    std::vector<float> _wavedata;
};

const Wavetable* builtinWaveTable(const std::string& name);

} // namespace ork::audio::singularity {
