////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <vector>
#include <stdint.h>

namespace ork::audio::singularity {

struct DspKeyOnInfo;
struct layer;
struct Wavetable;

///////////////////////////////////////////////////////////
// Yamaha FM operator
//  technically, its phase modulation...
///////////////////////////////////////////////////////////

struct PmOscData {
  int _waveform = 0;
};

struct PmOsc {
  PmOsc();
  ~PmOsc();

  void keyOn(const PmOscData& opd);
  void keyOff();
  float compute(float frq, float phase_offset);

  void setWave(int iw);

  int64_t _pbIndex;
  int64_t _pbIndexNext;
  int64_t _pbIncrBase;
  float _prevOutput;
  float _wtsXisr;
  int64_t _wtsize;
  

  const Wavetable* _waveform;
};

} // namespace ork::audio::singularity
