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
  // todo constant power...
  rval.lmix = (inp > 0) //
                  ? lerp(0.5, 0, inp)
                  : lerp(0.5, 1, -inp);
  rval.rmix = (inp > 0) //
                  ? lerp(0.5, 1, inp)
                  : lerp(0.5, 0, -inp);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
