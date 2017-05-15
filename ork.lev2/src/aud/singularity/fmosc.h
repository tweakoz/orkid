#pragma once

#include <stdint.h>
#include <vector>
#include "tx81z.h"
#include <stdint.h>

struct DspKeyOnInfo;
struct layer;
struct Wavetable;

///////////////////////////////////////////////////////////

struct FmOsc
{
    FmOsc();
    ~FmOsc();

    void keyOn(const DspKeyOnInfo& koi,const Fm4OpData& opd);
    void keyOff();
    float compute(float frq, float df);

    void setWave(int iw);

    static constexpr float kinv64k = 1.0f/65536.0f;
    static constexpr float kinv32k = 1.0f/32768.0f;

    float _baseFrequency;
    float _modIndex;

    int64_t _pbIndex;
    int64_t _pbIndexNext;
    int64_t _pbIncrBase;
    float _prevOutput;

    const Wavetable* _waveform;
};
