////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
constexpr float kmaxclip = 16.0f;  // 18dB
constexpr float kminclip = -16.0f; // 18dB

struct panLR {
  float lmix, rmix;
};

inline panLR panBlend(float inp) { // inp = -1 .. +1 (0==center)
  panLR rval;
  // constant power (quarter-sine): lmix^2+rmix^2 == 1 at all positions.
  // same law as PANNER/PANNER2D/PANNER2DU::compute - keep in sync.
  float pos = 0.5f + inp * 0.5f;
  rval.lmix = cosf(pos * PI * 0.5f);
  rval.rmix = sinf(pos * PI * 0.5f);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
