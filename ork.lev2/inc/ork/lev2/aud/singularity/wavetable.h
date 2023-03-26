////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
