////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
