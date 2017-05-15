#pragma once

#include <vector>

struct Wavetable
{
    Wavetable(int isize=0);
    ~Wavetable();

    float sample(float fi) const;
    float sampleLerp(float fi) const;

    std::vector<float> _wavedata;
};

const Wavetable* builtinWaveTable(const std::string& name);
